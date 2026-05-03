#pragma once

#include "Texture2D.hpp"

#include <SDL3/SDL.h>

#include <utility>

namespace Bokken
{
    namespace Renderer
    {

        /**
         * Off-screen render target. One color texture + optional depth.
         *
         * Used by the Pipeline so each Stage can read the previous stage's
         * output as a texture and write to its own. Most stages use HDR
         * (RGBA16F) so bloom and tone-mapping can preserve highlights.
         */
        class RenderTarget
        {
        public:
            RenderTarget() = default;
            ~RenderTarget();
            RenderTarget(const RenderTarget &) = delete;
            RenderTarget &operator=(const RenderTarget &) = delete;
            RenderTarget(RenderTarget &&) noexcept;
            RenderTarget &operator=(RenderTarget &&) noexcept;

            bool create(int width, int height,
                        TextureFormat colorFormat = TextureFormat::RGBA16F,
                        bool withDepth = false);

            /** Recreate at a new size. Cheap if size is unchanged. */
            bool resize(int width, int height);

            void bind() const;
            static void bindDefault();

            Texture2D &color() { return m_color; }
            const Texture2D &color() const { return m_color; }

            bool isValid() const { return m_fbo != 0; }
            int width() const { return m_width; }
            int height() const { return m_height; }

        private:
            GLuint m_fbo = 0;
            GLuint m_depthRBO = 0;
            Texture2D m_color;
            int m_width = 0;
            int m_height = 0;
            TextureFormat m_colorFormat = TextureFormat::RGBA16F;
            bool m_hasDepth = false;
        };

    }
}
