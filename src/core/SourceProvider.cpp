#include "skrumi/SourceProvider.h"
#include "skrumi/Runtime.h"
#include "skrumi/Platform.h"
#include "EmbeddedStdlib.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace skrumi {
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
        if (!implicit && !isSourceFile(name))
            throw std::runtime_error("Module Error: source files must use .skm: '" + name + "'");
        const std::vector<std::string> names{implicit ? name + ".skm" : name};
        if (!request.usingModule && from.rfind("@std/", 0) == 0) {
            auto key = "std/" + name;

            if (embeddedStdlib().count(key)) return "@" + key;
            throw std::runtime_error("Module Error: missing embedded standard source '" + key + "'");
        }
        auto onDisk = [&](const std::string& candidate, std::string& found) {
            try {
                found = runtime::resolveSourceFile(candidate, from, home);
            } catch (const std::runtime_error&) {
                // Only resolution may fall back; module execution errors must propagate.
                return false;
            }
            return true;
        };
        std::string found;
        if (request.usingModule) {
            // A standard module wins over a file of the same name next to the program:
            // `using arrays;` beside a script called arrays.skm must still mean std/arrays.
            for (const auto& file : names) if (onDisk("std/" + file, found)) return found;
            for (const auto& file : names) {
                auto standard = "std/" + file;

                if (embeddedStdlib().count(standard)) return "@" + standard;
            }
        }
        for (const auto& file : names) if (onDisk(file, found)) return found;
        // Explicit std paths are also available when the CLI has no disk stdlib.
        if (!request.usingModule) {
            auto key = name;

            if (key.rfind("std/", 0) == 0 && embeddedStdlib().count(key)) return "@" + key;
        }
        throw std::runtime_error("Module Error: '" + request.name + "' not found (imported by '" + from + "')");
    }

    std::string read(const std::string& identity) const override {
        if (!isSourceFile(identity)) throw std::runtime_error("Module Error: source files must use .skm");
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

std::shared_ptr<const SourceProvider> filesystemSources(const std::string& skrumiHome) {
    return std::make_shared<FilesystemSources>(skrumiHome);
}
} // namespace skrumi
