#pragma once

#include "Configuration.hpp"
#include "AssetPack.hpp"
#include "./renderer/Base.hpp"
#include "./scripting/modules/Canvas.hpp"
#include "./scripting/Engine.hpp"
#include "./scripting/modules/GameObject.hpp"
#include "./scripting/modules/Input.hpp"
#include "./game_object/Animation2D.hpp"
#include "./game_object/Distortion2D.hpp"
#include "./canvas/components/Label.hpp"
#include "./renderer/stages/SpriteStage.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <string>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <charconv>

namespace Bokken
{

    /**
     * Drives the main game loop using SDL3.
     *
     * Post-GL refactor:
     *   - Window is created with SDL_WINDOW_OPENGL (mandatory).
     *   - SDL_Renderer is gone — replaced by our Renderer (owns GL ctx,
     *     SpriteBatcher, GlyphCache, Pipeline).
     *   - tick() calls renderer.beginFrame()/endFrame() instead of
     *     SDL_RenderClear / SDL_RenderPresent.
     *
     * Usage is unchanged from the caller's perspective:
     *   Loop loop;
     *   loop.init(config, "development", 60, &assets);
     *   loop.loadBytecode(data, len, "index.script");
     *   loop.run();
     */
    class Loop
    {
    public:
        Loop() = default;
        ~Loop() { shutdown(); }

        Loop(const Loop &) = delete;
        Loop &operator=(const Loop &) = delete;

        bool init(const ProjectConfiguration &config,
                  const std::string &environment = "development",
                  int fixedHz = 60,
                  AssetPack *assets = nullptr);

        bool loadBytecode(const uint8_t *data, size_t len, const std::string &name);

        Scripting::Engine &scriptingEngine() { return Scripting::Engine::Instance(); }

        void run();
        void requestQuit() { m_quit = true; }

        SDL_Window *window() const { return m_window; }
        Renderer::Base *renderer() { return m_renderer.get(); }

    private:
        SDL_Window *m_window = nullptr;
        std::unique_ptr<Renderer::Base> m_renderer;
        AssetPack *m_assets = nullptr;

        bool m_quit = false;
        bool m_initialised = false;

        // Timing.
        double m_fixedStep = 0.02;
        double m_fixedAccum = 0.0;
        Uint64 m_lastTick = 0;

        static constexpr double k_maxDeltaTime = 0.25;

        // Cached clear color from configuration (linear sRGB-ish, 0..1).
        float m_clearR = 0.075f, m_clearG = 0.090f, m_clearB = 0.105f;

        void processEvents();
        void tick();
        void shutdown();

        static bool parseClearColor(const std::string &hex,
                                    uint8_t &r, uint8_t &g, uint8_t &b);
    };

} // namespace Bokken