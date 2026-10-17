#pragma once
#include "skrumi/SourceProvider.h"
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace skrumi::bundle {

using Bytes = std::vector<unsigned char>;
constexpr std::size_t maxPayload = 64 * 1024 * 1024;
constexpr std::size_t maxFiles = 4096;
constexpr std::size_t maxImports = 65536;

// Source IDs are bundle-local names, never extraction paths.
class Sources final : public SourceProvider {
public:
    using ImportKey = std::tuple<std::string, bool, std::string>;
    std::string entry;
    std::map<std::string, std::string> files;
    std::map<ImportKey, std::string> imports;
    std::map<std::string, std::string> resources;
    std::string resolve(const ModuleImport& request, const std::string& from) const override;
    std::string read(const std::string& identity) const override;
    std::string readResource(const std::string& name) const override;
    std::vector<std::string> listResources() const override;
};

Bytes encode(const Sources& sources);
Sources decode(const Bytes& bytes);
std::uint32_t checksum(const Bytes& bytes);
Sources collect(const std::string& entry, const SourceProvider& provider);

} // namespace skrumi::bundle
