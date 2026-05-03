#pragma once

#include "../Shader.hpp"
#include "../gl/GL.hpp"

namespace Bokken
{
    namespace Renderer
    {

        /**
         * Helper for post-process stages. Owns a fullscreen triangle VAO
         * and runs a fragment shader over it sampling some input texture.
         *
         * Why a fullscreen *triangle* instead of two triangles for a quad:
         * a single triangle that overshoots the viewport saves the GPU's
         * triangle setup pipeline a tiny bit and (more practically) avoids
         * a hairline seam down the middle of the screen on some drivers.
         * Standard trick, costs nothing.
         */
        class FullscreenPass
        {
        public:
            ~FullscreenPass();

            bool init();

            /** Run the given fragment shader (vertex shader is built-in).
             *  The user binds whatever input textures they need beforehand
             *  and sets uniforms on `shader` between bind() and draw(). */
            Shader &beginPass(const char *fragmentSrc, const char *debugName);
            void draw();

        private:
            Shader m_shader;
            GLuint m_vao = 0;
            bool m_built = false;
            std::string m_currentSrc;
        };

    }
}
