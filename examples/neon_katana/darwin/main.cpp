// Platform-specific overrides
#define BOKKEN_PROJECT_PATH "project.bokken"
#define BOKKEN_SCRIPT_PACK  "assets/scripts.assetpack"
#define BOKKEN_ENVIRONMENT  "development"
#define BOKKEN_FIXED_HZ     60

#include <CoreFoundation/CoreFoundation.h>
#include "../../../src/Bokken.hpp"

int main(int argc, char* argv[]) {
    // Fixes the working directory if launched from an .app bundle
    char path[PATH_MAX];

    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)) {
        chdir(path);
    }
    CFRelease(resourcesURL);

    return Bokken::entryPoint(argc, argv);
}