#include "Loop.hpp"

namespace Bokken
{

    // Init
    bool Loop::init(const ProjectConfiguration &configuration,
                    const std::string &environment,
                    int fixedHz,
                    AssetPack *assets)
    {
        // Retrieve the environment-specific overrides.
        const EnvironmentConfiguration *environmentConfiguration = nullptr;
        try
        {
            environmentConfiguration = &configuration.get_environment(environment == "production");
        }
        catch (...)
        {
            fprintf(stderr, "[Bokken] Unknown environment '%s', defaulting to development\n",
                    environment.c_str());
            environmentConfiguration = &configuration.get_environment(false);
        }

        const WindowSettings &window = configuration.windowBase;
        const auto &windowOVerrides = environmentConfiguration->windowOverrides;
        const auto &scriptingEngineConfiguration = environmentConfiguration->scriptingEngine;

        // SDL3 init
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            fprintf(stderr, "[Bokken] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

        // SDL3 TTF init
        if (!TTF_Init())
        {
            SDL_Log("TTF_Init Failed: %s", SDL_GetError());
            return -1;
        }

        // Window
        SDL_WindowFlags flags = 0;
        if (windowOVerrides.isFullscreen)
            flags |= SDL_WINDOW_FULLSCREEN;

        m_window = SDL_CreateWindow(
            configuration.general.displayTitle.c_str(),
            window.width, window.height,
            flags);

        if (!m_window)
        {
            fprintf(stderr, "[Bokken] SDL_CreateWindow failed: %s\n", SDL_GetError());
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        m_renderer = SDL_CreateRenderer(m_window, NULL); // NULL picks the default driver (Metal/DirectX/Vulkan)
        if (!m_renderer)
        {
            fprintf(stderr, "[Bokken] SDL_CreateRenderer failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(m_window);
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        SDL_SetRenderVSync(m_renderer, 1);
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        // Optional: apply clear colour hint via SDL surface (before a renderer is
        // attached). Modules that set up their own renderer (OpenGL, Vulkan, etc.)
        // should use their own clear-colour mechanism; this is just a placeholder.
        uint8_t cr = 0, cg = 0, cb = 0;
        if (parseClearColor(window.clearColor, cr, cg, cb))
        {
            // Store for use by a future renderer module. Nothing to do here yet.
            (void)cr;
            (void)cg;
            (void)cb;
        }

        // Scripting engine
        if (!m_scripting.init(assets, scriptingEngineConfiguration.runtime.maxHeapSizeMb,
                              scriptingEngineConfiguration.runtime.stackSizeKb,
                              scriptingEngineConfiguration.runtime.gcThresholdKb))
        {
            fprintf(stderr, "[Bokken] ScriptingEngine::init() failed\n");
            SDL_DestroyWindow(m_window);
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        // Timing
        m_fixedStep = (fixedHz > 0) ? (1.0 / fixedHz) : 0.02;
        m_fixedAccum = 0.0;
        m_lastTick = SDL_GetTicksNS(); // nanosecond precision

        m_initialised = true;
        m_quit = false;

        fprintf(stdout, "[Bokken] Engine initialised — %s v%s (%s)\n",
                configuration.general.displayTitle.c_str(),
                configuration.general.projectVersion.c_str(),
                environment.c_str());

        return true;
    }

    bool Loop::loadBytecode(const uint8_t *data, size_t len,
                            const std::string &name)
    {
        if (!m_initialised)
        {
            fprintf(stderr, "[Bokken] loadBytecode() called before init()\n");
            return false;
        }
        return m_scripting.loadBytecode(data, len, name);
    }
    // Main loop
    void Loop::run()
    {
        if (!m_initialised)
        {
            fprintf(stderr, "[Bokken] run() called before init()\n");
            return;
        }
        if (!m_scripting.isReady())
        {
            fprintf(stderr, "[Bokken] ScriptingEngine is not ready — did you call loadBytecode()?\n");
            return;
        }

        // onStart
        m_scripting.callOnStart();
        m_lastTick = SDL_GetTicksNS(); // reset after potentially slow onStart()

        // Main loop
        while (!m_quit)
        {
            processEvents();
            if (m_quit)
                break;
            tick();
        }
    }
    // Per-frame work
    void Loop::processEvents()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            Bokken::Modules::Canvas::handle_event(e);

            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                m_quit = true;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_quit = true;
                break;

            // Future: route input events to an InputModule here.
            default:
                break;
            }
        }
    }

    void Loop::tick()
    {
        // Delta time
        Uint64 now = SDL_GetTicksNS();
        double dt = static_cast<double>(now - m_lastTick) * 1e-9; // ns → s
        m_lastTick = now;

        // Guard against enormous spikes (e.g. debugger pause, OS scheduling).
        if (dt > k_maxDeltaTime)
            dt = k_maxDeltaTime;

        // Fixed update
        m_fixedAccum += dt;
        while (m_fixedAccum >= m_fixedStep)
        {
            m_scripting.callOnFixedUpdate(m_fixedStep);
            m_fixedAccum -= m_fixedStep;
        }

        // Variable update
        m_scripting.callOnUpdate(dt);

        Bokken::Modules::Canvas::update(static_cast<float>(dt));

        SDL_SetRenderDrawColor(m_renderer, 227, 115, 131, 255); // Watermelon pink
        SDL_RenderClear(m_renderer);

        Bokken::Modules::Canvas::present();

        SDL_RenderPresent(m_renderer);

        // Yield to OS
        // SDL_Delay(0) releases the timeslice briefly; avoids burning 100% CPU when
        // there is no GPU vsync limiting the loop. Renderer modules should replace
        // this with their own swap / present call.
        SDL_Delay(0);
    }

    // Shutdown
    void Loop::shutdown()
    {
        if (!m_initialised)
            return;

        m_scripting.shutdown();

        if (m_window)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        Bokken::Modules::Canvas::clear_font_cache();

        if (m_renderer)
        {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }

        TTF_Quit();
        SDL_Quit();
        m_initialised = false;

        fprintf(stdout, "[Bokken] Engine shutdown complete.\n");
    }

    // Helpers
    bool Loop::parseClearColor(const std::string &hex,
                               uint8_t &r, uint8_t &g, uint8_t &b)
    {
        // Accepts "#RRGGBB"
        if (hex.size() != 7 || hex[0] != '#')
            return false;

        auto hexByte = [&](size_t pos, uint8_t &out) -> bool
        {
            uint8_t val = 0;
            auto [ptr, ec] = std::from_chars(hex.data() + pos,
                                             hex.data() + pos + 2,
                                             val, 16);
            if (ec != std::errc())
                return false;
            out = val;
            return true;
        };

        return hexByte(1, r) && hexByte(3, g) && hexByte(5, b);
    }

} // namespace Bokken
