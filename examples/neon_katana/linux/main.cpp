/**
 * NeonKatana — Linux entry point.
 *
 * Just a thin wrapper around Bokken::entryPoint that ensures the working
 * directory is the executable's directory, so relative paths like
 * "project.bokken" resolve correctly regardless of where the user
 * launched the binary from.
 */

#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

#include <Bokken.hpp>

int main(int argc, char *argv[])
{
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);

    if (count != -1) {
        path[count] = '\0';
        chdir(dirname(path));
    }

    return Bokken::entryPoint(argc, argv);
}