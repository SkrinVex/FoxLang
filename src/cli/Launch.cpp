#include "Launch.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#endif

namespace foxlang::cli {
LaunchOptions parseLaunch(const std::vector<std::string>& arguments) {
    LaunchOptions result;
    bool literal = false;
    for (size_t i = 0; i < arguments.size(); ++i) {
        auto arg = arguments[i];
        if (!literal && arg.rfind("--skrumi-", 0) == 0) arg = "--foxlang-" + arg.substr(9);
        if (!literal && arg == "--") { literal = true; continue; }
        if (literal || arg.rfind("--foxlang-", 0) != 0) {
            result.interpreter.arguments.push_back(arg);
            continue;
        }
        auto dimension = [&]() {
            if (++i == arguments.size()) throw std::runtime_error(arg + " requires a size in pixels");
            const auto& value = arguments[i];
            if (value.empty() || value.size() > 4 || value.find_first_not_of("0123456789") != std::string::npos)
                throw std::runtime_error(arg + " must be an integer in 64..4096");
            int n = std::stoi(value);
            if (n < 64 || n > 4096) throw std::runtime_error(arg + " must be in 64..4096");
            return n;
        };
        if (arg == "--foxlang-console") result.console = ConsoleMode::Console;
        else if (arg == "--foxlang-no-console") result.console = ConsoleMode::Hidden;
        else if (arg == "--foxlang-width") result.interpreter.windowWidth = dimension();
        else if (arg == "--foxlang-height") result.interpreter.windowHeight = dimension();
        else if (arg == "--foxlang-fullscreen") result.interpreter.windowFullscreen = true;
        else if (arg == "--foxlang-windowed") result.interpreter.windowFullscreen = false;
        else throw std::runtime_error("unknown runtime option '" + arg + "'");
    }
    if (int64_t(result.interpreter.windowWidth) * result.interpreter.windowHeight > 8388608)
        throw std::runtime_error("window size must be at most 8388608 pixels");
    return result;
}

#ifdef _WIN32
static bool usable(FILE* stream) {
    int descriptor = _fileno(stream);
    if (descriptor < 0) return false;
    auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(descriptor));
    return handle && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
}
#endif
void configureConsole(ConsoleMode mode) {
#ifdef _WIN32
    if (mode == ConsoleMode::Hidden) { FreeConsole(); return; }
    if (mode != ConsoleMode::Console) return;
    if (!GetConsoleCP() && !AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole())
        throw std::runtime_error("cannot attach or create a console");
    // Preserve inherited pipes and files (including redirected stdout/stderr).
    FILE* stream = nullptr;
    if (!usable(stdin)) freopen_s(&stream, "CONIN$", "r", stdin);
    if (!usable(stdout)) freopen_s(&stream, "CONOUT$", "w", stdout);
    if (!usable(stderr)) freopen_s(&stream, "CONOUT$", "w", stderr);
    std::cin.clear(); std::cout.clear(); std::cerr.clear();
#else
    (void)mode; // Unix never creates an extra terminal; preserve the caller's streams.
#endif
}
void reportError(const std::string& message) {
#ifdef _WIN32
    if (!usable(stderr)) {
        int size = MultiByteToWideChar(CP_UTF8, 0, message.data(), static_cast<int>(message.size()), nullptr, 0);
        std::wstring text(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, message.data(), static_cast<int>(message.size()), text.data(), size);
        MessageBoxW(nullptr, text.c_str(), L"Skrumi", MB_OK | MB_ICONERROR);
        return;
    }
#endif
    std::cerr << "Skrumi: " << message << std::endl;
}
} // namespace foxlang::cli
