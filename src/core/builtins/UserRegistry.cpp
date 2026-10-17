// General current-user string registry operations; application policy stays in Skrumi.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace foxlang::runtime {
namespace {
#ifdef _WIN32
struct Key { HKEY value = nullptr; ~Key() { if (value) RegCloseKey(value); } };
[[noreturn]] void error(const char* operation, LSTATUS code) {
    throw std::runtime_error(std::string("Registry Error: ") + operation + " (" + std::to_string(code) + ")");
}
std::wstring checkedText(const std::string& value, size_t limit) {
    if (value.size() > limit || value.find('\0') != std::string::npos)
        throw std::runtime_error("Registry Error: invalid or excessive string");
    return platform::pathFromUtf8(value).native();
}
std::wstring keyPath(Call& c) {
    auto path = checkedText(c.text(0), 2048);
    if (path.rfind(L"Software\\", 0) != 0 || path.back() == L'\\' || path.find(L"\\\\") != std::wstring::npos || path.find(L'/') != std::wstring::npos)
        throw std::runtime_error("Registry Error: expected an HKCU Software\\ subkey path");
    return path;
}
ValueList absent() { return {boolean(false), boolean(false), text("")}; }
ValueList snapshot(HKEY key, const std::wstring& name) {
    DWORD type = 0, length = 0;
    auto status = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &length);
    if (status == ERROR_FILE_NOT_FOUND) return absent();
    if (status != ERROR_SUCCESS) error("read value", status);
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || length > 1024*1024 || length % sizeof(wchar_t))
        throw std::runtime_error("Registry Error: expected a bounded REG_SZ or REG_EXPAND_SZ value");
    std::wstring value(length/sizeof(wchar_t)+1, L'\0');
    status = RegQueryValueExW(key, name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &length);
    if (status != ERROR_SUCCESS) error("read value", status);
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || length % sizeof(wchar_t)) throw std::runtime_error("Registry Error: value type changed");
    value.resize(length/sizeof(wchar_t));
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    if (value.find(L'\0') != std::wstring::npos) throw std::runtime_error("Registry Error: embedded NUL in value");
    return {boolean(true), boolean(type == REG_EXPAND_SZ), text(platform::pathToUtf8(std::filesystem::path(value)))};
}
void validate(const ValueList& value) {
    if (value.size() != 3 || !value[0].isBool() || !value[1].isBool() || !value[2].isString() ||
        (!value[0].asBool() && (value[1].asBool() || !value[2].str().empty())))
        throw std::runtime_error("Registry Error: expected [present, expandable, string]");
    checkedText(value[2].str(), 1024*1024);
}
bool equal(const ValueList& a, const ValueList& b) {
    return a[0].asBool() == b[0].asBool() && a[1].asBool() == b[1].asBool() && a[2].str() == b[2].str();
}
Value registry(Call& c) {
    auto path = keyPath(c);
    bool removeKey = c.spec.name == "registry_user_remove_empty_key";
    bool write = c.spec.name == "registry_user_compare_exchange";
    auto name = removeKey ? std::wstring{} : checkedText(c.text(1), 256);
    Key key;
    auto status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0,
                               KEY_QUERY_VALUE | (write ? KEY_SET_VALUE : 0), &key.value);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND) error("open key", status);
    if (removeKey) {
        if (!key.value) return nothing();
        DWORD subkeys = 0, values = 0;
        status = RegQueryInfoKeyW(key.value, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr,
                                 &values, nullptr, nullptr, nullptr, nullptr);
        if (status != ERROR_SUCCESS) error("inspect key", status);
        if (subkeys || values) throw std::runtime_error("Registry Error: key is not empty; preserved");
        RegCloseKey(key.value); key.value = nullptr;
        status = RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) error("remove empty key", status);
        return nothing();
    }
    auto before = key.value ? snapshot(key.value, name) : absent();
    if (!write) return makeArray(std::move(before));
    const auto& expected = c.array(2); const auto& replacement = c.array(3);
    validate(expected); validate(replacement);
    if (!equal(before, expected)) throw std::runtime_error("Registry Error: concurrent value change; preserved");
    if (equal(before, replacement)) return nothing();
    if (!key.value) {
        status = RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 nullptr, &key.value, nullptr);
        if (status != ERROR_SUCCESS) error("create key", status);
        if (!equal(snapshot(key.value, name), expected)) throw std::runtime_error("Registry Error: concurrent key creation; preserved");
    }
    if (replacement[0].asBool()) {
        auto value = checkedText(replacement[2].str(), 1024*1024);
        status = RegSetValueExW(key.value, name.c_str(), 0, replacement[1].asBool() ? REG_EXPAND_SZ : REG_SZ,
                               reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    } else status = RegDeleteValueW(key.value, name.c_str());
    if (status != ERROR_SUCCESS) error("write value", status);
    status = RegFlushKey(key.value);
    if (status != ERROR_SUCCESS) error("flush key (value may already be changed)", status);
    return nothing();
}
#else
Value registry(Call&) { throw std::runtime_error("Registry Error: Windows user registry is unavailable on this platform"); }
#endif
}
void addUserRegistryBuiltins(std::vector<Builtin>& out) {
    out.push_back({{"registry_user_snapshot", "array", {{"string", "key"}, {"string", "name"}}, 2, false, "os", "Windows HKCU Software: снимок строкового значения [present, expandable, value]. Не раскрывает переменные; недоступно на других платформах."}, registry});
    out.push_back({{"registry_user_compare_exchange", "void", {{"string", "key"}, {"string", "name"}, {"array", "expected"}, {"array", "replacement"}}, 4, false, "os", "Проверяет и заменяет строковое значение HKCU Software, сохраняет тип, синхронизирует реестр. Не атомарно относительно стороннего редактора."}, registry});
    out.push_back({{"registry_user_remove_empty_key", "void", {{"string", "key"}}, 1, false, "os", "Удаляет только пустой HKCU Software подраздел. Вызывающий исключает конкурентные записи между проверкой и удалением."}, registry});
}
}
