#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace foxlang::debug {

// Debug Adapter Protocol messages framed by Content-Length, over the adapter's own
// standard streams or over a TCP connection to the editor.
class Channel {
public:
    virtual ~Channel() = default;
    // Blocks for the next message; false once the editor has gone.
    bool read(std::string& message);
    // Safe to call from any thread.
    void write(const std::string& message);

    // The adapter talks over stdin/stdout, so the program must not: its output is
    // captured and handed to onOutput (as UTF-8 text), and its input reads nothing.
    static std::unique_ptr<Channel> standardStreams(std::function<void(const std::string&)> onOutput);
    // The program keeps its terminal; the protocol goes over a connection to host:port.
    static std::unique_ptr<Channel> connect(const std::string& host, int port);

protected:
    virtual long receive(char* buffer, size_t size) = 0;
    virtual bool send(const char* data, size_t size) = 0;

private:
    std::string pending_;
    std::mutex writing_;
};

} // namespace foxlang::debug
