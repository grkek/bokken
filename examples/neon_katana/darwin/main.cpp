/**
 * NeonKatana — macOS entry point.
 *
 * Just a thin .app-bundle aware wrapper around Bokken::entryPoint.
 * When the binary is launched from inside a .app bundle the working
 * directory is something arbitrary (often "/"), so we cd into the
 * bundle's Resources/ before handing control to the engine — that way
 * relative paths like "project.bokken" resolve correctly.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <climits>

#include <Bokken.hpp>

int main(int argc, char *argv[])
{
    char path[PATH_MAX];

    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)) {
        chdir(path);
    }
    CFRelease(resourcesURL);

    return Bokken::entryPoint(argc, argv);
}