#pragma once
#include <cstdint>
#include <string>
#include <vector>

// The NDK core can also be hosted directly from C++. All callbacks run on the
// interpreter thread. A Host is installed for the entire execution/cleanup scope.
namespace foxlang::android {
struct Event {
    enum Kind { Key, Text, Mouse, Wheel, Focus, Size, Close } kind;
    int a = 0, b = 0;
    std::string text;
};
struct Host {
    virtual ~Host() = default;
    virtual std::string directory() const = 0;
    virtual void checkCancelled() = 0;
    virtual void pause(long long milliseconds) = 0;
    virtual void output(const std::string& text, bool error) = 0;
    virtual std::string input() = 0;
    virtual void open(int width, int height, const std::string& title) = 0;
    virtual void close() noexcept = 0;
    virtual void present(const uint32_t* pixels, int width, int height) = 0;
    virtual std::vector<Event> poll() = 0;
    virtual void resizable(bool enabled) = 0;
    virtual void fullscreen(bool enabled) = 0;
    virtual std::string clipboard() = 0;
    virtual void clipboard(const std::string& text) = 0;
    virtual bool playWav(const std::vector<uint8_t>& bytes) = 0;
    virtual void stopSound() noexcept = 0;
    virtual bool openUri(const std::string& uri) = 0;
};
Host* current() noexcept;
Host& host();
struct Scope {
    explicit Scope(Host& host);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    Host* previous;
};
void checkpoint();
}
