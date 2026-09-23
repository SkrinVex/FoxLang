#pragma once
#include "Bundle.h"
#include <filesystem>
#include <optional>

namespace foxlang::bundle {
constexpr std::size_t descriptorSize = 40;
constexpr std::size_t maxImage = 256 * 1024 * 1024;

// Locate the reserved descriptor using the ELF/PE section table, not a marker scan.
std::size_t descriptorOffset(const Bytes& image);
std::optional<Sources> unpack(const Bytes& image);
Bytes pack(Bytes image, const Sources& sources);
Bytes readImage(const std::filesystem::path& path);
std::filesystem::path executablePath();
void build(const std::filesystem::path& input, std::filesystem::path output);
} // namespace foxlang::bundle
