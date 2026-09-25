#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace foxlang::graphics {

// A decoded picture: 0xAARRGGBB per pixel, rows top to bottom.
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
};

// PNG (every color type, bit depths 1..16, palettes, transparency, interlacing) and
// uncompressed 24/32-bit BMP. A damaged or unsupported file is a Graphics Error.
Image decodeImage(const std::string& bytes);

} // namespace foxlang::graphics
