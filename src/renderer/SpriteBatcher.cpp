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

            // Initial per-segment capacity. Grows on demand if a frame
            // pushes more quads than this. 4096 quads = 16384 verts +
            // 24576 indices, roughly 640KB total across all 3 segments.
            // Comfortable for most 2D scenes; particle-heavy frames may
            // trigger one resize early on and then stay stable.
            static constexpr size_t k_initialQuadCapacity = 4096;
        }

        SpriteBatcher::~SpriteBatcher()
        {
            if (m_vao)
                glDeleteVertexArrays(1, &m_vao);
            if (m_vbo)
                glDeleteBuffers(1, &m_vbo);
            if (m_ibo)
                glDeleteBuffers(1, &m_ibo);
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

            // Allocate the triple-buffered VBO and IBO.
            m_vboCapacityVerts = k_initialQuadCapacity * 4;
            m_iboCapacityInds = k_initialQuadCapacity * 6;
            m_vboSegmentSize = m_vboCapacityVerts * sizeof(Vertex);
            m_iboSegmentSize = m_iboCapacityInds * sizeof(uint32_t);

            glBindVertexArray(m_vao);

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m_vboSegmentSize * k_bufferSegments),
                         nullptr, GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m_iboSegmentSize * k_bufferSegments),
                         nullptr, GL_DYNAMIC_DRAW);

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

            m_bufferSegment = 0;
            return true;
        }

        void SpriteBatcher::ensureBufferCapacity(size_t totalVerts, size_t totalIndices)
        {
            if (totalVerts <= m_vboCapacityVerts && totalIndices <= m_iboCapacityInds)
                return;

            // Double until we fit.
            while (m_vboCapacityVerts < totalVerts)
                m_vboCapacityVerts *= 2;
            while (m_iboCapacityInds < totalIndices)
                m_iboCapacityInds *= 2;

            m_vboSegmentSize = m_vboCapacityVerts * sizeof(Vertex);
            m_iboSegmentSize = m_iboCapacityInds * sizeof(uint32_t);

            // Reallocate the full triple-buffer. This orphans the old
            // allocation — any in-flight GPU reads from previous frames
            // continue on the old memory, so no sync is needed.
            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m_vboSegmentSize * k_bufferSegments),
                         nullptr, GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(m_iboSegmentSize * k_bufferSegments),
                         nullptr, GL_DYNAMIC_DRAW);
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
                                         uint32_t rgba, int layer,
                                         BlendMode blend)
        {
            Quad q{x, y, w, h, u0, v0, u1, v1, rgba, tex, layer, 0.0f, blend};
            m_quads.push_back(q);
        }

        void SpriteBatcher::drawRect(float x, float y, float w, float h,
                                     uint32_t rgba, int layer,
                                     BlendMode blend)
        {
            Quad q{x, y, w, h, 0.f, 0.f, 1.f, 1.f, rgba, nullptr, layer, 0.0f, blend};
            m_quads.push_back(q);
        }

        void SpriteBatcher::drawRotatedRect(float cx, float cy, float w, float h,
                                            float rotationRad, uint32_t rgba, int layer,
                                            BlendMode blend)
        {
            float x = cx - w * 0.5f;
            float y = cy - h * 0.5f;
            Quad q{x, y, w, h, 0.f, 0.f, 1.f, 1.f, rgba, nullptr, layer, rotationRad, blend};
            m_quads.push_back(q);
        }

        void SpriteBatcher::applyBlendMode(BlendMode mode)
        {
            switch (mode)
            {
            case BlendMode::Alpha:
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;

            case BlendMode::Additive:
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE,
                                    GL_ONE, GL_ONE);
                break;

            case BlendMode::Screen:
                // Screen: 1 - (1-src)*(1-dst) = src + dst - src*dst
                // GL approximation: src*(1) + dst*(1-src) per channel.
                glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR,
                                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }
        }

        void SpriteBatcher::issueBatch(const Texture2D *tex, BlendMode blend,
                                       size_t firstQuad, size_t count)
        {
            if (count == 0)
                return;

            // Build vertex/index data for this batch into the shared
            // CPU-side vectors. We append rather than clear because all
            // batches in a frame share the same VBO segment.
            size_t vertBase = m_verts.size();

            for (size_t i = 0; i < count; ++i)
            {
                const Quad &q = m_quads[firstQuad + i];
                const float r = ((q.rgba >> 24) & 0xFF) / 255.0f;
                const float g = ((q.rgba >> 16) & 0xFF) / 255.0f;
                const float b = ((q.rgba >> 8) & 0xFF) / 255.0f;
                const float a = ((q.rgba >> 0) & 0xFF) / 255.0f;

                // Four corners (top-left origin, y down).
                float x0 = q.x, y0 = q.y;
                float x1 = q.x + q.w, y1 = q.y;
                float x2 = q.x + q.w, y2 = q.y + q.h;
                float x3 = q.x, y3 = q.y + q.h;

                // Apply rotation around center if nonzero.
                if (q.rotation != 0.0f)
                {
                    float cx = q.x + q.w * 0.5f;
                    float cy = q.y + q.h * 0.5f;
                    float cosR = std::cos(q.rotation);
                    float sinR = std::sin(q.rotation);

                    auto rotate = [cx, cy, cosR, sinR](float &px, float &py)
                    {
                        float dx = px - cx;
                        float dy = py - cy;
                        px = cx + dx * cosR - dy * sinR;
                        py = cy + dx * sinR + dy * cosR;
                    };

                    rotate(x0, y0);
                    rotate(x1, y1);
                    rotate(x2, y2);
                    rotate(x3, y3);
                }

                const uint32_t base = static_cast<uint32_t>(m_verts.size());
                m_verts.push_back({x0, y0, q.u0, q.v0, r, g, b, a});
                m_verts.push_back({x1, y1, q.u1, q.v0, r, g, b, a});
                m_verts.push_back({x2, y2, q.u1, q.v1, r, g, b, a});
                m_verts.push_back({x3, y3, q.u0, q.v1, r, g, b, a});

                m_indices.push_back(base + 0);
                m_indices.push_back(base + 1);
                m_indices.push_back(base + 2);
                m_indices.push_back(base + 0);
                m_indices.push_back(base + 2);
                m_indices.push_back(base + 3);
            }

            m_stats.drawCallCount++;
            m_stats.textureBindCount++;
        }

        void SpriteBatcher::flush()
        {
            if (m_quads.empty())
                return;

            // Stable sort by (layer, blend mode, texture). Stable so quads
            // at the same key keep their submission order — matters for
            // overlapping translucent things like glyph kerning. Blend mode
            // sorts before texture because blend state changes are more
            // expensive than texture binds.
            std::stable_sort(m_quads.begin(), m_quads.end(),
                             [](const Quad &a, const Quad &b)
                             {
                                 if (a.layer != b.layer)
                                     return a.layer < b.layer;
                                 if (a.blend != b.blend)
                                     return a.blend < b.blend;
                                 return a.texture < b.texture;
                             });

            m_stats.quadCount = static_cast<int>(m_quads.size());

            // Build all vertex/index data for the entire frame into
            // m_verts / m_indices by walking batches.
            m_verts.clear();
            m_indices.clear();
            m_verts.reserve(m_quads.size() * 4);
            m_indices.reserve(m_quads.size() * 6);

            // Record batch boundaries: each entry is (firstQuad, count,
            // texture, blend). We build all geometry first, upload once,
            // then issue draws — one upload per frame instead of one per
            // batch.
            struct BatchRecord
            {
                size_t firstVert;
                size_t firstIndex;
                size_t indexCount;
                const Texture2D *texture;
                BlendMode blend;
            };
            std::vector<BatchRecord> batches;

            {
                size_t i = 0;
                while (i < m_quads.size())
                {
                    size_t j = i + 1;
                    while (j < m_quads.size()
                           && m_quads[j].layer == m_quads[i].layer
                           && m_quads[j].blend == m_quads[i].blend
                           && m_quads[j].texture == m_quads[i].texture)
                    {
                        ++j;
                    }

                    size_t prevVerts = m_verts.size();
                    size_t prevInds = m_indices.size();

                    issueBatch(m_quads[i].texture, m_quads[i].blend, i, j - i);

                    batches.push_back({
                        prevVerts,
                        prevInds,
                        m_indices.size() - prevInds,
                        m_quads[i].texture,
                        m_quads[i].blend
                    });

                    i = j;
                }
            }

            if (m_verts.empty())
            {
                m_quads.clear();
                return;
            }

            // Ensure the triple-buffer is large enough for this frame's data.
            ensureBufferCapacity(m_verts.size(), m_indices.size());

            // Upload the entire frame's geometry into this frame's segment
            // of the triple-buffer with a single glBufferSubData per buffer.
            size_t vboOffset = m_bufferSegment * m_vboSegmentSize;
            size_t iboOffset = m_bufferSegment * m_iboSegmentSize;

            glBindVertexArray(m_vao);

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferSubData(GL_ARRAY_BUFFER,
                            static_cast<GLintptr>(vboOffset),
                            static_cast<GLsizeiptr>(m_verts.size() * sizeof(Vertex)),
                            m_verts.data());

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                            static_cast<GLintptr>(iboOffset),
                            static_cast<GLsizeiptr>(m_indices.size() * sizeof(uint32_t)),
                            m_indices.data());

            // Set up GL state.
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);

            m_shader.bind();

            float proj[16];
            orthoTopLeft(proj, static_cast<float>(m_viewportW),
                         static_cast<float>(m_viewportH));
            m_shader.setMat4("u_proj", proj);

            // Issue draw calls for each batch. Geometry is already in the
            // VBO/IBO — we just set the texture, blend mode, and draw with
            // the right offsets.
            BlendMode currentBlend = BlendMode::Alpha;
            applyBlendMode(currentBlend);

            // The vertex attribute pointers were set up relative to the
            // start of the VBO during init(). For triple-buffering we need
            // to offset them to the current segment. We do this by rebinding
            // the VBO with the base offset baked into the attribute pointers.
            size_t vertByteBase = vboOffset;

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)(vertByteBase + offsetof(Vertex, x)));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)(vertByteBase + offsetof(Vertex, u)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  (const void *)(vertByteBase + offsetof(Vertex, r)));

            for (const auto &batch : batches)
            {
                if (batch.blend != currentBlend)
                {
                    applyBlendMode(batch.blend);
                    currentBlend = batch.blend;
                    m_stats.blendSwitchCount++;
                }

                const Texture2D *binding = batch.texture ? batch.texture : &m_whiteTex;
                binding->bind(0);
                const bool isAlphaMask = (binding->format() == TextureFormat::R8);
                m_shader.setInt("u_tex", 0);
                m_shader.setInt("u_isAlphaMask", isAlphaMask ? 1 : 0);

                // Index offset into the IBO for this batch, accounting for
                // the current triple-buffer segment.
                size_t indexByteOffset = iboOffset + batch.firstIndex * sizeof(uint32_t);

                glDrawElements(GL_TRIANGLES,
                               static_cast<GLsizei>(batch.indexCount),
                               GL_UNSIGNED_INT,
                               (const void *)indexByteOffset);
            }

            // Restore default blend mode for any non-batcher GL code that
            // runs between frames (e.g. the composite pass).
            if (currentBlend != BlendMode::Alpha)
                applyBlendMode(BlendMode::Alpha);

            // Rotate to the next triple-buffer segment for next frame.
            m_bufferSegment = (m_bufferSegment + 1) % k_bufferSegments;

            m_quads.clear();
        }

    }
}