// Windows user environment primitives. Shell integration is expressed in std/env.skm.
#include "Builtin.h"
#include "skrumi/Platform.h"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace skrumi::runtime {
namespace {
#ifdef _WIN32
struct Key { HKEY value = nullptr; ~Key() { if (value) RegCloseKey(value); } };
[[noreturn]] void envError(const char* action, LSTATUS code) {
    throw std::runtime_error(std::string("Environment Error: ") + action + " (" + std::to_string(code) + ")");
}
std::wstring name(Call& c) {
    const auto& value = c.text(0);
    if (value.empty() || value.find_first_of("=\r\n") != std::string::npos || value.find('\0') != std::string::npos)
        throw std::runtime_error("Environment Error: invalid variable name");
    return platform::pathFromUtf8(value).native();
}
ValueList snapshot(HKEY key, const std::wstring& variable) {
    DWORD type = 0, count = 0;
    auto status = RegQueryValueExW(key, variable.c_str(), nullptr, &type, nullptr, &count);
    if (status == ERROR_FILE_NOT_FOUND) return {boolean(false), boolean(false), text("")};
    if (status != ERROR_SUCCESS) envError("read user environment", status);
    if (type != REG_SZ && type != REG_EXPAND_SZ) throw std::runtime_error("Environment Error: unsupported registry value type");
    if (count > 1024 * 1024 || count % sizeof(wchar_t)) throw std::runtime_error("Environment Error: invalid registry string size");
    std::wstring value(count / sizeof(wchar_t) + 1, L'\0');
    status = RegQueryValueExW(key, variable.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &count);
    if (status != ERROR_SUCCESS) envError("read user environment", status);
    value.resize(count / sizeof(wchar_t));
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    if (value.find(L'\0') != std::wstring::npos) throw std::runtime_error("Environment Error: embedded NUL in registry value");
    return {boolean(true), boolean(type == REG_EXPAND_SZ), text(platform::pathToUtf8(std::filesystem::path(value)))};
}
void valid(const ValueList& v) {
    if (v.size() != 3 || !v[0].isBool() || !v[1].isBool() || !v[2].isString() ||
        v[2].str().find('\0') != std::string::npos || (!v[0].asBool() && (v[1].asBool() || !v[2].str().empty())))
        throw std::runtime_error("Environment Error: expected [present, expandable, value]");
}
bool equal(const ValueList& a, const ValueList& b) {
    return a[0].asBool() == b[0].asBool() && a[1].asBool() == b[1].asBool() && a[2].str() == b[2].str();
}
Value userEnvironment(Call& c) {
    auto variable = name(c);
    Key key;
    auto status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key.value);
    if (status != ERROR_SUCCESS) envError("open user environment", status);
    auto before = snapshot(key.value, variable);
    if (c.spec.name == "env_user_snapshot") return makeArray(std::move(before));
    const auto& expected = c.array(1); const auto& desired = c.array(2);
    valid(expected); valid(desired);
    if (!equal(before, expected)) throw std::runtime_error("Environment Error: user environment changed concurrently; no replacement performed");
    if (desired[0].asBool()) {
        auto value = platform::pathFromUtf8(desired[2].str()).native();
        if (value.size() > 32766) throw std::runtime_error("Environment Error: user environment value too long");
        status = RegSetValueExW(key.value, variable.c_str(), 0, desired[1].asBool() ? REG_EXPAND_SZ : REG_SZ,
                               reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key.value, variable.c_str());
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    if (status != ERROR_SUCCESS) envError("write user environment", status);
    status = RegFlushKey(key.value);
    if (status != ERROR_SUCCESS) envError("flush user environment (value may already be changed)", status);
    DWORD_PTR ignored = 0;
    // Notification is advisory: unresponsive applications must not undo a successful registry write.
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 1000, &ignored);
    return nothing();
}
#else
Value userEnvironment(Call&) { throw std::runtime_error("Environment Error: Windows user environment registry is unavailable on this platform"); }
#endif
}
void addUserEnvironmentBuiltins(std::vector<Builtin>& out) {
    out.push_back({{"env_user_snapshot", "array", {{"string", "name"}}, 1, false, "env", "Windows HKCU Environment: [present, expandable, value], без раскрытия %VAR%. Не меняет окружение процесса. Другие платформы — Environment Error."}, userEnvironment});
    out.push_back({{"env_user_compare_exchange", "void", {{"string", "name"}, {"array", "expected"}, {"array", "replacement"}}, 3, false, "env", "Windows: проверяет снимок [present, expandable, value], записывает новый, RegFlushKey и уведомляет окружение. Отказ при отличии; это не атомарный CAS относительно стороннего редактора реестра."}, userEnvironment});
}
}
