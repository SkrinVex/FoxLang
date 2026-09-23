#include "Bundle.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace foxlang::bundle {
namespace {
constexpr unsigned char magic[] = {'F','O','X','B','N','D','L',0};
constexpr std::size_t maxName = 4096;

[[noreturn]] void invalid(const std::string& reason) {
    throw std::runtime_error("Bundle Error: " + reason);
}

void number(Bytes& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void string(Bytes& bytes, const std::string& value) {
    if (value.size() > maxPayload - 4 || bytes.size() > maxPayload - value.size() - 4) invalid("payload exceeds 64 MiB");
    number(bytes, static_cast<std::uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

class Reader {
    const Bytes& bytes;
    std::size_t pos = 8;
public:
    explicit Reader(const Bytes& data) : bytes(data) {}
    std::uint32_t number() {
        if (bytes.size() - pos < 4) invalid("truncated integer");
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[pos++]) << (8 * i);
        return value;
    }
    std::string string(std::size_t limit) {
        auto size = number();
        if (size > limit || size > bytes.size() - pos) invalid("invalid string length");
        std::string out(bytes.begin() + pos, bytes.begin() + pos + size);
        pos += size;
        return out;
    }
    bool done() const { return pos == bytes.size(); }
};

void name(const std::string& value) {
    if (value.empty() || value.size() > maxName || value.find('\0') != std::string::npos) invalid("invalid name");
}

void validate(const Sources& sources) {
    if (sources.files.empty() || sources.files.size() > maxFiles || sources.imports.size() > maxImports) invalid("invalid entry count");
    if (!sources.files.count(sources.entry)) invalid("entry point is missing");
    for (const auto& file : sources.files) name(file.first);
    for (const auto& edge : sources.imports) {
        name(std::get<2>(edge.first));
        if (!sources.files.count(std::get<0>(edge.first)) || !sources.files.count(edge.second)) invalid("import references a missing file");
    }
}
}

std::string Sources::resolve(const ModuleImport& request, const std::string& from) const {
    auto found = imports.find({from, request.usingModule, request.name});
    if (found == imports.end()) invalid("module '" + request.name + "' is not bundled for '" + from + "'");
    return found->second;
}

std::string Sources::read(const std::string& identity) const {
    auto found = files.find(identity);
    if (found == files.end()) invalid("missing source '" + identity + "'");
    return found->second;
}

Bytes encode(const Sources& sources) {
    validate(sources);
    Bytes bytes(std::begin(magic), std::end(magic));
    number(bytes, 1);
    number(bytes, static_cast<std::uint32_t>(sources.files.size()));
    number(bytes, static_cast<std::uint32_t>(sources.imports.size()));
    string(bytes, sources.entry);
    for (const auto& file : sources.files) {
        string(bytes, file.first);
        string(bytes, file.second);
    }
    for (const auto& edge : sources.imports) {
        string(bytes, std::get<0>(edge.first));
        number(bytes, std::get<1>(edge.first) ? 1 : 0);
        string(bytes, std::get<2>(edge.first));
        string(bytes, edge.second);
    }
    if (bytes.size() > maxPayload) invalid("payload exceeds 64 MiB");
    return bytes;
}

Sources decode(const Bytes& bytes) {
    if (bytes.size() < 24 || bytes.size() > maxPayload || !std::equal(std::begin(magic), std::end(magic), bytes.begin())) invalid("invalid payload magic or size");
    Reader reader(bytes);
    if (reader.number() != 1) invalid("unsupported payload version");
    auto fileCount = reader.number();
    auto importCount = reader.number();
    if (!fileCount || fileCount > maxFiles || importCount > maxImports) invalid("invalid entry count");
    Sources sources;
    sources.entry = reader.string(maxName);
    for (std::uint32_t i = 0; i < fileCount; ++i) {
        auto id = reader.string(maxName);
        auto source = reader.string(maxPayload);
        if (!sources.files.emplace(id, std::move(source)).second) invalid("duplicate file");
    }
    for (std::uint32_t i = 0; i < importCount; ++i) {
        auto from = reader.string(maxName);
        auto kind = reader.number();
        if (kind > 1) invalid("invalid import kind");
        auto requested = reader.string(maxName);
        auto target = reader.string(maxName);
        if (!sources.imports.emplace(Sources::ImportKey{from, kind == 1, requested}, target).second) invalid("duplicate import");
    }
    if (!reader.done()) invalid("unexpected trailing data");
    validate(sources);
    return sources;
}

std::uint32_t checksum(const Bytes& bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (auto byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

Sources collect(const std::string& entry, const SourceProvider& provider) {
    Sources sources;
    sources.entry = "main.fox";
    auto root = std::filesystem::canonical(entry).string();
    std::map<std::string, std::string> identities{{root, sources.entry}};
    std::vector<std::string> pending{root};
    std::size_t totalBytes = 0;
    // Iterative traversal handles cycles without recursion or duplicate source copies.
    for (std::size_t i = 0; i < pending.size(); ++i) {
        auto identity = pending[i];
        auto id = identities.at(identity);
        if (identity.rfind("@std/", 0) != 0 && std::filesystem::file_size(identity) > maxPayload - totalBytes) invalid("sources exceed 64 MiB");
        auto source = provider.read(identity);
        if (source.size() > maxPayload - totalBytes) invalid("sources exceed 64 MiB");
        totalBytes += source.size();
        Lexer lexer(source);
        Parser parser(lexer.tokenize(), id);
        parser.parseProgram(); // Parse only; never eval or load .env during build.
        sources.files.emplace(id, std::move(source));
        for (const auto& request : parser.getImports()) {
            auto resolved = provider.resolve(request, identity);
            auto found = identities.find(resolved);
            if (found == identities.end()) {
                if (identities.size() >= maxFiles) invalid("too many source files");
                auto localId = "module/" + std::to_string(identities.size()) + ".fox";
                found = identities.emplace(resolved, localId).first;
                pending.push_back(resolved);
            }
            sources.imports.emplace(Sources::ImportKey{id, request.usingModule, request.name}, found->second);
            if (sources.imports.size() > maxImports) invalid("too many imports");
        }
    }
    return sources;
}
} // namespace foxlang::bundle
