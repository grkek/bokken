#include "SpriteBatcher.hpp"

namespace Bokken
{
    namespace Renderer
    {

        namespace
        {
            // GLSL 330 core. Stays simple — just a 2D pass-through with an
            // ortho projection done CPU-side, so no MVP uniform needed beyond
            // the projection matrix. Color comes per-vertex; texture is
            // sampled and modulated by it.
            const char *kVS = R"(#version 330 core
                layout(location = 0) in vec2 a_pos;
                layout(location = 1) in vec2 a_uv;
                layout(location = 2) in vec4 a_color;
                uniform mat4 u_proj;
                out vec2 v_uv;
                out vec4 v_color;
                void main() {
                    v_uv = a_uv;
                    v_color = a_color;
                    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
                }
                )";

            const char *kFS = R"(#version 330 core
                in vec2 v_uv;
                in vec4 v_color;
                uniform sampler2D u_tex;
                uniform int u_isAlphaMask;  // 1 for R8 glyph atlas, 0 for RGBA
                out vec4 FragColor;
                void main() {
                    if (u_isAlphaMask == 1) {
                        // R8 atlas: red channel is coverage; tint by v_color.
                        float a = texture(u_tex, v_uv).r;
                        FragColor = vec4(v_color.rgb, v_color.a * a);
                    } else {
                        FragColor = texture(u_tex, v_uv) * v_color;
                    }
                }
                )";

            // Build column-major 4x4 ortho. Maps:
            //   x: [0, w]   →  [-1, +1]
            //   y: [0, h]   →  [+1, -1]   (flip — screen-space top-left origin)
            //   z: [-1, 1]  →  [-1, +1]
            void orthoTopLeft(float out[16], float w, float h)
            {
                std::memset(out, 0, sizeof(float) * 16);
                out[0] = 2.0f / w;
                out[5] = -2.0f / h;
                out[10] = -1.0f;
                out[12] = -1.0f;
                out[13] = 1.0f;
                out[15] = 1.0f;
            }
        }

        SpriteBatcher::~SpriteBatcher()
        {
            // GLEW guarantees these entry points are non-null post-load,
            // so the old `&& glDeleteXxx` guards triggered
            // -Wpointer-bool-conversion. Keep the handle check, drop the
            // pointer check.
            if (m_vao) glDeleteVertexArrays(1, &m_vao);
            if (m_vbo) glDeleteBuffers(1, &m_vbo);
            if (m_ibo) glDeleteBuffers(1, &m_ibo);
        }

        bool SpriteBatcher::init()
        {
            if (!m_shader.fromSource(kVS, kFS, "sprite"))
                return false;

            // 1x1 white texture for solid-color quads.
            const uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            if (!m_whiteTex.uploadFull(1, 1, TextureFormat::RGBA8, white,
                                       TextureFilter::Nearest, TextureWrap::Clamp))
                return false;

            glGenVertexArrays(1, &m_vao);
            glGenBuffers(1, &m_vbo);
            glGenBuffers(1, &m_ibo);

            glBindVertexArray(m_vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            // Reserve a healthy starting size; we'll grow on demand.
            glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * 1024 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

            // Vertex layout: 2 (pos) + 2 (uv) + 4 (color) = 8 floats.
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)offsetof(Vertex, x));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)offsetof(Vertex, u));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)offsetof(Vertex, r));

            glBindVertexArray(0);
            return true;
        }

        void SpriteBatcher::begin(int viewportW, int viewportH)
        {
            m_viewportW = viewportW;
            m_viewportH = viewportH;
            m_quads.clear();
            m_verts.clear();
            m_indices.clear();
            m_stats = {};
        }

        void SpriteBatcher::drawTextured(const Texture2D *tex,
                                         float x, float y, float w, float h,
                                         float u0, float v0, float u1, float v1,
                                         uint32_t rgba, int layer)
        {
            Quad q{x, y, w, h, u0, v0, u1, v1, rgba, tex, layer};
            m_quads.push_back(q);
        }

        void SpriteBatcher::drawRect(float x, float y, float w, float h,
                                     uint32_t rgba, int layer)
        {
            Quad q{x, y, w, h, 0.f, 0.f, 1.f, 1.f, rgba, nullptr, layer};
            m_quads.push_back(q);
        }

        void SpriteBatcher::issueBatch(const Texture2D *tex, size_t firstQuad, size_t count)
        {
            if (count == 0)
                return;

            // Build vertex/index data for this batch.
            m_verts.clear();
            m_indices.clear();
            m_verts.reserve(count * 4);
            m_indices.reserve(count * 6);

            for (size_t i = 0; i < count; ++i)
            {
                const Quad &q = m_quads[firstQuad + i];
                const float r = ((q.rgba >> 24) & 0xFF) / 255.0f;
                const float g = ((q.rgba >> 16) & 0xFF) / 255.0f;
                const float b = ((q.rgba >> 8) & 0xFF) / 255.0f;
                const float a = ((q.rgba >> 0) & 0xFF) / 255.0f;

                const uint32_t base = static_cast<uint32_t>(m_verts.size());
                m_verts.push_back({q.x, q.y, q.u0, q.v0, r, g, b, a});
                m_verts.push_back({q.x + q.w, q.y, q.u1, q.v0, r, g, b, a});
                m_verts.push_back({q.x + q.w, q.y + q.h, q.u1, q.v1, r, g, b, a});
                m_verts.push_back({q.x, q.y + q.h, q.u0, q.v1, r, g, b, a});

                m_indices.push_back(base + 0);
                m_indices.push_back(base + 1);
                m_indices.push_back(base + 2);
                m_indices.push_back(base + 0);
                m_indices.push_back(base + 2);
                m_indices.push_back(base + 3);
            }

            glBindVertexArray(m_vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            // glBufferData with NULL data first hints the driver to orphan the
            // previous buffer rather than stall on it. Then re-upload.
            glBufferData(GL_ARRAY_BUFFER, m_verts.size() * sizeof(Vertex),
                         nullptr, GL_DYNAMIC_DRAW);
            glBufferData(GL_ARRAY_BUFFER, m_verts.size() * sizeof(Vertex),
                         m_verts.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t),
                         nullptr, GL_DYNAMIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t),
                         m_indices.data(), GL_DYNAMIC_DRAW);

            // Bind the texture for this batch. R8 textures are alpha masks
            // (used by the glyph atlas) so the shader handles them differently.
            const Texture2D *binding = tex ? tex : &m_whiteTex;
            binding->bind(0);
            const bool isAlphaMask = (binding->format() == TextureFormat::R8);
            m_shader.setInt("u_tex", 0);
            m_shader.setInt("u_isAlphaMask", isAlphaMask ? 1 : 0);

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                           GL_UNSIGNED_INT, nullptr);

            m_stats.drawCallCount++;
            m_stats.textureBindCount++;
        }

        void SpriteBatcher::flush()
        {
            if (m_quads.empty())
                return;

            // Stable sort by (layer, texture). Stable so quads at the same
            // (layer, texture) keep their submission order — this matters
            // for overlapping translucent things like glyph kerning.
            std::stable_sort(m_quads.begin(), m_quads.end(),
                             [](const Quad &a, const Quad &b)
                             {
                                 if (a.layer != b.layer)
                                     return a.layer < b.layer;
                                 return a.texture < b.texture;
                             });

            m_stats.quadCount = static_cast<int>(m_quads.size());

            // Set up GL state for 2D blending.
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            m_shader.bind();

            // Projection matches the viewport. Caller (Renderer) sets the
            // GL viewport before calling flush(); we just compute matching
            // ortho bounds from what we were told in begin().
            float proj[16];
            orthoTopLeft(proj, static_cast<float>(m_viewportW),
                         static_cast<float>(m_viewportH));
            m_shader.setMat4("u_proj", proj);

            // Walk sorted quads, batching consecutive runs by (layer, texture).
            size_t i = 0;
            while (i < m_quads.size())
            {
                size_t j = i + 1;
                while (j < m_quads.size() && m_quads[j].layer == m_quads[i].layer && m_quads[j].texture == m_quads[i].texture)
                {
                    ++j;
                }
                issueBatch(m_quads[i].texture, i, j - i);
                i = j;
            }

            // Don't clear m_quads here — caller may want to re-flush in the
            // same frame (e.g. for ping-pong post). They call begin() next frame.
            m_quads.clear();
        }

    }
}
