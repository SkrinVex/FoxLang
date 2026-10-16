#pragma once
#include <memory>
#include <string>

namespace foxlang {

// Both suffixes share the same parser, runtime and module loader.
inline bool isSourceFile(const std::string& name) {
    if (name.size() < 4) return false;
    const auto suffix = name.substr(name.size() - 4);
    return suffix == ".skm" || suffix == ".fox";
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
};

std::shared_ptr<const SourceProvider> filesystemSources(const std::string& foxHome = "");

} // namespace foxlang
