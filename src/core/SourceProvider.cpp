#include "foxlang/SourceProvider.h"
#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"
#include "EmbeddedStdlib.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace foxlang {
bool validResourceName(const std::string& name) {
    if (name.empty() || name.size() > 4096 || name.front() == '/' || name.back() == '/' ||
        name.find_first_of("\\:") != std::string::npos || name.find('\0') != std::string::npos) return false;
    size_t start = 0;
    for (;;) {
        auto end = name.find('/', start);
        auto part = name.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
}

std::string SourceProvider::readResource(const std::string& name) const {
    if (!validResourceName(name)) throw std::runtime_error("Resource Error: invalid resource name");
    throw std::runtime_error("Resource Error: no bundled resource '" + name + "'");
}

namespace {
class FilesystemSources final : public SourceProvider {
    std::string home;
public:
    explicit FilesystemSources(std::string root) : home(std::move(root)) {}

    std::string resolve(const ModuleImport& request, const std::string& from) const override {
        std::string name = request.name;
        const bool implicit = request.usingModule && !isSourceFile(name);
        // Legacy importing files retain their collision semantics, even when they
        // are included by a modern entry point. CLI spelling never changes this.
        const bool legacy = from.size() >= 4 && from.substr(from.size() - 4) == ".fox";
        const std::vector<std::string> names = implicit
            ? std::vector<std::string>{name + (legacy ? ".fox" : ".skm"), name + (legacy ? ".skm" : ".fox")}
            : std::vector<std::string>{name};
        if (!request.usingModule && from.rfind("@std/", 0) == 0) {
            auto key = "std/" + name;
            auto alias = stdlibAliases().find(key);
            if (alias != stdlibAliases().end()) key = alias->second;
            if (embeddedStdlib().count(key)) return "@" + key;
            throw std::runtime_error("Module Error: missing embedded standard source '" + key + "'");
        }
        auto onDisk = [&](const std::string& candidate, std::string& found) {
            try {
                found = runtime::resolveFoxFile(candidate, from, home);
            } catch (const std::runtime_error&) {
                // Only resolution may fall back; module execution errors must propagate.
                return false;
            }
            // Shipped compatibility adapters are transparent identities, not a
            // second set of declarations. Never redirect a user's real .fox file.
            auto path = platform::pathFromUtf8(found);
            const auto key = "std/" + platform::pathToUtf8(path.filename());
            auto alias = stdlibAliases().find(key);
            if (alias != stdlibAliases().end()) {
                std::ifstream input(path, std::ios::binary);
                const auto& expected = embeddedStdlib().at(key);
                std::string contents(expected.size() + 1, '\0');
                input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
                contents.resize(static_cast<size_t>(input.gcount()));
                if (!input.bad() && contents == expected) {
                    auto target = path.parent_path() / platform::pathFromUtf8(alias->second).filename();
                    if (!std::filesystem::is_regular_file(target))
                        throw std::runtime_error("Module Error: missing compatibility target '" + platform::pathToUtf8(target) + "'");
                    found = platform::pathToUtf8(std::filesystem::canonical(target));
                }
            }
            return true;
        };
        std::string found;
        if (request.usingModule) {
            // A standard module wins over a file of the same name next to the program:
            // `using arrays;` beside a script called arrays.fox must still mean std/arrays.
            for (const auto& file : names) if (onDisk("std/" + file, found)) return found;
            for (const auto& file : names) {
                auto standard = "std/" + file;
                auto alias = stdlibAliases().find(standard);
                if (alias != stdlibAliases().end()) standard = alias->second;
                if (embeddedStdlib().count(standard)) return "@" + standard;
            }
        }
        for (const auto& file : names) if (onDisk(file, found)) return found;
        // Explicit std paths are also available when the CLI has no disk stdlib.
        if (!request.usingModule) {
            auto key = name;
            auto alias = stdlibAliases().find(key);
            if (alias != stdlibAliases().end()) key = alias->second;
            if (key.rfind("std/", 0) == 0 && embeddedStdlib().count(key)) return "@" + key;
        }
        throw std::runtime_error("Module Error: '" + request.name + "' not found (imported by '" + from + "')");
    }

    std::string read(const std::string& identity) const override {
        if (identity.rfind("@std/", 0) == 0) return embeddedStdlib().at(identity.substr(1));
        std::ifstream file(platform::pathFromUtf8(identity), std::ios::binary);
        if (!file) throw std::runtime_error("Module Error: Cannot open file '" + identity + "'");
        std::ostringstream out;
        out << file.rdbuf();
        if (file.bad()) throw std::runtime_error("Module Error: Cannot read file '" + identity + "'");
        return out.str();
    }
};
}

std::shared_ptr<const SourceProvider> filesystemSources(const std::string& foxHome) {
    return std::make_shared<FilesystemSources>(foxHome);
}
} // namespace foxlang
