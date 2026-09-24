#include "Image.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace foxlang::bundle {
namespace fs = std::filesystem;
namespace {
class TemporaryDirectory {
public:
    fs::path path;
    explicit TemporaryDirectory(const fs::path& parent) {
        std::random_device random;
        for (int i = 0; i < 64; ++i) {
            auto candidate = parent / (".foxbuild-" + std::to_string(random()) + "-" + std::to_string(random()));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) {
                path = std::move(candidate);
                return;
            }
            if (ec && ec != std::errc::file_exists) throw fs::filesystem_error("cannot create build directory", candidate, ec);
        }
        throw std::runtime_error("Build Error: cannot create unique temporary directory");
    }
    ~TemporaryDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
}

void build(const fs::path& input, fs::path output) {
    if (output.empty()) output = input.stem();
#ifdef _WIN32
    auto extension = output.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".exe") output += ".exe";
#endif
    output = fs::absolute(output);
    if (fs::exists(fs::symlink_status(output))) throw std::runtime_error("Build Error: output already exists: " + output.string());
    if (!fs::is_regular_file(input)) throw std::runtime_error("Build Error: could not open file '" + input.string() + "'");
    auto sources = collect(input.string(), *filesystemSources());
    auto image = pack(readImage(executablePath()), sources);
    TemporaryDirectory temporary(output.parent_path());
    auto candidate = temporary.path / "program";
    {
        std::ofstream stream(candidate, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
        stream.close();
        if (!stream) throw std::runtime_error("Build Error: cannot write executable");
    }
    // Validate the bytes actually written before publishing the executable.
    unpack(readImage(candidate));
#ifdef _WIN32
    // MoveFileW refuses to replace an existing destination, including a racing writer.
    if (!MoveFileW(candidate.c_str(), output.c_str())) throw std::runtime_error("Build Error: cannot publish output (Windows error " + std::to_string(GetLastError()) + ")");
#else
    fs::permissions(candidate, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                    fs::perms::others_read | fs::perms::others_exec);
    // Same filesystem; link publishes atomically and never overwrites a source or output.
    fs::create_hard_link(candidate, output);
#endif
}
} // namespace foxlang::bundle
