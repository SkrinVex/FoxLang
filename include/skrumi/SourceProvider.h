#pragma once
#include <memory>
#include <string>
#include <vector>

namespace skrumi {

// Only the canonical source extension is accepted.
inline bool isSourceFile(const std::string& name) {
    if (name.size() < 4) return false;
    const auto suffix = name.substr(name.size() - 4);
    return suffix == ".skm";
}

struct ModuleImport {
    std::string name;
    bool usingModule = false;
};

// Source identities are opaque to the interpreter. A bundle never falls back to disk.
class SourceProvider {
public:
    virtual ~SourceProvider() = default;
    virtual std::string resolve(const ModuleImport& request, const std::string& from) const = 0;
    virtual std::string read(const std::string& identity) const = 0;
    // Optional immutable assets. Missing resources never fall back to the cwd.
    virtual std::string readResource(const std::string& name) const;
    virtual std::vector<std::string> listResources() const { return {}; }
};

bool validResourceName(const std::string& name);

std::shared_ptr<const SourceProvider> filesystemSources(const std::string& skrumiHome = "");

} // namespace skrumi
