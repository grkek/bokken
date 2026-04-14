#pragma once

#include <string>
#include <vector>
#include <map>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Bokken {

    struct Author {
        std::map<std::string, std::string> person;
        // organization is empty in your sample, but we'll map it
        std::map<std::string, std::string> organization;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Author, person, organization)
    };

    struct General {
        std::string displayTitle;
        std::string internalSlug;
        Author author;
        std::string projectVersion;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(General, displayTitle, internalSlug, author, projectVersion)
    };

    struct WindowSettings {
        int width;
        int height;
        std::string clearColor;
        bool isFullscreen = false; // Overrides
        bool useVsync = false;     // Overrides

        // Note: Map only what is in windowBase; overrides come from environments
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(WindowSettings, width, height, clearColor)
    };

    struct ScriptingEngine {
        struct Runtime {
            int maxHeapSizeMb;
            int stackSizeKb;
            int gcThresholdKb;
            int interruptHandlerMs;
        } runtime;

        struct Features {
            bool enableBytecodeCache;
            bool enableProxy;
        } features;

        struct Interop {
            bool printToStandardOut;
            std::string verbosity;
        } interop;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Runtime, maxHeapSizeMb, stackSizeKb, gcThresholdKb, interruptHandlerMs)
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Features, enableBytecodeCache, enableProxy)
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Interop, printToStandardOut, verbosity)
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ScriptingEngine, runtime, features, interop)
    };

    struct EnvironmentConfiguration {
        struct WindowOverrides {
            bool isFullscreen;
            bool useVsync;
        } windowOverrides;

        ScriptingEngine scriptingEngine;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(WindowOverrides, isFullscreen, useVsync)
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(EnvironmentConfiguration, windowOverrides, scriptingEngine)
    };

    struct BuildMetadata {
        std::string engineVersion;
        std::string lastBuilt;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BuildMetadata, engineVersion, lastBuilt)
    };

    struct ProjectConfiguration {
        General general;
        WindowSettings windowBase;
        std::map<std::string, EnvironmentConfiguration> environments;
        BuildMetadata buildMetadata;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProjectConfiguration, general, windowBase, environments, buildMetadata)

        // Helper to get active config
        const EnvironmentConfiguration& get_environment(bool is_production) const {
            return environments.at(is_production ? "production" : "development");
        }
    };
}