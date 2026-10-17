#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace foxlang::graphics {
class Surface;
class Font {
public:
    explicit Font(std::string bytes);
    ~Font();
    int width(const std::string& text, int pixels);
    void draw(Surface& surface, int x, int y, const std::string& text, int pixels, uint32_t color);
private:
    struct State;
    std::unique_ptr<State> state_;
};
class Fonts {
public:
    int load(const std::string& bytes);
    Font& get(int handle);
    void release(int handle);
private:
    std::vector<std::unique_ptr<Font>> faces_;
};
const char* fontLicense();
}
