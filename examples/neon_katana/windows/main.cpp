// Platform-specific overrides
#define BOKKEN_PROJECT_PATH "project.bokken"
#define BOKKEN_SCRIPT_PACK  "assets/scripts.assetpack"
#define BOKKEN_ENVIRONMENT  "development"
#define BOKKEN_FIXED_HZ     60

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../../src/Bokken.hpp"

int main(int argc, char* argv[]) {
    // Ensure terminal handles ANSI colors for logs
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    return Bokken::entryPoint(argc, argv);
}