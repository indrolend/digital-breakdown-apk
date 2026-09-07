#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "DesktopApp.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    char executable[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, executable, MAX_PATH);
    if(length == 0 || length >= MAX_PATH) return 1;

    char* argv[] = { executable, nullptr };
    return runDigitalBreakdown(1, argv);
}