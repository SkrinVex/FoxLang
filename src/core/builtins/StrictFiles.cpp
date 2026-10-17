// Checked filesystem primitives for ordinary Skrumi applications. No installer policy.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <map>
#include <memory>
#include <limits>
#include <set>
#include <stdexcept>
#include <cerrno>
#ifndef __EMSCRIPTEN__
#include <mbedtls/sha256.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifndef __EMSCRIPTEN__
#include <sys/file.h>
#endif
#endif

namespace foxlang::platform {
struct FileLocks {
    struct Lock {
#ifdef _WIN32
        HANDLE value = INVALID_HANDLE_VALUE;
        ~Lock() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
#else
        int value = -1;
        ~Lock() { if (value >= 0) ::close(value); }
#endif
    };
    int next = 1;
    std::map<int, std::unique_ptr<Lock>> entries;
};
}
namespace foxlang::runtime {
namespace {
namespace fs = std::filesystem;
[[noreturn]] void error(const char* operation, const fs::path& path, std::error_code code) {
    throw std::runtime_error(std::string("System Error: ") + operation + " '" +
        platform::pathToUtf8(path) + "': " + code.category().name() + ":" +
        std::to_string(code.value()) + " " + code.message());
}
fs::path checkedPath(const std::string& value) {
    if (value.empty() || value.find('\0') != std::string::npos)
        throw std::runtime_error("System Error: path must be nonempty and contain no NUL");
    return platform::pathFromUtf8(value);
}
void noLinks(const fs::path& path) {
    fs::path current;
    for (const auto& part : fs::absolute(path)) {
        current /= part;
        std::error_code ec;
        auto status = fs::symlink_status(current, ec);
        if (ec == std::errc::no_such_file_or_directory) continue;
        if (ec) error("inspect", current, ec);
        if (fs::is_symlink(status)) error("symbolic link refused", current, std::make_error_code(std::errc::too_many_symbolic_link_levels));
#ifdef _WIN32
        auto attributes = GetFileAttributesW(current.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            error("reparse point refused", current, std::make_error_code(std::errc::too_many_symbolic_link_levels));
#endif
    }
}
fs::path safePath(const fs::path& inputBase, const std::string& relative) {
            auto base = fs::absolute(inputBase).lexically_normal();
            checkedPath(relative);
            if (relative.find_first_of("\\:*?\"<>|") != std::string::npos || relative.front() == '/' || relative.back() == '/' ||
                std::any_of(relative.begin(), relative.end(), [](unsigned char ch) { return ch < 32; }))
                error("unsafe relative path", base, std::make_error_code(std::errc::invalid_argument));
            size_t start = 0;
            do {
                auto end = relative.find('/', start);
                auto part = relative.substr(start, end == std::string::npos ? end : end - start);
                if (part.empty() || part.back() == '.' || part.back() == ' ')
                    error("unsafe component", base, std::make_error_code(std::errc::invalid_argument));
                auto device = part.substr(0, part.find('.'));
                for (char& ch : device) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                if (device == "CON" || device == "PRN" || device == "AUX" || device == "NUL" ||
                    (device.size() == 4 && (device.substr(0, 3) == "COM" || device.substr(0, 3) == "LPT") &&
                     device[3] >= '1' && device[3] <= '9'))
                    error("reserved device name", base, std::make_error_code(std::errc::invalid_argument));
                if (end == std::string::npos) break;
                start = end + 1;
            } while (true);
            auto path = base / checkedPath(relative);
            noLinks(path);
            return path;
}
fs::path temporaryDirectory(const fs::path& parent) {
    noLinks(parent);
    std::random_device random;
    for (int i = 0; i < 128; ++i) {
        auto path = parent / (".skrumi-tmp-" + std::to_string(random()) + "-" + std::to_string(random()));
#ifdef _WIN32
        if (CreateDirectoryW(path.c_str(), nullptr)) return path;
        auto code = GetLastError();
        if (code != ERROR_ALREADY_EXISTS) error("mkdir", path, {static_cast<int>(code), std::system_category()});
#else
        if (::mkdir(path.c_str(), 0700) == 0) return path;
        if (errno != EEXIST) error("mkdir", path, {errno, std::generic_category()});
#endif
    }
    error("temporary directory", parent, std::make_error_code(std::errc::file_exists));
}
void syncDirectory(const fs::path& path) {
#ifndef _WIN32
    int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) error("open directory", path, {errno, std::generic_category()});
    int result = ::fsync(fd), saved = errno;
    ::close(fd);
    if (result != 0) error("sync directory", path, {saved, std::generic_category()});
#else
    (void)path; // MoveFileExW below requests write-through; no portable directory fsync on Windows.
#endif
}
[[maybe_unused]] void atomicWrite(const fs::path& destination, const std::string& data) {
    noLinks(destination);
    if (fs::exists(destination) && !fs::is_regular_file(destination))
        error("replace non-file refused", destination, std::make_error_code(std::errc::invalid_argument));
    auto parent = fs::absolute(destination).parent_path();
    auto temporary = temporaryDirectory(parent);
    auto candidate = temporary / "data";
    try {
#ifdef _WIN32
        HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) error("create", candidate, {static_cast<int>(GetLastError()), std::system_category()});
        size_t offset = 0;
        DWORD failure = ERROR_SUCCESS;
        while (offset < data.size()) {
            DWORD written = 0;
            DWORD count = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 1024 * 1024));
            if (!WriteFile(file, data.data() + offset, count, &written, nullptr) || !written) { failure = GetLastError(); if (!failure) failure = ERROR_WRITE_FAULT; break; }
            offset += written;
        }
        if (!failure && !FlushFileBuffers(file)) failure = GetLastError();
        if (!CloseHandle(file) && !failure) failure = GetLastError();
        if (failure) error("write", candidate, {static_cast<int>(failure), std::system_category()});
        if (!MoveFileExW(candidate.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            error("replace", destination, {static_cast<int>(GetLastError()), std::system_category()});
#else
        int fd = ::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0) error("create", candidate, {errno, std::generic_category()});
        size_t offset = 0;
        int failure = 0;
        while (offset < data.size()) {
            auto written = ::write(fd, data.data() + offset, data.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { failure = written < 0 ? errno : EIO; break; }
            offset += static_cast<size_t>(written);
        }
        if (!failure && ::fsync(fd) != 0) failure = errno;
        if (::close(fd) != 0 && !failure) failure = errno;
        if (failure) error("write", candidate, {failure, std::generic_category()});
        if (::rename(candidate.c_str(), destination.c_str()) != 0) error("replace", destination, {errno, std::generic_category()});
        syncDirectory(parent);
#endif
        fs::remove(temporary);
    } catch (...) {
        std::error_code ignored;
        fs::remove(candidate, ignored);
        fs::remove(temporary, ignored);
        throw;
    }
}
struct TarEntry { std::string name; size_t offset, size; unsigned mode; bool directory; };
size_t octal(const std::string& bytes, size_t offset, size_t length) {
    size_t value = 0; bool ended = false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char ch = bytes[offset+i];
        if (ch == 0 || ch == ' ') { if (value || ch == 0) ended = true; continue; }
        if (ended || ch < '0' || ch > '7' || value > (64 * 1024 * 1024) / 8)
            throw std::runtime_error("Archive Error: invalid or excessive octal field");
        value = value * 8 + ch - '0';
    }
    return value;
}
std::string tarText(const std::string& data, size_t offset, size_t count) {
    auto value = data.substr(offset, count);
    auto zero = value.find('\0');
    if (zero != std::string::npos) value.resize(zero);
    return value;
}
std::vector<TarEntry> readTar(const std::string& data, const fs::path& destination) {
    if (data.size() > 64 * 1024 * 1024 || data.size() < 1024 || data.size() % 512)
        throw std::runtime_error("Archive Error: expected bounded, block-aligned USTAR archive");
    std::vector<TarEntry> entries;
    std::set<std::string> names;
    size_t cursor = 0;
    while (cursor + 512 <= data.size()) {
        bool zero = data.compare(cursor, 512, std::string(512, '\0')) == 0;
        if (zero) {
            if (cursor + 1024 > data.size() || data.find_first_not_of('\0', cursor) != std::string::npos)
                throw std::runtime_error("Archive Error: missing end blocks or trailing content");
            return entries;
        }
        if (entries.size() >= 4096 || data.compare(cursor+257, 6, std::string("ustar\0", 6)) != 0)
            throw std::runtime_error("Archive Error: unsupported archive format or too many entries");
        size_t checksum = 0;
        for (size_t i = 0; i < 512; ++i) checksum += (i >= 148 && i < 156) ? 32 : static_cast<unsigned char>(data[cursor+i]);
        if (checksum != octal(data, cursor+148, 8)) throw std::runtime_error("Archive Error: header checksum mismatch");
        char type = data[cursor+156];
        if (type != 0 && type != '0' && type != '5') throw std::runtime_error("Archive Error: links, devices and extensions are refused");
        auto name = tarText(data, cursor, 100), prefix = tarText(data, cursor+345, 155);
        if (!prefix.empty()) name = prefix + "/" + name;
        bool directory = type == '5';
        if (directory && !name.empty() && name.back() == '/') name.pop_back();
        safePath(destination, name);
        // Portable packages cannot contain names that collide on case-insensitive Windows volumes.
        std::string folded = name;
        for (auto& ch : folded) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (!names.insert(folded).second) throw std::runtime_error("Archive Error: duplicate entry");
        size_t size = octal(data, cursor+124, 12), mode = octal(data, cursor+100, 8);
        if (mode > 0777 || (directory && size)) throw std::runtime_error("Archive Error: unsafe mode or directory data");
        size_t padded = (size + 511) / 512 * 512;
        if (padded > data.size() - cursor - 512) throw std::runtime_error("Archive Error: truncated entry");
        entries.push_back({name, cursor+512, size, static_cast<unsigned>(mode), directory});
        cursor += 512 + padded;
    }
    throw std::runtime_error("Archive Error: missing end blocks");
}

}
void addStrictFileBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };
    add({"archive_extract", "array", {{"string", "bytes"}, {"string", "destination"}}, 2, false, "fs", "Проверяет и распаковывает USTAR из байтов в новый каталог. Возвращает имена файлов. До 64 МиБ/4096 записей; ссылки, устройства, traversal, расширения и перезапись запрещены. При I/O ошибке частичный каталог сохраняется."},
        [](Call& c) -> Value {
#ifdef __EMSCRIPTEN__
            (void)c; throw std::runtime_error("Archive Error: native archive extraction is unavailable in the browser");
#else
            auto destination = fs::absolute(checkedPath(c.text(1))); noLinks(destination);
            auto entries = readTar(c.text(0), destination); // Validate all headers before any write.
            std::error_code ec;
            if (!fs::create_directory(destination, ec)) {
                if (!ec) ec = std::make_error_code(std::errc::file_exists);
                error("create extraction directory", destination, ec);
            }
            fs::permissions(destination, fs::perms::owner_all, fs::perm_options::replace, ec);
            if (ec) error("restrict extraction directory", destination, ec);
            ValueList files;
            for (const auto& entry : entries) {
                auto path = safePath(destination, entry.name);
                fs::create_directories(entry.directory ? path : path.parent_path(), ec);
                if (ec) error("create archive directory", path, ec);
                if (!entry.directory) {
                    if (fs::exists(path, ec)) error("archive destination exists", path, std::make_error_code(std::errc::file_exists));
                    if (ec) error("inspect archive destination", path, ec);
                    atomicWrite(path, c.text(0).substr(entry.offset, entry.size));
                    fs::permissions(path, static_cast<fs::perms>(entry.mode), fs::perm_options::replace, ec);
                    if (ec) error("set archive permissions", path, ec);
                    files.push_back(text(entry.name));
                }
            }
            // Directory permissions are applied last so a read-only parent does not block extraction.
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) if (it->directory) {
                auto path = safePath(destination, it->name);
                fs::permissions(path, static_cast<fs::perms>(it->mode), fs::perm_options::replace, ec);
                if (ec) error("set archive directory permissions", path, ec);
            }
            syncDirectory(destination.parent_path());
            return makeArray(std::move(files));
#endif
        });
    add({"fs_lock", "int", {{"string", "path"}}, 1, false, "fs", "Неблокирующая эксклюзивная блокировка файла (создаёт при отсутствии). Возвращает handle; занято/ошибка — System Error. Не удаляйте lock-файл, пока возможны другие владельцы."},
        [](Call& c) -> Value {
#ifdef __EMSCRIPTEN__
            (void)c; throw std::runtime_error("System Error: file locks are unavailable in the browser");
#else
            auto path = checkedPath(c.text(0)); noLinks(path);
            auto& locks = c.ctx.getRoot()->fileLocks;
            if (!locks) locks = std::make_shared<platform::FileLocks>();
            if (locks->entries.size() >= 128 || locks->next == std::numeric_limits<int>::max())
                throw std::runtime_error("System Error: file lock handle limit");
            auto lock = std::make_unique<platform::FileLocks::Lock>();
#ifdef _WIN32
            lock->value = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (lock->value == INVALID_HANDLE_VALUE) error("lock", path, {static_cast<int>(GetLastError()), std::system_category()});
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(lock->value, &info)) error("inspect lock", path, {static_cast<int>(GetLastError()), std::system_category()});
            if (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
                error("invalid lock file", path, std::make_error_code(std::errc::invalid_argument));
#else
            lock->value = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (lock->value < 0) error("lock", path, {errno, std::generic_category()});
            struct stat info{};
            if (fstat(lock->value, &info) != 0) error("inspect lock", path, {errno, std::generic_category()});
            if (!S_ISREG(info.st_mode) || info.st_nlink != 1) error("invalid lock file", path, std::make_error_code(std::errc::invalid_argument));
            if (flock(lock->value, LOCK_EX | LOCK_NB) != 0) error("lock", path, {errno, std::generic_category()});
#endif
            int id = locks->next++; locks->entries.emplace(id, std::move(lock)); return integer(id);
#endif
        });
    add({"fs_unlock", "void", {{"int", "handle"}}, 1, false, "fs", "Освобождает fs_lock handle; неизвестный handle — System Error. Блокировки также снимаются при уничтожении runtime."},
        [](Call& c) {
            auto& locks = c.ctx.getRoot()->fileLocks;
            if (!locks || !locks->entries.erase(static_cast<int>(c.integer(0)))) throw std::runtime_error("System Error: unknown file lock handle");
            return nothing();
        });
    add({"fs_exists_checked", "bool", {{"string", "path"}}, 1, false, "fs", "Проверяет существование без следования ссылкам. Ошибки доступа — System Error; отсутствующий путь — false."},
        [](Call& c) {
            auto path = checkedPath(c.text(0)); noLinks(path);
            std::error_code ec; bool found = fs::exists(path, ec);
            if (ec) error("exists", path, ec);
            return boolean(found);
        });
    add({"fs_mkdir_checked", "void", {{"string", "path"}, {"bool", "exclusive"}}, 2, false, "fs", "Создаёт один каталог (родитель должен существовать). exclusive=true отклоняет уже существующий каталог. Ссылки запрещены; System Error при сбое."},
        [](Call& c) {
            auto path = checkedPath(c.text(0)); noLinks(path);
            std::error_code ec; bool made = fs::create_directory(path, ec);
            if (ec) error("mkdir", path, ec);
            if (!made && c.flag(1)) error("directory exists", path, std::make_error_code(std::errc::file_exists));
            syncDirectory(fs::absolute(path).parent_path());
            return nothing();
        });
    add({"fs_rename_checked", "void", {{"string", "from"}, {"string", "to"}}, 2, false, "fs", "Перемещает файл/каталог в пределах тома, отказываясь от существующей цели и ссылок. Синхронизирует родительские каталоги. System Error при сбое."},
        [](Call& c) {
            auto from = checkedPath(c.text(0)), to = checkedPath(c.text(1)); noLinks(from); noLinks(to);
            std::error_code ec;
            if (fs::exists(to, ec)) error("destination exists", to, std::make_error_code(std::errc::file_exists));
            if (ec) error("inspect destination", to, ec);
#ifdef _WIN32
            if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH))
                error("rename", to, {static_cast<int>(GetLastError()), std::system_category()});
#else
            // The caller must exclude concurrent writers; POSIX rename may replace a target created after the check.
            fs::rename(from, to, ec);
            if (ec) error("rename", to, ec);
            syncDirectory(fs::absolute(from).parent_path());
            syncDirectory(fs::absolute(to).parent_path());
#endif
            return nothing();
        });
    add({"fs_remove_checked", "void", {{"string", "path"}}, 1, false, "fs", "Удаляет один файл или пустой каталог. Отсутствие допускается. Ссылки запрещены; синхронизирует родителя. Не удаляет дерево."},
        [](Call& c) {
            auto path = checkedPath(c.text(0)); noLinks(path);
            std::error_code ec; bool removed = fs::remove(path, ec);
            if (ec) error("remove", path, ec);
            if (removed) syncDirectory(fs::absolute(path).parent_path());
            return nothing();
        });
    add({"fs_list_checked", "array", {{"string", "path"}}, 1, false, "fs", "Сортированные имена каталога; ошибка вместо пустого массива при недоступности. Ссылки запрещены."},
        [](Call& c) {
            auto path = checkedPath(c.text(0)); noLinks(path);
            std::error_code ec; std::vector<std::string> names;
            for (fs::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec))
                names.push_back(platform::pathToUtf8(it->path().filename()));
            if (ec) error("list", path, ec);
            std::sort(names.begin(), names.end()); ValueList result;
            for (auto& name : names) result.push_back(text(std::move(name)));
            return makeArray(std::move(result));
        });
    add({"sha256_bytes", "string", {{"string", "data"}}, 1, false, "fs", "SHA-256 байтов строки (включая NUL), 64 строчных hex-символа. Браузер: System Error."},
        [](Call& c) -> Value {
#ifdef __EMSCRIPTEN__
            (void)c; throw std::runtime_error("System Error: sha256_bytes is unavailable in the browser");
#else
            unsigned char digest[32];
            int result = mbedtls_sha256(reinterpret_cast<const unsigned char*>(c.text(0).data()), c.text(0).size(), digest, 0);
            if (result) throw std::runtime_error("System Error: SHA-256 failure " + std::to_string(result));
            static const char hex[] = "0123456789abcdef";
            std::string value;
            for (auto byte : digest) { value += hex[byte >> 4]; value += hex[byte & 15]; }
            return text(std::move(value));
#endif
        });
    add({"fs_sha256", "string", {{"string", "path"}}, 1, false, "fs",
         "SHA-256 обычного файла, 64 строчных hex-символа; чтение потоковое, ссылки запрещены. Ошибки вызывают System Error. В браузере недоступно."},
        [](Call& c) -> Value {
#ifdef __EMSCRIPTEN__
            (void)c;
            throw std::runtime_error("System Error: fs_sha256 is unavailable in the browser");
#else
            auto path = checkedPath(c.text(0));
            noLinks(path);
            std::error_code ec;
            bool regular = fs::is_regular_file(path, ec);
            if (ec) error("inspect hash input", path, ec);
            if (!regular) error("hash requires a regular file", path, std::make_error_code(std::errc::invalid_argument));
            std::ifstream stream(path, std::ios::binary);
            if (!stream) error("read hash input", path, {errno, std::generic_category()});
            struct Hash {
                mbedtls_sha256_context context;
                Hash() { mbedtls_sha256_init(&context); }
                ~Hash() { mbedtls_sha256_free(&context); }
            } hash;
            auto check = [&](int status) {
                if (status != 0) throw std::runtime_error("System Error: SHA-256 failure " + std::to_string(status));
            };
            check(mbedtls_sha256_starts(&hash.context, 0));
            char buffer[65536];
            while (stream.read(buffer, sizeof(buffer)) || stream.gcount())
                check(mbedtls_sha256_update(&hash.context, reinterpret_cast<const unsigned char*>(buffer), static_cast<size_t>(stream.gcount())));
            if (!stream.eof()) error("read hash input", path, std::make_error_code(std::errc::io_error));
            unsigned char digest[32];
            check(mbedtls_sha256_finish(&hash.context, digest));
            static const char hex[] = "0123456789abcdef";
            std::string result;
            for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
            return text(std::move(result));
#endif
        });
    add({"fs_read_checked", "string", {{"string", "path"}}, 1, false, "fs", "Читает файл (до 64 МиБ); ошибки чтения вызывают System Error, пустой файл возвращает пустую строку."},
        [](Call& c) {
            auto path = checkedPath(c.text(0));
            noLinks(path);
            std::error_code ec;
            bool regular = fs::is_regular_file(path, ec);
            if (ec) error("read", path, ec);
            if (!regular) error("read requires a regular file", path, std::make_error_code(std::errc::invalid_argument));
            std::ifstream stream(path, std::ios::binary);
            if (!stream) error("read", path, {errno, std::generic_category()});
            std::string data;
            char buffer[65536];
            while (stream.read(buffer, sizeof(buffer)) || stream.gcount()) {
                data.append(buffer, static_cast<size_t>(stream.gcount()));
                if (data.size() > 64 * 1024 * 1024) error("read limit", path, std::make_error_code(std::errc::file_too_large));
            }
            if (!stream.eof()) error("read", path, std::make_error_code(std::errc::io_error));
            return text(std::move(data));
        });
    add({"fs_write_atomic", "void", {{"string", "path"}, {"string", "data"}}, 2, false, "fs", "Записывает новый файл и атомарно заменяет цель в том же каталоге; синхронизирует данные. Ссылки запрещены. Ошибки вызывают System Error."},
        [](Call& c) {
#if defined(__EMSCRIPTEN__)
            (void)c;
            throw std::runtime_error("System Error: durable atomic writes are unavailable in the browser");
#else
            atomicWrite(checkedPath(c.text(0)), c.text(1));
#endif
            return nothing();
        });
    add({"fs_mkdtemp", "string", {{"string", "parent"}}, 1, false, "fs", "Создаёт уникальный временный каталог внутри существующего parent (0700 на POSIX). Возвращает абсолютный путь; ошибка вызывает System Error."},
        [](Call& c) { return text(platform::pathToUtf8(temporaryDirectory(fs::absolute(checkedPath(c.text(0)))))); });
    add({"path_safe_join", "string", {{"string", "base"}, {"string", "relative"}}, 2, false, "fs", "Соединяет base с переносимым относительным путём. Запрещает пустые компоненты, . и .., обратный слеш, двоеточие, абсолютные пути и существующие ссылки. Это проверка, не защита от конкурентной подмены каталогов."},
        [](Call& c) {
            return text(platform::pathToUtf8(safePath(checkedPath(c.text(0)), c.text(1))));
        });
}
} // namespace foxlang::runtime
