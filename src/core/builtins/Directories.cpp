#include "Builtin.h"
#include "foxlang/Platform.h"
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif
namespace foxlang::runtime {
namespace {
namespace fs = std::filesystem;
std::string directory(const std::string& kind) {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
    (void)kind;
    throw std::runtime_error("System Error: desktop user directories are unavailable on this platform");
#else
#ifdef _WIN32
    const KNOWNFOLDERID* id = &FOLDERID_LocalAppData;
    if (kind == "home") id = &FOLDERID_Profile;
    else if (kind != "data" && kind != "config" && kind != "cache" && kind != "state")
        throw std::runtime_error("System Error: unknown user directory kind");
    PWSTR path = nullptr;
    HRESULT result = SHGetKnownFolderPath(*id, 0, nullptr, &path);
    if (FAILED(result)) throw std::runtime_error("System Error: SHGetKnownFolderPath " + std::to_string(result));
    fs::path value(path); CoTaskMemFree(path);
#else
    std::string home = platform::getEnvVar("HOME");
    if (home.empty() || !platform::pathFromUtf8(home).is_absolute()) {
        auto* entry = getpwuid(getuid());
        if (!entry) throw std::runtime_error("System Error: cannot determine user home");
        home = entry->pw_dir;
    }
    fs::path value = platform::pathFromUtf8(home);
    if (kind != "home") {
        std::string variable, fallback;
        if (kind == "data") { variable = "XDG_DATA_HOME"; fallback = ".local/share"; }
        else if (kind == "config") { variable = "XDG_CONFIG_HOME"; fallback = ".config"; }
        else if (kind == "cache") { variable = "XDG_CACHE_HOME"; fallback = ".cache"; }
        else if (kind == "state") { variable = "XDG_STATE_HOME"; fallback = ".local/state"; }
        else throw std::runtime_error("System Error: unknown user directory kind");
        auto configured = platform::pathFromUtf8(platform::getEnvVar(variable));
        value = configured.is_absolute() ? configured : value / fallback;
    }
#endif
    return platform::pathToUtf8(value.lexically_normal());
#endif
}
}
void addDirectoryBuiltins(std::vector<Builtin>& out) {
    out.push_back({{"os_user_dir", "string", {{"string", "kind"}}, 1, false, "os",
        "Абсолютный пользовательский каталог: home/data/config/cache/state. Linux: XDG с fallback, Windows: Known Folders. Не создаёт каталог. Ошибки — System Error. Browser/Android недоступны."},
        [](Call& c) { return text(directory(c.text(0))); }});
    out.push_back({{"os_arch", "string", {}, 0, false, "os", "Архитектура текущего runtime: x86_64, arm64, x86, arm, wasm32 или unknown; не архитектура хоста при эмуляции."},
        [](Call&) {
#if defined(__EMSCRIPTEN__)
            return text("wasm32");
#elif defined(_M_X64) || defined(__x86_64__)
            return text("x86_64");
#elif defined(_M_ARM64) || defined(__aarch64__)
            return text("arm64");
#elif defined(_M_IX86) || defined(__i386__)
            return text("x86");
#elif defined(_M_ARM) || defined(__arm__)
            return text("arm");
#else
            return text("unknown");
#endif
        }});
}
}
