#include "pch.h"
#include "app_console.h"

#include <cstdio>

static bool g_appConsoleEnabled = true;

#ifdef _WIN32
static bool
ConsoleBelongsOnlyToThisProcess()
{
    DWORD processList[2] = {};
    const DWORD processCount = GetConsoleProcessList(processList, 2);
    return processCount == 1;
}

static void
RedirectStandardStreamsToConsole()
{
    FILE* file = nullptr;
    (void)freopen_s(&file, "CONOUT$", "w", stdout);
    (void)freopen_s(&file, "CONOUT$", "w", stderr);
    (void)freopen_s(&file, "CONIN$", "r", stdin);
    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
}
#endif

void
SetAppConsoleEnabled(bool enabled)
{
    g_appConsoleEnabled = enabled;

#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (enabled) {
        if (consoleWindow == nullptr) {
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                (void)AllocConsole();
            }
            RedirectStandardStreamsToConsole();
            consoleWindow = GetConsoleWindow();
        }
        if (consoleWindow != nullptr) {
            ShowWindow(consoleWindow, SW_SHOW);
        }
    } else if (consoleWindow != nullptr && ConsoleBelongsOnlyToThisProcess()) {
        ShowWindow(consoleWindow, SW_HIDE);
    }
#endif
}

bool
IsAppConsoleEnabled()
{
    return g_appConsoleEnabled;
}
