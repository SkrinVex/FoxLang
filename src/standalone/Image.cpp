#include "Image.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace foxlang::bundle {
namespace {
[[noreturn]] void invalid(const std::string& why) {
    throw std::runtime_error("Bundle Error: " + why);
}

void bounds(const Bytes& bytes, std::uint64_t offset, std::uint64_t length) {
    if (offset > bytes.size() || length > bytes.size() - offset) invalid("executable offset out of bounds");
}

std::uint64_t get(const Bytes& bytes, std::uint64_t offset, unsigned width) {
    bounds(bytes, offset, width);
    std::uint64_t result = 0;
    for (unsigned i = 0; i < width; ++i) result |= std::uint64_t(bytes[static_cast<std::size_t>(offset) + i]) << (8 * i);
    return result;
}

void put(Bytes& bytes, std::size_t offset, std::uint64_t value, unsigned width) {
    bounds(bytes, offset, width);
    for (unsigned i = 0; i < width; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}

bool matches(const Bytes& bytes, std::uint64_t offset, const std::string& text) {
    bounds(bytes, offset, text.size());
    return std::equal(text.begin(), text.end(), bytes.begin() + static_cast<std::size_t>(offset));
}

void checkDescriptor(const Bytes& bytes, std::size_t at) {
    if (!matches(bytes, at, std::string("FOXSTUB\0", 8)) || get(bytes, at + 8, 4) != 1 || get(bytes, at + 36, 4) != 0) invalid("invalid stub descriptor");
}
}

std::size_t descriptorOffset(const Bytes& image) {
    if (image.size() < 64 || image.size() > maxImage + maxPayload) invalid("invalid executable size");
    std::uint64_t result = 0;
    auto record = [&](std::uint64_t at, std::uint64_t size) {
        if (result || !at || size < descriptorSize) invalid("invalid or duplicate .foxbndl section");
        bounds(image, at, size);
        result = at;
    };
    if (matches(image, 0, std::string("\177ELF", 4))) {
        if (get(image, 4, 1) != 2 || get(image, 5, 1) != 1 || get(image, 6, 1) != 1 ||
            get(image, 18, 2) != 62 || get(image, 20, 4) != 1 ||
            (get(image, 16, 2) != 2 && get(image, 16, 2) != 3) || get(image, 52, 2) != 64) {
            invalid("expected an x86_64 little-endian ELF executable");
        }
        auto table = get(image, 40, 8);
        auto count = get(image, 60, 2);
        auto namesIndex = get(image, 62, 2);
        if (!count || namesIndex >= count || get(image, 58, 2) != 64) invalid("unsupported ELF section table");
        bounds(image, table, count * 64);
        auto namesHeader = table + namesIndex * 64;
        auto names = get(image, namesHeader + 24, 8);
        auto namesSize = get(image, namesHeader + 32, 8);
        bounds(image, names, namesSize);
        for (std::uint64_t i = 0; i < count; ++i) {
            auto header = table + i * 64;
            auto type = get(image, header + 4, 4);
            auto at = get(image, header + 24, 8);
            auto size = get(image, header + 32, 8);
            if (type != 8) bounds(image, at, size); // SHT_NOBITS has no file data.
            auto nameAt = get(image, header, 4);
            if (nameAt >= namesSize) invalid("invalid ELF section name");
            if (namesSize - nameAt >= 9 && matches(image, names + nameAt, std::string(".foxbndl\0", 9))) {
                if (type != 1) invalid("stub must be an ELF PROGBITS section");
                record(at, size);
            }
        }
        auto programs = get(image, 32, 8);
        auto programCount = get(image, 56, 2);
        if (!programCount || get(image, 54, 2) != 56) invalid("invalid ELF program table");
        bounds(image, programs, programCount * 56);
        for (std::uint64_t i = 0; i < programCount; ++i) {
            auto header = programs + i * 56;
            bounds(image, get(image, header + 8, 8), get(image, header + 32, 8));
        }
    } else if (matches(image, 0, "MZ")) {
        auto pe = get(image, 60, 4);
        bounds(image, pe, 24);
        if (!matches(image, pe, std::string("PE\0\0", 4)) || get(image, pe + 4, 2) != 0x8664 ||
            (get(image, pe + 22, 2) & 0x2002) != 2) invalid("expected an x86_64 PE executable");
        auto count = get(image, pe + 6, 2);
        auto optionalSize = get(image, pe + 20, 2);
        auto optional = pe + 24;
        bounds(image, optional, optionalSize);
        if (!count || optionalSize < 152 || get(image, optional, 2) != 0x20b) invalid("invalid PE32+ header");
        // Packaging modifies the image: do not silently invalidate an Authenticode signature.
        if (get(image, optional + 108, 4) > 4 && (get(image, optional + 144, 4) || get(image, optional + 148, 4))) invalid("signed PE images are not supported");
        auto table = optional + optionalSize;
        bounds(image, table, count * 40);
        for (std::uint64_t i = 0; i < count; ++i) {
            auto header = table + i * 40;
            auto at = get(image, header + 20, 4);
            auto size = get(image, header + 16, 4);
            bounds(image, at, size);
            if (matches(image, header, ".foxbndl")) record(at, size);
        }
    } else {
        invalid("unsupported executable format (expected ELF64 or PE32+)");
    }
    if (!result) invalid("missing .foxbndl section");
    checkDescriptor(image, static_cast<std::size_t>(result));
    return static_cast<std::size_t>(result);
}

std::optional<Sources> unpack(const Bytes& image) {
    auto descriptor = descriptorOffset(image);
    auto mode = get(image, descriptor + 12, 4);
    auto offset = get(image, descriptor + 16, 8);
    auto size = get(image, descriptor + 24, 8);
    auto crc = get(image, descriptor + 32, 4);
    if (!mode) {
        if (offset || size || crc) invalid("invalid empty stub descriptor");
        return std::nullopt;
    }
    if (mode != 1 || offset < descriptor + descriptorSize || offset > maxImage || size > maxPayload) invalid("invalid payload descriptor");
    bounds(image, offset, size);
    if (offset + size != image.size()) invalid("payload length does not match executable size");
    // All executable sections/tables must fit before the payload.
    Bytes stub(image.begin(), image.begin() + static_cast<std::size_t>(offset));
    descriptorOffset(stub);
    Bytes payload(image.begin() + static_cast<std::size_t>(offset), image.end());
    if (checksum(payload) != crc) invalid("payload checksum mismatch");
    return decode(payload);
}

Bytes pack(Bytes image, const Sources& sources) {
    auto descriptor = descriptorOffset(image);
    if (unpack(image)) invalid("cannot use a bundled program as a CLI stub");
    if (image.size() > maxImage) invalid("stub exceeds 256 MiB");
    auto payload = encode(sources);
    put(image, descriptor + 12, 1, 4);
    put(image, descriptor + 16, image.size(), 8);
    put(image, descriptor + 24, payload.size(), 8);
    put(image, descriptor + 32, checksum(payload), 4);
    image.insert(image.end(), payload.begin(), payload.end());
    return image;
}

Bytes readImage(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) invalid("cannot open executable");
    auto end = stream.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > maxImage + maxPayload) invalid("invalid executable size");
    Bytes bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) invalid("cannot read executable");
    return bytes;
}

std::filesystem::path executablePath() {
#ifdef _WIN32
    std::vector<wchar_t> path(32768);
    auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) invalid("cannot locate executable");
    return std::filesystem::path(std::wstring(path.data(), size));
#elif defined(__linux__)
    // The kernel identifies the running image, including PATH and symlink launches.
    return "/proc/self/exe";
#else
    invalid("standalone packaging is supported on Linux and Windows x86_64");
#endif
}
} // namespace foxlang::bundle
