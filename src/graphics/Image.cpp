#include "Image.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace foxlang::graphics {
namespace {

[[noreturn]] void broken(const std::string& reason) { throw std::runtime_error("Graphics Error: " + reason); }

// DEFLATE (RFC 1951) as PNG uses it: stored, fixed and dynamic Huffman blocks.
class Inflater {
public:
    Inflater(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    std::vector<uint8_t> run(size_t expected) {
        out_.reserve(expected);
        bool last = false;
        while (!last) {
            last = bits(1);
            unsigned type = bits(2);
            if (type == 0) stored();
            else if (type == 1) fixed();
            else if (type == 2) dynamic();
            else broken("PNG data is damaged (bad DEFLATE block)");
            if (out_.size() > limit_) broken("PNG image is too large");
        }
        return std::move(out_);
    }

private:
    struct Huffman {
        std::array<uint16_t, 16> counts{};
        std::vector<uint16_t> symbols;
    };

    const uint8_t* data_;
    size_t size_, pos_ = 0;
    uint32_t buffer_ = 0;
    int available_ = 0;
    std::vector<uint8_t> out_;
    static constexpr size_t limit_ = size_t{512} * 1024 * 1024;

    unsigned bits(int count) {
        while (available_ < count) {
            if (pos_ >= size_) broken("PNG data ends too early");
            buffer_ |= uint32_t(data_[pos_++]) << available_;
            available_ += 8;
        }
        unsigned value = buffer_ & ((1u << count) - 1);
        buffer_ >>= count;
        available_ -= count;
        return value;
    }

    static Huffman build(const uint8_t* lengths, size_t count) {
        Huffman table;
        for (size_t i = 0; i < count; ++i) ++table.counts[lengths[i]];
        table.counts[0] = 0;
        std::array<uint16_t, 16> offsets{};
        for (int i = 1; i < 16; ++i) offsets[i] = static_cast<uint16_t>(offsets[i - 1] + table.counts[i - 1]);
        table.symbols.resize(count);
        for (size_t i = 0; i < count; ++i)
            if (lengths[i]) table.symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        return table;
    }

    unsigned decode(const Huffman& table) {
        int code = 0, first = 0, index = 0;
        for (int length = 1; length < 16; ++length) {
            code |= static_cast<int>(bits(1));
            int count = table.counts[length];
            if (code - count < first) return table.symbols[static_cast<size_t>(index + (code - first))];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        broken("PNG data is damaged (bad Huffman code)");
    }

    void stored() {
        buffer_ = 0;
        available_ = 0;
        if (pos_ + 4 > size_) broken("PNG data ends too early");
        unsigned length = data_[pos_] | (data_[pos_ + 1] << 8);
        unsigned complement = data_[pos_ + 2] | (data_[pos_ + 3] << 8);
        pos_ += 4;
        if ((length ^ 0xFFFF) != complement) broken("PNG data is damaged (stored block)");
        if (pos_ + length > size_) broken("PNG data ends too early");
        out_.insert(out_.end(), data_ + pos_, data_ + pos_ + length);
        pos_ += length;
    }

    void codes(const Huffman& literals, const Huffman& distances) {
        static const uint16_t lengthBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                              35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const uint8_t lengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t distanceBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
                                                769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const uint8_t distanceExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
                                                9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            unsigned symbol = decode(literals);
            if (symbol < 256) {
                out_.push_back(static_cast<uint8_t>(symbol));
            } else if (symbol == 256) {
                return;
            } else {
                symbol -= 257;
                if (symbol >= 29) broken("PNG data is damaged (bad length)");
                size_t length = lengthBase[symbol] + bits(lengthExtra[symbol]);
                unsigned distanceSymbol = decode(distances);
                if (distanceSymbol >= 30) broken("PNG data is damaged (bad distance)");
                size_t distance = distanceBase[distanceSymbol] + bits(distanceExtra[distanceSymbol]);
                if (distance > out_.size()) broken("PNG data is damaged (distance too far)");
                size_t from = out_.size() - distance;
                for (size_t i = 0; i < length; ++i) out_.push_back(out_[from + i]);
                if (out_.size() > limit_) broken("PNG image is too large");
            }
        }
    }

    void fixed() {
        uint8_t lengths[288 + 32];
        std::fill(lengths, lengths + 144, 8);
        std::fill(lengths + 144, lengths + 256, 9);
        std::fill(lengths + 256, lengths + 280, 7);
        std::fill(lengths + 280, lengths + 288, 8);
        std::fill(lengths + 288, lengths + 320, 5);
        codes(build(lengths, 288), build(lengths + 288, 32));
    }

    void dynamic() {
        unsigned literalCount = bits(5) + 257, distanceCount = bits(5) + 1, codeCount = bits(4) + 4;
        static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        uint8_t codeLengths[19] = {};
        for (unsigned i = 0; i < codeCount; ++i) codeLengths[order[i]] = static_cast<uint8_t>(bits(3));
        Huffman lengthCodes = build(codeLengths, 19);
        uint8_t lengths[320] = {};
        unsigned index = 0;
        while (index < literalCount + distanceCount) {
            unsigned symbol = decode(lengthCodes);
            if (symbol < 16) {
                lengths[index++] = static_cast<uint8_t>(symbol);
                continue;
            }
            unsigned repeat = 0;
            uint8_t value = 0;
            if (symbol == 16) {
                if (index == 0) broken("PNG data is damaged (repeat with nothing before)");
                value = lengths[index - 1];
                repeat = 3 + bits(2);
            } else if (symbol == 17) {
                repeat = 3 + bits(3);
            } else {
                repeat = 11 + bits(7);
            }
            if (index + repeat > literalCount + distanceCount) broken("PNG data is damaged (too many lengths)");
            while (repeat--) lengths[index++] = value;
        }
        codes(build(lengths, literalCount), build(lengths + literalCount, distanceCount));
    }
};

uint32_t bigEndian(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t littleEndian(const uint8_t* p) { return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

void checkSize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || width > 16384 || height > 16384 || uint64_t(width) * height > 64u * 1024 * 1024)
        broken("image must be 1..16384 pixels on each side and at most 67108864 pixels");
}

Image decodePng(const std::string& bytes) {
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    size_t size = bytes.size(), pos = 8;
    uint32_t width = 0, height = 0;
    uint8_t depth = 0, colorType = 0, interlace = 0;
    std::vector<uint8_t> packed, palette, transparency;
    bool header = false;
    while (pos + 8 <= size) {
        uint32_t length = bigEndian(data + pos);
        std::string type(reinterpret_cast<const char*>(data + pos + 4), 4);
        if (length > size || pos + 12 + length > size) broken("PNG file ends too early");
        const uint8_t* chunk = data + pos + 8;
        if (type == "IHDR") {
            if (length < 13) broken("PNG header is damaged");
            width = bigEndian(chunk);
            height = bigEndian(chunk + 4);
            depth = chunk[8];
            colorType = chunk[9];
            interlace = chunk[12];
            header = true;
        } else if (type == "PLTE") {
            palette.assign(chunk, chunk + length);
        } else if (type == "tRNS") {
            transparency.assign(chunk, chunk + length);
        } else if (type == "IDAT") {
            packed.insert(packed.end(), chunk, chunk + length);
        } else if (type == "IEND") {
            break;
        }
        pos += 12 + length;
    }
    if (!header) broken("PNG header is missing");
    checkSize(width, height);
    int channels = colorType == 0 ? 1 : colorType == 2 ? 3 : colorType == 3 ? 1 : colorType == 4 ? 2 : colorType == 6 ? 4 : 0;
    bool depthOk = depth == 8 || depth == 16 || (depth < 8 && (colorType == 0 || colorType == 3) && (depth == 1 || depth == 2 || depth == 4));
    if (!channels || !depthOk) broken("unsupported PNG color type or bit depth");
    if (colorType == 3 && palette.empty()) broken("PNG palette is missing");
    if (packed.size() < 2 || (packed[0] & 15) != 8) broken("PNG data is not zlib-compressed");

    size_t bitsPerPixel = size_t(channels) * depth;
    size_t bytesPerPixel = std::max<size_t>(1, bitsPerPixel / 8);
    auto rowBytes = [&](uint32_t w) { return (size_t(w) * bitsPerPixel + 7) / 8; };
    size_t expected = (rowBytes(width) + 1) * height;
    std::vector<uint8_t> raw = Inflater(packed.data() + 2, packed.size() - 2).run(expected);

    Image image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.assign(size_t(width) * height, 0);

    auto sample = [&](const uint8_t* row, uint32_t x, int channel) -> unsigned {
        if (depth == 8) return row[size_t(x) * channels + size_t(channel)];
        if (depth == 16) return row[(size_t(x) * channels + size_t(channel)) * 2]; // the high byte is enough on screen
        size_t bit = size_t(x) * depth;
        return (row[bit / 8] >> (8 - depth - bit % 8)) & ((1u << depth) - 1);
    };
    auto sample16 = [&](const uint8_t* row, uint32_t x, int channel) -> unsigned {
        if (depth == 16) return (row[(size_t(x) * channels + size_t(channel)) * 2] << 8) | row[(size_t(x) * channels + size_t(channel)) * 2 + 1];
        return sample(row, x, channel);
    };
    auto pixel = [&](const uint8_t* row, uint32_t x) -> uint32_t {
        unsigned r = 0, g = 0, b = 0, a = 255;
        unsigned scale = depth < 8 ? 255 / ((1u << depth) - 1) : 1;
        switch (colorType) {
            case 0: {
                unsigned v = sample(row, x, 0);
                r = g = b = v * scale;
                if (transparency.size() >= 2 && sample16(row, x, 0) == ((unsigned(transparency[0]) << 8) | transparency[1])) a = 0;
                break;
            }
            case 2:
                r = sample(row, x, 0); g = sample(row, x, 1); b = sample(row, x, 2);
                if (transparency.size() >= 6 && sample16(row, x, 0) == ((unsigned(transparency[0]) << 8) | transparency[1]) &&
                    sample16(row, x, 1) == ((unsigned(transparency[2]) << 8) | transparency[3]) &&
                    sample16(row, x, 2) == ((unsigned(transparency[4]) << 8) | transparency[5])) a = 0;
                break;
            case 3: {
                unsigned index = sample(row, x, 0);
                if (size_t(index) * 3 + 2 >= palette.size()) broken("PNG pixel refers past the palette");
                r = palette[index * 3]; g = palette[index * 3 + 1]; b = palette[index * 3 + 2];
                if (index < transparency.size()) a = transparency[index];
                break;
            }
            case 4:
                r = g = b = sample(row, x, 0);
                a = sample(row, x, 1);
                break;
            default:
                r = sample(row, x, 0); g = sample(row, x, 1); b = sample(row, x, 2); a = sample(row, x, 3);
        }
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
    };

    // Reverses the per-row filters of one (sub)image and writes its pixels.
    size_t offset = 0;
    auto pass = [&](uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t dx, uint32_t dy) {
        if (w == 0 || h == 0) return;
        size_t stride = rowBytes(w);
        std::vector<uint8_t> previous(stride, 0), current(stride);
        for (uint32_t y = 0; y < h; ++y) {
            if (offset + 1 + stride > raw.size()) broken("PNG data ends too early");
            uint8_t filter = raw[offset];
            std::memcpy(current.data(), raw.data() + offset + 1, stride);
            offset += 1 + stride;
            for (size_t i = 0; i < stride; ++i) {
                unsigned left = i >= bytesPerPixel ? current[i - bytesPerPixel] : 0;
                unsigned up = previous[i];
                unsigned corner = i >= bytesPerPixel ? previous[i - bytesPerPixel] : 0;
                unsigned add = 0;
                switch (filter) {
                    case 0: break;
                    case 1: add = left; break;
                    case 2: add = up; break;
                    case 3: add = (left + up) / 2; break;
                    case 4: {
                        int p = int(left) + int(up) - int(corner);
                        int pa = std::abs(p - int(left)), pb = std::abs(p - int(up)), pc = std::abs(p - int(corner));
                        add = pa <= pb && pa <= pc ? left : pb <= pc ? up : corner;
                        break;
                    }
                    default: broken("PNG data is damaged (bad filter)");
                }
                current[i] = static_cast<uint8_t>(current[i] + add);
            }
            for (uint32_t x = 0; x < w; ++x)
                image.pixels[size_t(y0 + y * dy) * width + (x0 + x * dx)] = pixel(current.data(), x);
            std::swap(previous, current);
        }
    };
    if (interlace == 0) {
        pass(width, height, 0, 0, 1, 1);
    } else {
        static const uint32_t start[7][2] = {{0, 0}, {4, 0}, {0, 4}, {2, 0}, {0, 2}, {1, 0}, {0, 1}};
        static const uint32_t step[7][2] = {{8, 8}, {8, 8}, {4, 8}, {4, 4}, {2, 4}, {2, 2}, {1, 2}};
        for (int p = 0; p < 7; ++p) {
            uint32_t w = width > start[p][0] ? (width - start[p][0] + step[p][0] - 1) / step[p][0] : 0;
            uint32_t h = height > start[p][1] ? (height - start[p][1] + step[p][1] - 1) / step[p][1] : 0;
            pass(w, h, start[p][0], start[p][1], step[p][0], step[p][1]);
        }
    }
    return image;
}

// Uncompressed 24- and 32-bit BMP, bottom-up or top-down.
Image decodeBmp(const std::string& bytes) {
    const auto* data = reinterpret_cast<const uint8_t*>(bytes.data());
    if (bytes.size() < 54) broken("BMP file is damaged");
    uint32_t offset = littleEndian(data + 10);
    int32_t width = static_cast<int32_t>(littleEndian(data + 18));
    int32_t height = static_cast<int32_t>(littleEndian(data + 22));
    unsigned bits = data[28] | (data[29] << 8);
    uint32_t compression = littleEndian(data + 30);
    if ((bits != 24 && bits != 32) || (compression != 0 && compression != 3)) broken("only uncompressed 24/32-bit BMP is supported");
    bool topDown = height < 0;
    uint32_t w = static_cast<uint32_t>(width), h = static_cast<uint32_t>(topDown ? -int64_t(height) : height);
    checkSize(w, h);
    size_t stride = (size_t(w) * bits / 8 + 3) & ~size_t(3);
    if (offset + stride * h > bytes.size()) broken("BMP file ends too early");
    Image image;
    image.width = static_cast<int>(w);
    image.height = static_cast<int>(h);
    image.pixels.resize(size_t(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* row = data + offset + stride * (topDown ? y : h - 1 - y);
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t* p = row + size_t(x) * (bits / 8);
            uint32_t alpha = bits == 32 ? p[3] : 255;
            image.pixels[size_t(y) * w + x] = (alpha << 24) | (uint32_t(p[2]) << 16) | (uint32_t(p[1]) << 8) | p[0];
        }
    }
    // A 32-bit BMP that leaves every alpha at zero means an opaque picture.
    if (bits == 32 && std::all_of(image.pixels.begin(), image.pixels.end(), [](uint32_t c) { return (c >> 24) == 0; }))
        for (auto& c : image.pixels) c |= 0xFF000000u;
    return image;
}

} // namespace

Image decodeImage(const std::string& bytes) {
    static const char png[] = "\x89PNG\r\n\x1a\n";
    if (bytes.size() >= 8 && std::memcmp(bytes.data(), png, 8) == 0) return decodePng(bytes);
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') return decodeBmp(bytes);
    broken("unsupported image format: PNG and BMP are supported");
}

} // namespace foxlang::graphics
