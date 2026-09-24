#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace foxlang::graphics {

// Shared software renderer: identical pixels on Win32 and X11, no GPU/asset files.
class Surface {
public:
    Surface(int width, int height);
    void clear(uint32_t color);
    void rectangle(int x, int y, int width, int height, uint32_t color);
    void circle(int x, int y, int radius, uint32_t color);
    void text(int x, int y, const std::string& text, int scale, uint32_t color);
    int width() const { return width_; }
    int height() const { return height_; }
    const std::vector<uint32_t>& pixels() const { return pixels_; }
private:
    int width_, height_;
    std::vector<uint32_t> pixels_;
};

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    bool poll();
    void present();
    bool keyDown(const std::string& key) const;
    bool keyPressed(const std::string& key) const;
    int mouseX() const { return mouseX_; }
    int mouseY() const { return mouseY_; }
    bool focused() const { return focused_; }
    double delta() const { return delta_; }
    Surface& surface() { return surface_; }
    // Native backends feed one shared input state. Press edges are reset by poll().
    void keyEvent(int key, bool down);
    void focusEvent(bool focused);
    void mouseEvent(int x, int y) { mouseX_ = x; mouseY_ = y; }
    void closeEvent() { open_ = false; }
private:
    struct Native;
    Surface surface_;
    std::unique_ptr<Native> native_;
    std::array<bool, 256> down_{};
    std::array<bool, 256> pressed_{};
    bool open_ = true, focused_ = false;
    int mouseX_ = 0, mouseY_ = 0;
    double delta_ = 1.0 / 60.0;
    std::chrono::steady_clock::time_point lastPoll_ = std::chrono::steady_clock::now();
    static int keyCode(const std::string& key);
};

} // namespace foxlang::graphics
