#pragma once
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "foxlang/SourceProvider.h"

namespace foxlang {

// Sources as an editor sees them: unsaved buffers take the place of files on disk.
class OverlaySources final : public SourceProvider {
public:
    explicit OverlaySources(std::shared_ptr<const SourceProvider> base) : base(std::move(base)) {}
    std::string resolve(const ModuleImport& request, const std::string& from) const override;
    std::string read(const std::string& identity) const override;
    void set(const std::string& path, std::string text);
    void erase(const std::string& path);
    const std::string* find(const std::string& identity) const;

private:
    std::shared_ptr<const SourceProvider> base;
    std::map<std::string, std::string> buffers; // canonical path -> text
};

// The files that run together with a file. FoxLang has one global namespace per
// program, so utils.fox may call a function that render.fox defines when main.fox
// includes both. Such files are connected through include/using in either direction;
// the index finds that group among the .fox files under a root directory.
class ProjectIndex {
public:
    explicit ProjectIndex(std::shared_ptr<const OverlaySources> sources);

    void setRoot(const std::string& root) { rootDir = root; }
    const std::string& root() const { return rootDir; }

    // Other files of the same program, as canonical paths, never including file itself.
    std::vector<std::string> peers(const std::string& file);

    static constexpr size_t maxFiles = 2000;

private:
    struct Entry {
        std::filesystem::file_time_type modified;
        std::uintmax_t size = 0;
        std::vector<ModuleImport> imports;
    };
    std::shared_ptr<const OverlaySources> sources;
    std::string rootDir;
    std::map<std::string, Entry> cache;
    // Listing a directory tree on every keystroke is wasted work: it is reused briefly.
    std::string scannedFor;
    std::chrono::steady_clock::time_point scannedAt;
    std::vector<std::string> scanned;

    std::vector<std::string> scan(const std::string& file) const;
    const std::vector<ModuleImport>& importsOf(const std::string& path);
};

// weakly_canonical, as module resolution reports paths.
std::string canonicalPath(const std::string& path);

} // namespace foxlang
