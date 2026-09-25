#include "foxlang/SourceProvider.h"
#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"
#include "EmbeddedStdlib.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace foxlang {
namespace {
class FilesystemSources final : public SourceProvider {
    std::string home;
public:
    explicit FilesystemSources(std::string root) : home(std::move(root)) {}

    std::string resolve(const ModuleImport& request, const std::string& from) const override {
        std::string name = request.name;
        if (request.usingModule && (name.size() < 4 || name.substr(name.size() - 4) != ".fox")) {
            name += ".fox";
        }
        auto onDisk = [&](const std::string& candidate, std::string& found) {
            try {
                found = runtime::resolveFoxFile(candidate, from, home);
                return true;
            } catch (const std::runtime_error&) {
                // Only resolution may fall back; module execution errors must propagate.
                return false;
            }
        };
        std::string found;
        if (request.usingModule) {
            // A standard module wins over a file of the same name next to the program:
            // `using arrays;` beside a script called arrays.fox must still mean std/arrays.
            std::string standard = "std/" + name;
            if (onDisk(standard, found)) return found;
            if (embeddedStdlib().count(standard)) return "@" + standard;
        }
        if (onDisk(name, found)) return found;
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
