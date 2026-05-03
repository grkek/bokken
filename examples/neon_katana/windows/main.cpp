/**
 * NeonKatana — Windows entry point.
 *
 * Just a thin wrapper around Bokken::entryPoint that flips the console's
 * virtual-terminal mode on so ANSI colour codes from the engine's logs
 * render correctly in cmd.exe / Windows Terminal.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Bokken.hpp>

int main(int argc, char *argv[])
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;

    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    return Bokken::entryPoint(argc, argv);
}