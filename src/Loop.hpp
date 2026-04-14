#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include "Configuration.hpp"
#include "./scripting/Engine.hpp"

namespace Bokken
{

    /**
     * Drives the main game loop using SDL3.
     *
     * Responsibilities:
     *   - Initialise SDL3 (video + events).
     *   - Create and configure the OS window from ProjectConfiguration.
     Scripting::*   - Own the Engine and forward its module registrations.
     *   - Run the SDL event loop with:
     *       • Variable-timestep  onUpdate(dt)        — every rendered frame.
     *       • Fixed-timestep     onFixedUpdate(dt)   — at a configurable Hz.
     *       • One-shot           onStart()           — before the first frame.
     *   - Handle SDL_QUIT and expose a clean shutdown path.
     *
     * Usage:
     *   Loop loop;
     *   loop.init(config, "development");
     *   loop.scripting().addModule(std::make_unique<LogModule>());
     *   loop.loadBytecode(data, len, "index.script");
     *   loop.run();   // blocks until the window is closed
     */
    class Loop
    {
    public:
        Loop() = default;
        ~Loop() { shutdown(); }

        // Non-copyable.
        Loop(const Loop &) = delete;
        Loop &operator=(const Loop &) = delete;

        /**
         * Initialises SDL, creates the window, and sets up the scripting engine.
         * @param config      Parsed ProjectConfiguration (from project.bokken).
         * @param environment "development" or "production".
         * @param fixedHz     Fixed-update frequency in Hz. Default: 60 (16.67ms steps).
         * @return true on success.
         */
        bool init(const ProjectConfiguration &config,
                  const std::string &environment = "development",
                  int fixedHz = 60,
                  AssetPack *assets = nullptr);

        /**
         * Convenience overload: load bytecode into the scripting engine.
         * Must be called after init() and after all modules have been registered.
         */
        bool loadBytecode(const uint8_t *data, size_t len, const std::string &name);

        /**
         * Access the scripting engine to register native modules.
         * Call addModule() on the returned reference before loadBytecode().
         */
        Scripting::Engine &scripting() { return m_scripting; }

        /**
         * Enter the SDL event loop. Blocks until the window is closed or
         * shutdown() is called from within a JS hook (via a native binding).
         */
        void run();

        /**
         * Request a clean shutdown. Safe to call from any thread.
         * The loop will exit after the current frame completes.
         */
        void requestQuit() { m_quit = true; }

        /** Returns the SDL window handle (useful for renderer modules). */
        SDL_Window *window() const { return m_window; }

    private:
        SDL_Window *m_window = nullptr;
        Scripting::Engine m_scripting;

        bool m_quit = false;
        bool m_initialised = false;

        // Timing
        double m_fixedStep = 0.02; // seconds per fixed-update tick (1/fixedHz)
        double m_fixedAccum = 0.0; // accumulated time since last fixed tick
        Uint64 m_lastTick = 0;     // SDL_GetTicks64() at previous frame

        // Cap the maximum dt fed into onUpdate to avoid spiral-of-death on hitches.
        static constexpr double k_maxDeltaTime = 0.25; // 260 ms

        void processEvents();
        void tick();
        void shutdown();

        // Parse a CSS-style hex color string ("#RRGGBB") into 0–255 components.
        static bool parseClearColor(const std::string &hex,
                                    uint8_t &r, uint8_t &g, uint8_t &b);
    };

} // namespace Bokken
