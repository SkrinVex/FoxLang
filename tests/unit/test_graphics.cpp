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
    CHECK(pixels.pixels() == before);
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
    foxlang::Interpreter interpreter;
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
