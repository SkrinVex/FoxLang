// File system details, path arithmetic and starting programs: what a file manager or
// a build script needs beyond reading and writing whole files.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#ifndef _WIN32
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace foxlang::runtime {
namespace {
namespace fs = std::filesystem;

fs::path pathOf(const std::string& text) { return platform::pathFromUtf8(text); }
std::string textOf(const fs::path& path) { return platform::pathToUtf8(path); }

std::string permissionText(fs::perms p) {
    auto bit = [&](fs::perms mask, char ch) { return (p & mask) != fs::perms::none ? ch : '-'; };
    std::string out;
    out += bit(fs::perms::owner_read, 'r');
    out += bit(fs::perms::owner_write, 'w');
    out += bit(fs::perms::owner_exec, 'x');
    out += bit(fs::perms::group_read, 'r');
    out += bit(fs::perms::group_write, 'w');
    out += bit(fs::perms::group_exec, 'x');
    out += bit(fs::perms::others_read, 'r');
    out += bit(fs::perms::others_write, 'w');
    out += bit(fs::perms::others_exec, 'x');
    return out;
}

// "755", "0644" or "rwxr-xr-x".
bool parsePermissions(const std::string& text, fs::perms& out) {
    if (text.size() == 9) {
        const char* letters = "rwxrwxrwx";
        unsigned value = 0;
        for (size_t i = 0; i < 9; ++i) {
            if (text[i] == letters[i]) value |= 1u << (8 - i);
            else if (text[i] != '-') return false;
        }
        out = static_cast<fs::perms>(value);
        return true;
    }
    std::string digits = text.size() == 4 && text[0] == '0' ? text.substr(1) : text;
    if (digits.size() != 3 || digits.find_first_not_of("01234567") != std::string::npos) return false;
    out = static_cast<fs::perms>(std::stoul(digits, nullptr, 8));
    return true;
}

// A path whose removal would wipe a whole drive or the user's home is refused.
bool protectedPath(const fs::path& path) {
    std::error_code ec;
    fs::path absolute = fs::weakly_canonical(fs::absolute(path, ec), ec);
    if (absolute.empty() || absolute == absolute.root_path()) return true;
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    return home && *home && fs::weakly_canonical(pathOf(home), ec) == absolute;
}

int& lastExitCode() {
    static thread_local int code = 0;
    return code;
}

std::string homeDirectory() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) home = std::getenv("USERPROFILE");
    if (home && *home) return home;
#ifndef _WIN32
    if (const passwd* entry = getpwuid(getuid())) return entry->pw_dir;
#endif
    return "";
}

} // namespace

void addFileBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };

    add({"fs_is_file", "bool", {{"string", "path"}}, 1, false, "fs", "Является ли путь обычным файлом."},
        [](Call& c) {
            std::error_code ec;
            return boolean(fs::is_regular_file(pathOf(c.text(0)), ec));
        });
    add({"fs_is_link", "bool", {{"string", "path"}}, 1, false, "fs", "Является ли путь символической ссылкой."},
        [](Call& c) {
            std::error_code ec;
            return boolean(fs::is_symlink(pathOf(c.text(0)), ec));
        });
    add({"fs_modified", "float", {{"string", "path"}}, 1, false, "fs",
         "Время последнего изменения в миллисекундах UNIX (как `time_now_ms`) или `-1`, если пути нет."},
        [](Call& c) {
            std::error_code ec;
            auto stamp = fs::last_write_time(pathOf(c.text(0)), ec);
            if (ec) return real(-1);
            // file_time_type has no portable epoch in C++17: shift it through "now" on both clocks.
            auto system = std::chrono::system_clock::now() +
                std::chrono::duration_cast<std::chrono::system_clock::duration>(stamp - fs::file_time_type::clock::now());
            return real(static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(system.time_since_epoch()).count()));
        });
    add({"fs_permissions", "string", {{"string", "path"}}, 1, false, "fs",
         "Права доступа в виде `rwxr-xr-x` (владелец, группа, остальные) или пустая строка, если пути нет. "
         "На Windows отражают только признак «только для чтения»."},
        [](Call& c) {
            std::error_code ec;
            auto status = fs::status(pathOf(c.text(0)), ec);
            if (ec || !fs::exists(status)) return text("");
            return text(permissionText(status.permissions()));
        });
    add({"fs_set_permissions", "bool", {{"string", "path"}, {"string", "mode"}}, 2, false, "fs",
         "Задаёт права доступа: восьмерично (`\"755\"`, `\"0644\"`) или буквами (`\"rwxr-x---\"`). `true` при успехе."},
        [](Call& c) {
            fs::perms mode{};
            if (!parsePermissions(c.text(1), mode))
                throw std::runtime_error("Runtime Error: fs_set_permissions() mode must look like 755 or rwxr-xr-x, got '" + c.text(1) + "'");
            std::error_code ec;
            fs::permissions(pathOf(c.text(0)), mode, fs::perm_options::replace, ec);
            return boolean(!ec);
        });
    add({"fs_is_executable", "bool", {{"string", "path"}}, 1, false, "fs",
         "Можно ли запустить файл: есть право на выполнение (на Windows — расширения `.exe`, `.bat`, `.cmd`, `.com`)."},
        [](Call& c) {
            std::error_code ec;
            auto path = pathOf(c.text(0));
            if (!fs::is_regular_file(path, ec)) return boolean(false);
#ifdef _WIN32
            std::string ext = textOf(path.extension());
            for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return boolean(ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com");
#else
            auto p = fs::status(path, ec).permissions();
            return boolean((p & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none);
#endif
        });
    add({"fs_owner", "string", {{"string", "path"}}, 1, false, "fs",
         "Имя пользователя-владельца файла; на Windows и для отсутствующего пути — пустая строка."},
        [](Call& c) {
#ifdef _WIN32
            (void)c;
            return text("");
#else
            struct stat info {};
            if (lstat(c.text(0).c_str(), &info) != 0) return text("");
            if (const passwd* entry = getpwuid(info.st_uid)) return text(entry->pw_name);
            return text(std::to_string(info.st_uid));
#endif
        });
    add({"fs_copy", "bool", {{"string", "from"}, {"string", "to"}, {"bool", "overwrite"}}, 2, false, "fs",
         "Копирует файл или каталог со всем содержимым, сохраняя права. Существующая цель заменяется только при "
         "`overwrite = true`, иначе — `false`."},
        [](Call& c) {
            std::error_code ec;
            auto from = pathOf(c.text(0)), to = pathOf(c.text(1));
            bool overwrite = c.has(2) && c.flag(2);
            if (!fs::exists(from, ec) || (fs::exists(to, ec) && !overwrite)) return boolean(false);
            auto options = fs::copy_options::recursive | fs::copy_options::copy_symlinks;
            if (overwrite) options |= fs::copy_options::overwrite_existing;
            fs::copy(from, to, options, ec);
            return boolean(!ec);
        });
    add({"fs_move", "bool", {{"string", "from"}, {"string", "to"}}, 2, false, "fs",
         "Переименовывает или перемещает файл или каталог, в том числе на другой диск. Если цель уже есть — `false`."},
        [](Call& c) {
            std::error_code ec;
            auto from = pathOf(c.text(0)), to = pathOf(c.text(1));
            if (!fs::exists(fs::symlink_status(from, ec)) || fs::exists(fs::symlink_status(to, ec))) return boolean(false);
            fs::rename(from, to, ec);
            if (!ec) return boolean(true);
            // Different file systems: copy, then remove the original only after a full copy.
            ec.clear();
            fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
            if (ec) {
                std::error_code ignore;
                fs::remove_all(to, ignore);
                return boolean(false);
            }
            fs::remove_all(from, ec);
            return boolean(!ec);
        });
    add({"fs_remove_all", "int", {{"string", "path"}}, 1, false, "fs",
         "Удаляет файл или каталог **со всем содержимым** и возвращает число удалённых объектов (`0`, если пути "
         "нет, `-1` при ошибке). Корень диска и домашний каталог удалить нельзя — это ошибка выполнения."},
        [](Call& c) {
            auto path = pathOf(c.text(0));
            if (c.text(0).empty() || protectedPath(path))
                throw std::runtime_error("Runtime Error: fs_remove_all() refuses to remove '" + c.text(0) + "'");
            std::error_code ec;
            auto removed = fs::remove_all(path, ec);
            if (ec) return integer(-1);
            return integer(static_cast<long long>(std::min<std::uintmax_t>(removed, 2147483647)));
        });
    add({"fs_free_space", "float", {{"string", "path"}}, 1, false, "fs",
         "Свободное место в байтах на диске, где находится путь, или `-1`."},
        [](Call& c) {
            std::error_code ec;
            auto info = fs::space(pathOf(c.text(0)), ec);
            return real(ec ? -1.0 : static_cast<double>(info.available));
        });
    add({"fs_home", "string", {}, 0, false, "fs", "Домашний каталог текущего пользователя."},
        [](Call&) { return text(homeDirectory()); });
    add({"fs_temp_dir", "string", {}, 0, false, "fs", "Каталог для временных файлов."},
        [](Call&) {
            std::error_code ec;
            return text(textOf(fs::temp_directory_path(ec)));
        });

    add({"path_join", "string", {{"string", "base"}, {"string", "name"}}, 2, false, "fs",
         "Соединяет части пути разделителем ОС: `path_join(\"/home/lis\", \"notes.txt\")`. Абсолютный `name` заменяет `base`."},
        [](Call& c) {
            if (c.text(0).empty()) return text(c.text(1));
            return text(textOf(pathOf(c.text(0)) / pathOf(c.text(1))));
        });
    add({"path_parent", "string", {{"string", "path"}}, 1, false, "fs",
         "Каталог, в котором находится путь: `/home/lis/a.txt` → `/home/lis`; у корня родитель — он сам."},
        [](Call& c) {
            auto path = pathOf(c.text(0));
            if (path.has_relative_path() && !path.has_filename()) path = path.parent_path(); // "dir/" -> "dir"
            auto parent = path.parent_path();
            return text(textOf(parent.empty() ? path.root_path() : parent));
        });
    add({"path_name", "string", {{"string", "path"}}, 1, false, "fs", "Последняя часть пути: имя файла или каталога."},
        [](Call& c) {
            auto path = pathOf(c.text(0));
            if (path.has_relative_path() && !path.has_filename()) path = path.parent_path();
            return text(textOf(path.filename()));
        });
    add({"path_stem", "string", {{"string", "path"}}, 1, false, "fs", "Имя файла без расширения: `archive.tar.gz` → `archive.tar`."},
        [](Call& c) { return text(textOf(pathOf(c.text(0)).stem())); });
    add({"path_extension", "string", {{"string", "path"}}, 1, false, "fs",
         "Расширение с точкой (`.gz`) или пустая строка. Файл вида `.env` расширения не имеет."},
        [](Call& c) { return text(textOf(pathOf(c.text(0)).extension())); });
    add({"path_absolute", "string", {{"string", "path"}}, 1, false, "fs",
         "Полный путь без `.` и `..`, отсчитанный от рабочего каталога."},
        [](Call& c) {
            std::error_code ec;
            auto absolute = fs::absolute(pathOf(c.text(0)), ec);
            return text(ec ? c.text(0) : textOf(absolute.lexically_normal()));
        });

    add({"os_open", "bool", {{"string", "target"}}, 1, false, "os",
         "Открывает файл, каталог или URL приложением по умолчанию (как двойной щелчок в проводнике). `true`, если удалось."},
        [](Call& c) { return boolean(platform::openWithDefaultApp(c.text(0))); });
    add({"os_run", "int", {{"string", "program"}, {"array", "args"}}, 1, false, "os",
         "Запускает программу с аргументами и ждёт её завершения. Возвращает код возврата, `-1` — не удалось "
         "запустить. Командная оболочка не используется: аргументы передаются как есть.\n\n"
         "```foxlang\nint code = os_run(\"git\", [\"status\", \"--short\"]);\n```"},
        [](Call& c) {
            std::vector<std::string> args;
            if (c.has(1)) for (const auto& item : c.array(1)) args.push_back(item.value.str());
            return integer(platform::runProcess(c.text(0), args, nullptr));
        });
    add({"os_run_output", "string", {{"string", "program"}, {"array", "args"}}, 1, false, "os",
         "Запускает программу и возвращает её стандартный вывод (до 16 МиБ); код возврата — `os_last_exit()`."},
        [](Call& c) {
            std::vector<std::string> args;
            if (c.has(1)) for (const auto& item : c.array(1)) args.push_back(item.value.str());
            std::string output;
            lastExitCode() = platform::runProcess(c.text(0), args, &output);
            return text(std::move(output));
        });
    add({"os_last_exit", "int", {}, 0, false, "os", "Код возврата программы из последнего `os_run_output`."},
        [](Call&) { return integer(lastExitCode()); });
}

} // namespace foxlang::runtime
