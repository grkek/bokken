#pragma once

/**
 * Bokken.hpp
 *
 * Shared bootstrap logic included by every platform main.cpp.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>

#include <nlohmann/json.hpp>

#include "Configuration.hpp"
#include "Loop.hpp"
#include "AssetPack.hpp"

// Built-in modules
#include "scripting/modules/Log.hpp"
#include "scripting/modules/Canvas.hpp"
#include "scripting/modules/Window.hpp"

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

// Virtual path inside the scripts pack where the entry bytecode lives.
// BokkenPacker always writes compiled JS as "scripts/<n>.script".
#ifndef BOKKEN_ENTRY_SCRIPT
#define BOKKEN_ENTRY_SCRIPT "/scripts/index.script"
#endif

namespace Bokken
{

    // Helpers

    // Read a real-filesystem file into a string (used only for project.bokken).
    inline std::string readTextFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f)
            return {};
        return {std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>()};
    }

    // Entry point
    inline int entryPoint(int /*argc*/, char *argv[])
    {

        // PhysFS needs the real executable path so it can resolve relative paths.
        PHYSFS_init(argv[0]);

        // 1. Load and parse project configuration (plain JSON, not packed)
        std::string configText = readTextFile(BOKKEN_PROJECT_PATH);
        if (configText.empty())
        {
            fprintf(stderr, "[Bokken] Could not read project file: %s\n",
                    BOKKEN_PROJECT_PATH);
            PHYSFS_deinit();
            return EXIT_FAILURE;
        }

        ProjectConfiguration configuration;
        try
        {
            nlohmann::json j = nlohmann::json::parse(configText);
            configuration = j.get<ProjectConfiguration>();
        }
        catch (const std::exception &ex)
        {
            fprintf(stderr, "[Bokken] Failed to parse project configuration: %s\n", ex.what());
            PHYSFS_deinit();
            return EXIT_FAILURE;
        }

        // 2. Mount asset packs via PhysFS
        //
        //    The AssetPack wrapper calls PHYSFS_init internally, but we already
        //    called it above with argv[0] so PhysFS's ref-count is now 2 —
        //    AssetPack's destructor will deinit once, and our PHYSFS_deinit()
        //    below will bring it back to 0.
        AssetPack assets;

        // Scripts asset pack
        if (!assets.mount(BOKKEN_SCRIPT_PACK, "/"))
        {
            fprintf(stderr, "[Bokken] Failed to mount scripts asset pack: %s\n",
                    BOKKEN_SCRIPT_PACK);
            return EXIT_FAILURE;
        }

        // Fonts asset pack
        if (!assets.mount("assets/fonts.assetpack", "/fonts"))
        {
            fprintf(stderr, "[Bokken] Failed to mount fonts asset pack: %s\n",
                    BOKKEN_SCRIPT_PACK);
            return EXIT_FAILURE;
        }

        // 3. Initialise the engine loop
        Loop loop;
        if (!loop.init(configuration, BOKKEN_ENVIRONMENT, BOKKEN_FIXED_HZ, &assets))
        {
            return EXIT_FAILURE;
        }

        // 4. Register native modules
        loop.scripting().addModule(std::make_unique<Modules::Log>());
        loop.scripting().addModule(std::make_unique<Modules::Canvas>(loop.window(), loop.renderer(), &assets));
        loop.scripting().addModule(std::make_unique<Modules::Window>(loop.window()));

        // 5. Load script bytecode via SDL_IOStream
        //
        //    We open the compiled entry script from the mounted PhysFS VFS
        //    using an SDL_IOStream so the scripting engine never touches the
        //    real filesystem directly — all I/O goes through the pack layer.
        if (!assets.exists(BOKKEN_ENTRY_SCRIPT))
        {
            fprintf(stderr, "[Bokken] Entry script not found in pack: %s\n",
                    BOKKEN_ENTRY_SCRIPT);
            return EXIT_FAILURE;
        }

        // Open the virtual file as an SDL_IOStream via the PhysFS bridge.
        SDL_IOStream *scriptIO = assets.openIOStream(BOKKEN_ENTRY_SCRIPT);
        if (!scriptIO)
        {
            fprintf(stderr, "[Bokken] Failed to open entry script stream: %s\n",
                    BOKKEN_ENTRY_SCRIPT);
            return EXIT_FAILURE;
        }

        // SDL_LoadFile_IO reads the entire stream into a malloc'd buffer.
        // closeio=true means SDL closes the stream for us when done.
        size_t scriptLen = 0;
        void *scriptRaw = SDL_LoadFile_IO(scriptIO, &scriptLen, true);
        if (!scriptRaw || scriptLen == 0)
        {
            fprintf(stderr, "[Bokken] SDL_LoadFile_IO failed for entry script: %s\n",
                    SDL_GetError());
            return EXIT_FAILURE;
        }

        // Copy into a vector for clean ownership, then free the SDL buffer.
        std::vector<uint8_t> scriptData(
            static_cast<uint8_t *>(scriptRaw),
            static_cast<uint8_t *>(scriptRaw) + scriptLen);
        SDL_free(scriptRaw);

        if (!loop.loadBytecode(scriptData.data(), scriptData.size(),
                               BOKKEN_ENTRY_SCRIPT))
        {
            fprintf(stderr, "[Bokken] Failed to load script bytecode.\n");
            return EXIT_FAILURE;
        }

        // 6. Run — blocks until the window is closed
        loop.run();

        // assets and loop destruct here; PhysFS and SDL are cleaned up
        // by their respective destructors / PHYSFS_deinit below.
        PHYSFS_deinit();
        return EXIT_SUCCESS;
    }

} // namespace Bokken
