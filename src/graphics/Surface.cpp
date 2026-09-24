#include "Graphics.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace foxlang::graphics {
namespace {
using Glyph = std::array<uint8_t, 7>;
// Original 5x7 bitmap alphabet; no OS fonts, font files or third-party font license.
Glyph glyph(char32_t code) {
    if (code >= U'a' && code <= U'z') code -= 32;
    if (code >= U'а' && code <= U'я') code -= 32;
    if (code == U'ё') code = U'Ё';
    static const std::unordered_map<char32_t, Glyph> letters = {
        {U'A',{14,17,17,31,17,17,17}}, {U'B',{30,17,17,30,17,17,30}},
        {U'C',{14,17,16,16,16,17,14}}, {U'D',{30,17,17,17,17,17,30}},
        {U'E',{31,16,16,30,16,16,31}}, {U'F',{31,16,16,30,16,16,16}},
        {U'G',{14,17,16,23,17,17,15}}, {U'H',{17,17,17,31,17,17,17}},
        {U'I',{14,4,4,4,4,4,14}}, {U'J',{7,2,2,2,18,18,12}},
        {U'K',{17,18,20,24,20,18,17}}, {U'L',{16,16,16,16,16,16,31}},
        {U'M',{17,27,21,21,17,17,17}}, {U'N',{17,25,21,19,17,17,17}},
        {U'O',{14,17,17,17,17,17,14}}, {U'P',{30,17,17,30,16,16,16}},
        {U'Q',{14,17,17,17,21,18,13}}, {U'R',{30,17,17,30,20,18,17}},
        {U'S',{15,16,16,14,1,1,30}}, {U'T',{31,4,4,4,4,4,4}},
        {U'U',{17,17,17,17,17,17,14}}, {U'V',{17,17,17,17,17,10,4}},
        {U'W',{17,17,17,21,21,27,17}}, {U'X',{17,17,10,4,10,17,17}},
        {U'Y',{17,17,10,4,4,4,4}}, {U'Z',{31,1,2,4,8,16,31}},
        {U'0',{14,17,19,21,25,17,14}}, {U'1',{4,12,4,4,4,4,14}},
        {U'2',{14,17,1,2,4,8,31}}, {U'3',{30,1,1,14,1,1,30}},
        {U'4',{2,6,10,18,31,2,2}}, {U'5',{31,16,16,30,1,1,30}},
        {U'6',{14,16,16,30,17,17,14}}, {U'7',{31,1,2,4,8,8,8}},
        {U'8',{14,17,17,14,17,17,14}}, {U'9',{14,17,17,15,1,1,14}},
        {U' ',{0,0,0,0,0,0,0}}, {U'.',{0,0,0,0,0,6,6}},
        {U',',{0,0,0,0,6,6,4}}, {U':',{0,6,6,0,6,6,0}},
        {U';',{0,6,6,0,6,6,4}}, {U'!',{4,4,4,4,4,0,4}},
        {U'?',{14,17,1,2,4,0,4}}, {U'-',{0,0,0,31,0,0,0}},
        {U'+',{0,4,4,31,4,4,0}}, {U'/',{1,1,2,4,8,16,16}},
        {U'=',{0,0,31,0,31,0,0}}, {U'(',{2,4,8,8,8,4,2}},
        {U')',{8,4,2,2,2,4,8}}, {U'[',{14,8,8,8,8,8,14}},
        {U']',{14,2,2,2,2,2,14}}, {U'_', {0,0,0,0,0,0,31}},
        {U'♥',{0,10,31,31,14,4,0}}, {U'←',{0,4,8,31,8,4,0}},
        {U'→',{0,4,2,31,2,4,0}}, {U'Б',{31,16,16,30,17,17,30}},
        {U'Г',{31,16,16,16,16,16,16}}, {U'Д',{7,9,9,9,17,31,17}},
        {U'Ё',{10,0,31,16,30,16,31}}, {U'Ж',{21,21,14,4,14,21,21}},
        {U'З',{30,1,1,14,1,1,30}}, {U'И',{17,17,19,21,25,17,17}},
        {U'Й',{10,4,17,19,21,25,17}}, {U'Л',{7,9,9,9,9,9,17}},
        {U'П',{31,17,17,17,17,17,17}}, {U'У',{17,17,17,15,1,17,14}},
        {U'Ф',{4,14,21,21,21,14,4}}, {U'Ц',{18,18,18,18,18,31,1}},
        {U'Ч',{17,17,17,15,1,1,1}}, {U'Ш',{21,21,21,21,21,21,31}},
        {U'Щ',{21,21,21,21,21,31,1}}, {U'Ъ',{24,8,8,14,9,9,14}},
        {U'Ы',{17,17,17,29,21,21,29}}, {U'Ь',{16,16,16,30,17,17,30}},
        {U'Э',{14,17,1,7,1,17,14}}, {U'Ю',{18,21,21,29,21,21,18}},
        {U'Я',{15,17,17,15,5,9,17}}
    };
    static const std::u32string russian = U"АВЕКМНОРСТХ";
    static const std::u32string latin = U"ABEKMHOPCTX";
    auto equivalent = russian.find(code);
    if (equivalent != std::u32string::npos) code = latin[equivalent];
    auto found = letters.find(code);
    return found == letters.end() ? letters.at(U'?') : found->second;
}
char32_t nextCode(const std::string& text, size_t& i) {
    auto first = static_cast<unsigned char>(text[i++]);
    if (first < 128) return first;
    int count = first >= 0xf0 && first <= 0xf4 ? 3 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xc2 && first <= 0xdf ? 1 : 0;
    if (!count || i + count > text.size()) return U'?';
    char32_t code = first & ((1 << (6 - count)) - 1);
    for (int n = 0; n < count; ++n) {
        auto c = static_cast<unsigned char>(text[i]);
        if ((c & 0xc0) != 0x80) return U'?';
        ++i;
        code = (code << 6) | (c & 63);
    }
    if (code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
        (count == 1 && code < 128) || (count == 2 && code < 2048) || (count == 3 && code < 65536)) return U'?';
    return code;
}
}

Surface::Surface(int width, int height) : width_(width), height_(height) {
    if (width < 1 || height < 1 || width > 4096 || height > 4096 || int64_t(width) * height > 8388608)
        throw std::runtime_error("Graphics Error: dimensions must be 1..4096, at most 8388608 pixels");
    pixels_.resize(size_t(width) * height);
}
void Surface::clear(uint32_t color) { std::fill(pixels_.begin(), pixels_.end(), color & 0xffffff); }
void Surface::rectangle(int x, int y, int width, int height, uint32_t color) {
    if (width < 0 || height < 0) throw std::runtime_error("Graphics Error: rectangle size must not be negative");
    int left = std::max(0, x), top = std::max(0, y);
    int right = static_cast<int>(std::min<int64_t>(width_, int64_t(x) + width));
    int bottom = static_cast<int>(std::min<int64_t>(height_, int64_t(y) + height));
    if (left >= right || top >= bottom) return;
    for (int row = top; row < bottom; ++row)
        std::fill(pixels_.begin() + size_t(row) * width_ + left, pixels_.begin() + size_t(row) * width_ + right, color & 0xffffff);
}
void Surface::circle(int x, int y, int radius, uint32_t color) {
    if (radius < 0 || radius > 8192) throw std::runtime_error("Graphics Error: radius must be 0..8192");
    int64_t top = std::max<int64_t>(0, int64_t(y) - radius), bottom = std::min<int64_t>(height_ - 1, int64_t(y) + radius);
    for (int64_t row = top; row <= bottom; ++row) {
        int64_t dy = row - y;
        int64_t dx = static_cast<int64_t>(std::sqrt(double(int64_t(radius) * radius - dy * dy)));
        int64_t left = std::max<int64_t>(0, int64_t(x) - dx), right = std::min<int64_t>(width_ - 1, int64_t(x) + dx);
        if (left <= right) rectangle(static_cast<int>(left), static_cast<int>(row), static_cast<int>(right - left + 1), 1, color);
    }
}
void Surface::text(int x, int y, const std::string& value, int scale, uint32_t color) {
    if (scale < 1 || scale > 32) throw std::runtime_error("Graphics Error: text scale must be 1..32");
    if (value.size() > 65536) throw std::runtime_error("Graphics Error: text exceeds 65536 bytes");
    int64_t cursorX = x, cursorY = y;
    for (size_t i = 0; i < value.size();) {
        char32_t code = nextCode(value, i);
        if (code == U'\n') { cursorX = x; cursorY += 9 * scale; continue; }
        if (cursorX > -6 * scale && cursorX < width_ && cursorY > -7 * scale && cursorY < height_) {
            auto bitmap = glyph(code);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (bitmap[row] & (1 << (4 - col))) rectangle(static_cast<int>(cursorX) + col * scale, static_cast<int>(cursorY) + row * scale, scale, scale, color);
        }
        cursorX += 6 * scale;
    }
}
} // namespace foxlang::graphics
