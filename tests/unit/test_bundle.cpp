#include "Bundle.h"
#include "Image.h"
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace foxlang::bundle;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void rejects(const std::function<void()>& call) {
    try { call(); } catch (const std::exception&) { return; }
    throw std::runtime_error("corrupt bundle was accepted");
}

static void put(Bytes& data, std::size_t pos, std::uint64_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) data.at(pos + i) = static_cast<unsigned char>(value >> (8 * i));
}

static Bytes peFixture() {
    Bytes pe(512);
    pe[0] = 'M'; pe[1] = 'Z'; put(pe, 60, 64);
    pe[64] = 'P'; pe[65] = 'E';
    put(pe, 68, 0x8664, 2); put(pe, 70, 1, 2); // machine, sections
    put(pe, 84, 240, 2); put(pe, 86, 2, 2); // optional size, executable
    put(pe, 88, 0x20b, 2); put(pe, 196, 16); // PE32+, directory count
    const std::string section = ".foxbndl";
    std::copy(section.begin(), section.end(), pe.begin() + 328);
    put(pe, 344, 40); put(pe, 348, 400); // raw size, offset
    const std::string descriptor("FOXSTUB\0", 8);
    std::copy(descriptor.begin(), descriptor.end(), pe.begin() + 400);
    put(pe, 408, 1);
    return pe;
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected CLI image path");
        Sources sources;
        sources.entry = "main.fox";
        sources.files = {{"main.fox", "using helper;"}, {"helper.fox", "string word = \"Привет 🦊\";"}};
        sources.imports[{"main.fox", true, "helper"}] = "helper.fox";
        sources.imports[{"helper.fox", false, "main.fox"}] = "main.fox";
        auto payload = encode(sources);
        auto roundtrip = decode(payload);
        require(roundtrip.files == sources.files && roundtrip.imports == sources.imports && roundtrip.entry == sources.entry, "roundtrip changed sources");
        require(roundtrip.resolve({"helper", true}, "main.fox") == "helper.fox", "wrong import");
        rejects([&] { roundtrip.resolve({"helper", false}, "main.fox"); });
        rejects([&] { roundtrip.read("not bundled"); });
        require(checksum(Bytes{'1','2','3','4','5','6','7','8','9'}) == 0xcbf43926u, "wrong CRC32");
        for (std::size_t length = 0; length < payload.size(); ++length) {
            rejects([&] { decode(Bytes(payload.begin(), payload.begin() + length)); });
        }
        for (auto offset : {0, 8, 12, 16, 20}) {
            auto bad = payload;
            put(bad, offset, 0xffffffffu);
            rejects([&] { decode(bad); });
        }
        auto trailing = payload;
        trailing.push_back(0);
        rejects([&] { decode(trailing); });
        auto invalidSources = sources;
        invalidSources.entry = "missing";
        rejects([&] { encode(invalidSources); });
        invalidSources = sources;
        invalidSources.imports[{"main.fox", false, "missing"}] = "missing";
        rejects([&] { encode(invalidSources); });

        // Duplicate file records, missing entry, invalid graph and import kind.
        Sources small;
        small.entry = "a";
        small.files = {{"a", ""}, {"b", ""}};
        auto duplicate = encode(small);
        duplicate.at(38) = 'a';
        rejects([&] { decode(duplicate); });
        auto missingEntry = encode(small);
        missingEntry.at(24) = 'x';
        rejects([&] { decode(missingEntry); });
        small.imports[{"a", false, "b"}] = "b";
        auto badGraph = encode(small);
        badGraph.back() = 'x';
        rejects([&] { decode(badGraph); });
        auto badKind = encode(small);
        put(badKind, 48, 2);
        rejects([&] { decode(badKind); });

        auto cli = readImage(argv[1]);
        require(!unpack(cli), "CLI should be an empty stub");
        for (const auto& stub : {cli, peFixture()}) {
            auto executable = pack(stub, sources);
            require(unpack(executable)->files == sources.files, "pack lost files");
            rejects([&] { pack(executable, sources); });
            auto at = descriptorOffset(executable);
            for (auto offset : {at + 8, at + 12, at + 16, at + 24, at + 32, at + 36}) {
                auto broken = executable;
                put(broken, offset, 0xffffffffu);
                rejects([&] { unpack(broken); });
            }
            executable.resize(stub.size());
            rejects([&] { unpack(executable); });
        }
        auto pe = peFixture();
        for (auto offset : {60, 68, 84, 88, 344, 348}) {
            auto bad = pe;
            put(bad, offset, 0xffffffffu);
            rejects([&] { descriptorOffset(bad); });
        }
        put(pe, 232, 400); // Authenticode directory
        rejects([&] { descriptorOffset(pe); });
        rejects([&] { descriptorOffset(Bytes(100)); });
        std::cout << "BUNDLE_OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
