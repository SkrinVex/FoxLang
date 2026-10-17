// Checked filesystem primitives for ordinary Skrumi applications. No installer policy.
#include "Builtin.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
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
#endif

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
}
void addStrictFileBuiltins(std::vector<Builtin>& out) {
    auto add = [&](BuiltinSpec spec, Handler handler) { out.push_back({std::move(spec), handler}); };
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
            auto base = fs::absolute(checkedPath(c.text(0))).lexically_normal();
            const auto& relative = c.text(1);
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
            return text(platform::pathToUtf8(path));
        });
}
} // namespace foxlang::runtime
