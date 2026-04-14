// Platform-specific overrides
#define BOKKEN_PROJECT_PATH "project.bokken"
#define BOKKEN_SCRIPT_PACK  "assets/scripts.assetpack"
#define BOKKEN_ENVIRONMENT  "development"
#define BOKKEN_FIXED_HZ     60

#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>
#include "../../../src/Bokken.hpp"

int main(int argc, char* argv[]) {
    // Ensures the working directory is the executable's directory
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    
    if (count != -1) {
        path[count] = '\0';
        chdir(dirname(path));
    }

    return Bokken::entryPoint(argc, argv);
}