#include "Base.hpp"

namespace Bokken
{
    namespace Renderer
    {

        Base::~Base() { shutdown(); }

        bool Base::init(SDL_Window *window, AssetPack *assets)
        {
            if (!window)
                return false;
            m_window = window;
            (void)assets; // not used directly here; passed to GlyphCache on demand

            // Request a 3.3 Core context. macOS requires Core profile to get
            // anything > 2.1, so we ask for it explicitly.
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

            m_glContext = SDL_GL_CreateContext(window);
            if (!m_glContext)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[Renderer] SDL_GL_CreateContext failed: %s", SDL_GetError());
                return false;
            }
            SDL_GL_MakeCurrent(window, m_glContext);
            SDL_GL_SetSwapInterval(1); // vsync; can be flipped at runtime later

            // glad's loader takes a callback with signature
            //   void* (const char *)
            // SDL3's SDL_GL_GetProcAddress returns an SDL_FunctionPointer
            // (a typed function-pointer alias), not void*. They're
            // ABI-compatible — SDL_FunctionPointer is just a typed
            // function pointer — but the C++ type system needs the
            // explicit reinterpret_cast.
            const int version = gladLoadGL(
                reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));

            if (version == 0)
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[GL] gladLoadGL failed — no GL context current?");
                return false;
            }

            const int major = GLAD_VERSION_MAJOR(version);
            const int minor = GLAD_VERSION_MINOR(version);
            if (major < 3 || (major == 3 && minor < 3))
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                             "[GL] need GL 3.3 core, got %d.%d", major, minor);
                return false;
            }

            // Establish current size before we build resources sized to it.
            updateSize();

            if (!m_batcher.init())
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[Renderer] SpriteBatcher init failed");
                return false;
            }
            if (!m_glyphs.init())
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[Renderer] GlyphCache init failed");
                return false;
            }

            // Default pipeline: scene → composite. JS code can add bloom etc.
            buildDefaultPipeline();

            if (!m_pipeline.init(m_physicalW, m_physicalH))
            {
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[Renderer] Pipeline init failed");
                return false;
            }

            return true;
        }

        void Base::shutdown()
        {
            if (m_glContext)
            {
                SDL_GL_DestroyContext(m_glContext);
                m_glContext = nullptr;
            }
        }

        void Base::buildDefaultPipeline()
        {
            m_pipeline.addStage(std::make_unique<SpriteStage>("scene"));
            m_pipeline.addStage(std::make_unique<CompositeStage>("composite"));
        }

        void Base::updateSize()
        {
            if (!m_window)
                return;
            int lw, lh, pw, ph;
            SDL_GetWindowSize(m_window, &lw, &lh);
            SDL_GetWindowSizeInPixels(m_window, &pw, &ph);
            m_logicalW = lw;
            m_logicalH = lh;
            m_physicalW = pw;
            m_physicalH = ph;
        }

        void Base::beginFrame()
        {
            // Pick up window resizes / DPI changes.
            int oldW = m_physicalW, oldH = m_physicalH;
            updateSize();
            if (m_physicalW != oldW || m_physicalH != oldH)
            {
                m_pipeline.resize(m_physicalW, m_physicalH);
            }
            m_batcher.begin(m_physicalW, m_physicalH);
        }

        void Base::endFrame(float dt)
        {
            // Run pipeline. Each stage writes into the rotating ping-pong
            // targets; final output is in pipeline.finalOutput().
            m_pipeline.render(&m_batcher, dt);

            // Composite to the default framebuffer (the actual window).
            const RenderTarget *final = m_pipeline.finalOutput();
            if (final)
            {
                // Bind default FBO, viewport at physical pixel size.
                RenderTarget::bindDefault();
                glViewport(0, 0, m_physicalW, m_physicalH);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);

                // Use the composite stage's pass machinery — but we don't
                // own one here. Quick path: draw a fullscreen quad via the
                // batcher pretending the final target is a sprite. Cheap.
                // (We'd otherwise duplicate the FullscreenPass logic.)
                m_batcher.begin(m_physicalW, m_physicalH);
                // Draw a screen-sized quad with the final color texture as
                // its source. UV is flipped vertically because GL textures
                // origin is bottom-left, but we render top-left.
                // Note: SpriteBatcher's UVs are passed straight to the
                // shader, which samples y "as is" — the fullscreen-triangle
                // post stages were already y-up; here we want the texture
                // displayed without flipping, so we use (0,0)→(1,1) and let
                // the projection's vertical flip in begin() handle screen
                // orientation.
                m_batcher.drawTextured(&final->color(),
                                       0, 0,
                                       (float)m_physicalW, (float)m_physicalH,
                                       0.0f, 1.0f, 1.0f, 0.0f,
                                       0xFFFFFFFFu, 0);
                m_batcher.flush();
            }

            SDL_GL_SwapWindow(m_window);
        }

    }
}
