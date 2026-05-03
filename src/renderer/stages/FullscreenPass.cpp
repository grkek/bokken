#include "FullscreenPass.hpp"

namespace Bokken
{
    namespace Renderer
    {

        namespace
        {
            // Fullscreen triangle in NDC. gl_VertexID lookup avoids needing
            // any vertex buffer — the GPU computes positions from the index.
            const char *kVS = R"(#version 330 core
                out vec2 v_uv;
                void main() {
                    // Generate a fullscreen triangle from gl_VertexID.
                    //   id=0 -> (-1,-1)  id=1 -> (3,-1)  id=2 -> (-1,3)
                    vec2 pos = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                                    (gl_VertexID == 2) ? 3.0 : -1.0);
                    v_uv = (pos + 1.0) * 0.5;
                    gl_Position = vec4(pos, 0.0, 1.0);
                }
                )";
        }

        FullscreenPass::~FullscreenPass()
        {
            if (m_vao)
                glDeleteVertexArrays(1, &m_vao);
        }

        bool FullscreenPass::init()
        {
            glGenVertexArrays(1, &m_vao);
            return m_vao != 0;
        }

        Shader &FullscreenPass::beginPass(const char *fragmentSrc, const char *debugName)
        {
            // Recompile only when the FS source changes (it shouldn't, but
            // this keeps the hot reload path open if we want it later).
            if (!m_built || m_currentSrc != fragmentSrc)
            {
                m_shader.fromSource(kVS, fragmentSrc, debugName);
                m_currentSrc = fragmentSrc;
                m_built = true;
            }
            m_shader.bind();
            return m_shader;
        }

        void FullscreenPass::draw()
        {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

    }
}
