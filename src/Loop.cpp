#include "Loop.hpp"

namespace Bokken
{

    bool Loop::init(const ProjectConfiguration &configuration,
                    const std::string &environment,
                    int fixedHz,
                    AssetPack *assets)
    {
        m_assets = assets;

        // Resolve environment overrides.
        const EnvironmentConfiguration *environmentConfiguration = nullptr;
        try {
            environmentConfiguration = &configuration.get_environment(environment == "production");
        } catch (...) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] Unknown environment '%s', defaulting to development\n",
                         environment.c_str());
            environmentConfiguration = &configuration.get_environment(false);
        }

        const WindowSettings &window = configuration.windowBase;
        const auto &windowOverrides = environmentConfiguration->windowOverrides;
        const auto &scriptingEngineConfiguration = environmentConfiguration->scriptingEngine;

        // SDL3 init — video + events. Audio comes via the audio module.
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

        if (!TTF_Init()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s", SDL_GetError());
            SDL_Quit();
            return false;
        }

        // Window flags. SDL_WINDOW_OPENGL is mandatory now — the GL
        // renderer needs a GL-capable surface to attach to.
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL;

        if (windowOverrides.isFullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;
        if (windowOverrides.isBorderlessFullscreen)
            flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
        if (windowOverrides.alwaysOnTop)
            flags |= SDL_WINDOW_ALWAYS_ON_TOP;
        if (windowOverrides.transparent)
            flags |= SDL_WINDOW_TRANSPARENT;

        m_window = SDL_CreateWindow(
            configuration.general.displayTitle.c_str(),
            window.width, window.height,
            flags);

        if (!m_window) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] SDL_CreateWindow failed: %s\n", SDL_GetError());
            TTF_Quit();
            SDL_Quit();
            return false;
        }
        SDL_SetWindowMinimumSize(m_window, 640, 480);

        // Cache clear color in 0..1 floats. SpriteStage will pick this
        // up below.
        uint8_t cr = 19, cg = 23, cb = 27;
        if (parseClearColor(window.clearColor, cr, cg, cb)) {
            m_clearR = cr / 255.0f;
            m_clearG = cg / 255.0f;
            m_clearB = cb / 255.0f;
        }

        // Create our renderer — this is where the GL context is born.
        m_renderer = std::make_unique<Renderer::Base>();
        if (!m_renderer->init(m_window, m_assets)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] Renderer::init failed");
            SDL_DestroyWindow(m_window);
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        // Apply configured clear color to the default scene stage.
        if (auto *st = dynamic_cast<Renderer::SpriteStage *>(
                m_renderer->pipeline().findStage("scene"))) {
            st->clearR = m_clearR;
            st->clearG = m_clearG;
            st->clearB = m_clearB;
            st->clearA = 1.0f;
        }

        // Wire the renderer into the Canvas pieces that need it.
        Scripting::Modules::Canvas::setBatcher(&m_renderer->batcher());
        Scripting::Modules::GameObject::setBatcher(&m_renderer->batcher());
        Canvas::Components::Label::s_glyphCache = &m_renderer->glyphs();

        // Scripting engine.
        if (!this->scriptingEngine().init(assets,
                scriptingEngineConfiguration.runtime.maxHeapSizeMb,
                scriptingEngineConfiguration.runtime.stackSizeKb,
                scriptingEngineConfiguration.runtime.gcThresholdKb)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] ScriptingEngine::init() failed\n");
            m_renderer.reset();
            SDL_DestroyWindow(m_window);
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        // Timing.
        m_fixedStep = (fixedHz > 0) ? (1.0 / fixedHz) : 0.02;
        m_fixedAccum = 0.0;
        m_lastTick = SDL_GetTicksNS();

        m_initialised = true;
        m_quit = false;

        fprintf(stdout, "[Bokken] Engine initialised — %s v%s (%s)\n",
                configuration.general.displayTitle.c_str(),
                configuration.general.projectVersion.c_str(),
                environment.c_str());
        return true;
    }

    bool Loop::loadBytecode(const uint8_t *data, size_t len, const std::string &name)
    {
        if (!m_initialised) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] loadBytecode() called before init()\n");
            return false;
        }
        return this->scriptingEngine().loadBytecode(data, len, name);
    }

    void Loop::run()
    {
        auto &engine = this->scriptingEngine();
        if (!m_initialised) return;
        if (!engine.isReady()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "[Bokken] ScriptingEngine is not ready!\n");
            return;
        }

        engine.callOnStart();
        m_lastTick = SDL_GetTicksNS();

        while (!m_quit) {
            processEvents();
            if (m_quit) break;
            tick();
        }
    }

    void Loop::processEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            Scripting::Modules::Canvas::handleEvent(e);
            Scripting::Modules::Input::handleEvent(e); 
            switch (e.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    m_quit = true;
                    break;
                default: break;
            }
        }
    }

    void Loop::tick()
    {
        auto &engine = this->scriptingEngine();

        // dt
        Uint64 now = SDL_GetTicksNS();
        double dt = static_cast<double>(now - m_lastTick) * 1e-9;
        m_lastTick = now;
        if (dt > k_maxDeltaTime) dt = k_maxDeltaTime;

        // Fixed steps.
        m_fixedAccum += dt;
        while (m_fixedAccum >= m_fixedStep) {
            engine.callOnFixedUpdate(m_fixedStep);
            Scripting::Modules::GameObject::fixedUpdate((float)m_fixedStep);
            m_fixedAccum -= m_fixedStep;
        }

        // Variable updates.
        engine.callOnUpdate(dt);
        Scripting::Modules::GameObject::update((float)dt);
        Scripting::Modules::Canvas::update((float)dt);

        // Render.
        m_renderer->beginFrame();

        // Modules submit draws into m_renderer->batcher() during these calls.
        Scripting::Modules::GameObject::present();
        Scripting::Modules::Canvas::present();

        m_renderer->endFrame((float)dt);

        // Tiny yield so we don't spin in the rare case vsync is off.
        SDL_Delay(0);

        // Clear transient input state after the frame.
        Scripting::Modules::Input::endFrame();
    }

    void Loop::shutdown()
    {
        if (!m_initialised) return;
        auto &engine = this->scriptingEngine();
        engine.shutdown();

        // Drop renderer BEFORE the window — it owns the GL context bound to it.
        m_renderer.reset();

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        Scripting::Modules::Canvas::clear_font_cache();
        Canvas::Components::Label::clear_font_cache();

        TTF_Quit();
        SDL_Quit();
        m_initialised = false;

        fprintf(stdout, "[Bokken] Engine shutdown complete.\n");
    }

    bool Loop::parseClearColor(const std::string &hex,
                                uint8_t &r, uint8_t &g, uint8_t &b)
    {
        if (hex.size() != 7 || hex[0] != '#') return false;
        auto hexByte = [&](size_t pos, uint8_t &out) -> bool {
            uint8_t val = 0;
            auto [ptr, ec] = std::from_chars(hex.data() + pos,
                                              hex.data() + pos + 2, val, 16);
            if (ec != std::errc()) return false;
            out = val;
            return true;
        };
        return hexByte(1, r) && hexByte(3, g) && hexByte(5, b);
    }

} // namespace Bokken
