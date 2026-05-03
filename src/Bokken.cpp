/**
 * Bokken.cpp
 *
 * Implementation of the public entry point declared in include/Bokken.hpp.
 *
 * This file used to be an inline body inside Bokken.hpp, which forced
 * every consumer to provide every transitive include path the body
 * touched (SDL, GLM, glad, PhysFS, QuickJS, nlohmann/json, ...). Moving
 * the body here makes the public surface a single function declaration
 * and consumers' build files collapse to one include directory + one
 * library link.
 */

#include "Bokken.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

#include "Configuration.hpp"
#include "Loop.hpp"
#include "AssetPack.hpp"

// Built-in scripting modules.
#include "scripting/modules/Audio.hpp"
#include "scripting/modules/Canvas.hpp"
#include "scripting/modules/GameObject.hpp"
#include "scripting/modules/Log.hpp"
#include "scripting/modules/Window.hpp"
#include "scripting/modules/Renderer.hpp"

// Compile-time defaults — see the doc-comment in Bokken.hpp for the
// override convention. These #ifndef guards stay in case someone passes
// -DBOKKEN_FIXED_HZ=120 (or similar) to the engine's CMake configure.
#ifndef BOKKEN_PROJECT_PATH
#define BOKKEN_PROJECT_PATH "project.bokken"
#endif
#ifndef BOKKEN_SCRIPT_PACK
#define BOKKEN_SCRIPT_PACK "assets/scripts.assetpack"
#endif
#ifndef BOKKEN_ENVIRONMENT
#define BOKKEN_ENVIRONMENT "development"
#endif
#ifndef BOKKEN_FIXED_HZ
#define BOKKEN_FIXED_HZ 60
#endif
#ifndef BOKKEN_ENTRY_SCRIPT
#define BOKKEN_ENTRY_SCRIPT "/scripts/index.script"
#endif

namespace Bokken
{
    // TU-local helper. Was previously inline-in-header; demoted to static
    // here since no other TU needs it.
    static std::string readTextFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f) return {};
        return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    }

    int entryPoint(int /*argc*/, char *argv[])
    {
        // PhysFS needs the real executable path so it can resolve relative paths.
        PHYSFS_init(argv[0]);

        // 1. Load and parse project configuration (plain JSON, not packed)
        std::string configText = readTextFile(BOKKEN_PROJECT_PATH);
        if (configText.empty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Could not read project file: %s\n", BOKKEN_PROJECT_PATH);
            PHYSFS_deinit();
            return EXIT_FAILURE;
        }

        ProjectConfiguration configuration;
        try {
            nlohmann::json j = nlohmann::json::parse(configText);
            configuration = j.get<ProjectConfiguration>();
        } catch (const std::exception &ex) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Failed to parse project configuration: %s\n", ex.what());
            PHYSFS_deinit();
            return EXIT_FAILURE;
        }

        // 2. Mount asset packs via PhysFS.
        //
        //    The AssetPack wrapper calls PHYSFS_init internally, but we already
        //    called it above with argv[0] so PhysFS's ref-count is now 2 —
        //    AssetPack's destructor will deinit once, and our PHYSFS_deinit()
        //    below will bring it back to 0.
        AssetPack assets;
        if (!assets.mount(BOKKEN_SCRIPT_PACK, "/")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Failed to mount scripts asset pack: %s\n", BOKKEN_SCRIPT_PACK);
            return EXIT_FAILURE;
        }
        if (!assets.mount("assets/fonts.assetpack", "/fonts")) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Failed to mount fonts asset pack\n");
            return EXIT_FAILURE;
        }

        // 3. Initialise the engine loop.
        Loop loop;
        if (!loop.init(configuration, BOKKEN_ENVIRONMENT, BOKKEN_FIXED_HZ, &assets))
            return EXIT_FAILURE;

        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::Audio>());
        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::Canvas>(loop.window(), &assets));
        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::GameObject>(loop.window(), &assets));
        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::Log>());
        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::Window>(loop.window()));
        loop.scriptingEngine().addModule(std::make_unique<Scripting::Modules::Renderer>(loop.renderer()));

        // 4. Entry script.
        if (!assets.exists(BOKKEN_ENTRY_SCRIPT)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Entry script not found in pack: %s\n", BOKKEN_ENTRY_SCRIPT);
            return EXIT_FAILURE;
        }

        SDL_IOStream *scriptIO = assets.openIOStream(BOKKEN_ENTRY_SCRIPT);
        if (!scriptIO) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Failed to open entry script stream\n");
            return EXIT_FAILURE;
        }

        size_t scriptLen = 0;
        void *scriptRaw = SDL_LoadFile_IO(scriptIO, &scriptLen, true);

        if (!scriptRaw || scriptLen == 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] SDL_LoadFile_IO failed: %s\n", SDL_GetError());
            return EXIT_FAILURE;
        }

        std::vector<uint8_t> scriptData(
            static_cast<uint8_t*>(scriptRaw),
            static_cast<uint8_t*>(scriptRaw) + scriptLen);

        SDL_free(scriptRaw);

        if (!loop.loadBytecode(scriptData.data(), scriptData.size(), BOKKEN_ENTRY_SCRIPT)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "[Bokken] Failed to load script bytecode.\n");
            return EXIT_FAILURE;
        }

        loop.run();

        PHYSFS_deinit();
        return EXIT_SUCCESS;
    }

} // namespace Bokken
