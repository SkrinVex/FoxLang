#include "foxlang/SourceProvider.h"
#include "foxlang/Runtime.h"
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
        const std::vector<std::string> candidates = request.usingModule
            ? std::vector<std::string>{"std/" + name, name}
            : std::vector<std::string>{name};
        for (const auto& candidate : candidates) {
            try {
                return runtime::resolveFoxFile(candidate, from, home);
            } catch (const std::runtime_error&) {
                // Only resolution may fall back; module execution errors must propagate.
            }
        }
        for (const auto& candidate : candidates) {
            if (embeddedStdlib().count(candidate)) return "@" + candidate;
        }
        throw std::runtime_error("Module Error: '" + request.name + "' not found (imported by '" + from + "')");
    }

    std::string read(const std::string& identity) const override {
        if (identity.rfind("@std/", 0) == 0) return embeddedStdlib().at(identity.substr(1));
        std::ifstream file(identity, std::ios::binary);
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
