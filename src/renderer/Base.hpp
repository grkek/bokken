#pragma once

#include "Pipeline.hpp"
#include "SpriteBatcher.hpp"
#include "GlyphCache.hpp"
#include "AssetPack.hpp"
#include "stages/SpriteStage.hpp"
#include "stages/CompositeStage.hpp"

// GLEW first (via gl/GL.hpp); SDL after. GLEW requires being included
// before any system GL headers, which SDL3 may pull in indirectly on
// some platforms.
#include "gl/GL.hpp"
#include <SDL3/SDL.h>

#include <memory>

namespace Bokken
{
    namespace Renderer
    {

        /**
         * The renderer.
         *
         * Owns:
         *   - the SDL_GLContext
         *   - the SpriteBatcher (shared, used by every 2D stage)
         *   - the GlyphCache (text rasterization → atlas)
         *   - the Pipeline (ordered stages with ping-pong targets)
         *
         * Lifecycle:
         *   Renderer r;
         *   r.init(window, assets);     // creates GL context + default pipeline
         *   loop:
         *     r.beginFrame();
         *     r.batcher().drawRect(...);
         *     r.endFrame(dt);            // executes pipeline + composites
         *   r.shutdown();
         *
         * The default pipeline configured by init() is just a SpriteStage
         * followed by a CompositeStage. Anything fancier (bloom, color
         * grading, CRT) is added by the user via the bokken/renderer JS API.
         */
        class Base
        {
        public:
            Base() = default;
            ~Base();

            bool init(SDL_Window *window, class Bokken::AssetPack *assets);
            void shutdown();

            /** Begin a frame. Resizes pipeline targets if the window changed. */
            void beginFrame();

            /** Execute the pipeline, then composite the final output to screen. */
            void endFrame(float dt);

            SpriteBatcher &batcher() { return m_batcher; }
            GlyphCache &glyphs() { return m_glyphs; }
            Pipeline &pipeline() { return m_pipeline; }
            SDL_Window *window() const { return m_window; }

            /** Logical (pre-DPI) window size — useful for layout. */
            int logicalWidth() const { return m_logicalW; }
            int logicalHeight() const { return m_logicalH; }
            /** Physical (post-DPI) framebuffer size — what GL draws into. */
            int physicalWidth() const { return m_physicalW; }
            int physicalHeight() const { return m_physicalH; }
            float dpiScale() const { return m_logicalW > 0 ? (float)m_physicalW / (float)m_logicalW : 1.0f; }

        private:
            SDL_Window *m_window = nullptr;
            SDL_GLContext m_glContext = nullptr;

            SpriteBatcher m_batcher;
            GlyphCache m_glyphs;
            Pipeline m_pipeline;

            int m_logicalW = 0, m_logicalH = 0;
            int m_physicalW = 0, m_physicalH = 0;

            void buildDefaultPipeline();
            void updateSize();
        };

    }
}
