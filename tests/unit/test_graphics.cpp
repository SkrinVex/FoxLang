#include "../../src/graphics/Graphics.h"
#include "foxlang/FoxLang.h"
#include <algorithm>
#include <climits>
#include <iostream>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL " #condition << ':' << __LINE__ << '\n'; return 1; } } while (0)
int main() {
    using foxlang::graphics::Surface;
    Surface pixels(32, 24);
    pixels.clear(0x102030);
    pixels.rectangle(-2, -2, 4, 4, 0xff0000);
    CHECK(pixels.pixels()[0] == 0xff0000);
    CHECK(pixels.pixels()[1 + 32] == 0xff0000);
    CHECK(pixels.pixels()[2] == 0x102030);
    auto before = pixels.pixels();
    pixels.rectangle(INT_MAX, INT_MAX, INT_MAX, INT_MAX, 0);
    pixels.rectangle(INT_MIN, INT_MIN, 4, 4, 0);
    pixels.circle(INT_MAX, INT_MIN, 8192, 0);
    CHECK(pixels.pixels() == before);
    pixels.circle(16, 12, 3, 0xabcdef);
    CHECK(pixels.pixels()[12 * 32 + 16] == 0xabcdef);
    CHECK(pixels.pixels()[12 * 32 + 19] == 0xabcdef);
    CHECK(pixels.pixels()[12 * 32 + 20] == 0x102030);
    pixels.clear(0);
    pixels.text(0, 0, "Лисий\nЁж", 1, 0xffffff);
    CHECK(std::count(pixels.pixels().begin(), pixels.pixels().end(), 0xffffff) > 30);
    before = pixels.pixels();
    pixels.clear(0);
    pixels.text(0, 0, "ЛИСИЙ\nЁЖ", 1, 0xffffff);
    CHECK(pixels.pixels() != before); // lower case has its own letters
    // Every Latin and Cyrillic lower-case letter has a glyph of its own, not '?'.
    for (const char* letter : {"a", "g", "m", "q", "z", "б", "д", "ж", "л", "ф", "щ", "ы", "ю", "я", "ё"}) {
        Surface a(8, 8), b(8, 8);
        a.text(0, 0, letter, 1, 0xffffff);
        b.text(0, 0, "?", 1, 0xffffff);
        CHECK(a.pixels() != b.pixels());
    }
    {
        Surface latin(8, 8), cyrillic(8, 8);
        latin.text(0, 0, "o", 1, 0xffffff);
        cyrillic.text(0, 0, "о", 1, 0xffffff);
        CHECK(latin.pixels() == cyrillic.pixels()); // Cyrillic о looks like Latin o
    }
    pixels.text(INT_MAX, INT_MAX, "ignored", 32, 0);
    pixels.text(INT_MIN, INT_MIN, "ignored", 1, 0);
    pixels.text(0, 0, "\xf0\x80\x80\x80\xed\xa0\x80\xff", 1, 0xff00ff);
    bool rejected = false;
    try { Surface invalid(4096, 4096); } catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
    rejected = false;
    try { pixels.circle(0, 0, -1, 0); } catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
    rejected = false;
    try { pixels.text(0, 0, "test", 0, 0); } catch (const std::runtime_error&) { rejected = true; }
    CHECK(rejected);
    // Lines, frames, rings and text measurement
    pixels.clear(0);
    pixels.line(0, 0, 31, 23, 0x00ff00);
    CHECK(pixels.pixels()[0] == 0x00ff00 && pixels.pixels()[23 * 32 + 31] == 0x00ff00);
    pixels.line(-100, 5, 100, 5, 0x0000ff);
    CHECK(pixels.pixels()[5 * 32] == 0x0000ff && pixels.pixels()[5 * 32 + 31] == 0x0000ff);
    pixels.clear(0);
    pixels.frame(2, 2, 10, 8, 1, 0xff0000);
    CHECK(pixels.pixels()[2 * 32 + 2] == 0xff0000 && pixels.pixels()[9 * 32 + 11] == 0xff0000);
    CHECK(pixels.pixels()[5 * 32 + 6] == 0);
    pixels.clear(0);
    pixels.ring(16, 12, 6, 1, 0xffffff);
    CHECK(pixels.pixels()[12 * 32 + 22] == 0xffffff && pixels.pixels()[12 * 32 + 16] == 0);
    CHECK(Surface::textWidth("ab", 1) == 11 && Surface::textWidth("ab\nЛисий", 2) == 58 && Surface::textWidth("", 3) == 0);
    // Punctuation used in games and interfaces has its own glyph instead of '?'
    for (const char* symbol : {"%", "\"", "'", "*", "#", "<", ">", "@", "&", "{", "}", "|", "$", "№", "°", "«", "»"}) {
        Surface a(8, 8), b(8, 8);
        a.text(0, 0, symbol, 1, 0xffffff);
        b.text(0, 0, "?", 1, 0xffffff);
        CHECK(a.pixels() != b.pixels());
    }
    // X11 keysyms a Russian layout sends become Cyrillic text
    using foxlang::graphics::keysymToUnicode;
    CHECK(keysymToUnicode('a') == 'a' && keysymToUnicode('/') == '/' && keysymToUnicode(0xE9) == 0xE9);
    CHECK(keysymToUnicode(0x6CC) == 0x43B && keysymToUnicode(0x6EC) == 0x41B); // л Л
    CHECK(keysymToUnicode(0x6C1) == 0x430 && keysymToUnicode(0x6FF) == 0x42A); // а Ъ
    CHECK(keysymToUnicode(0x6A3) == 0x451 && keysymToUnicode(0x6B3) == 0x401); // ё Ё
    CHECK(keysymToUnicode(0x100263A) == 0x263A && keysymToUnicode(0xFFB5) == '5');
    CHECK(keysymToUnicode(0xFF08) == 0 && keysymToUnicode(0xFFE1) == 0); // BackSpace, Shift type nothing
    foxlang::Interpreter interpreter;
    auto measured = interpreter.runSource("using graphics; int w = text_width(\"FoxLang\", 2);");
    CHECK(measured.success && interpreter.getGlobal("w").value == "82");
    auto result = interpreter.runSource("using graphics; int c = rgb(18, 52, 86); close_window(); close_window();");
    CHECK(result.success);
    CHECK(interpreter.getGlobal("c").value == "1193046");
    result = interpreter.runSource("gfx_rgb(256, 0, 0);");
    CHECK(!result.success);
    result = interpreter.runSource("gfx_rect(0, 0, 20, 20, 0);");
    CHECK(!result.success && result.errorMessage.find("open_window") != std::string::npos);
    result = interpreter.runSource("gfx_open(0, 100, \"invalid\");");
    CHECK(!result.success);
    CHECK(!interpreter.getContext().graphics);
    interpreter.reset();
    CHECK(!interpreter.getContext().graphics);
    std::cout << "GRAPHICS_RASTER_AND_VALIDATION_OK\n";
}
