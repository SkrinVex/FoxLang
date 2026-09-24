#include "Channel.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace foxlang::debug {
namespace {

constexpr size_t maxMessage = 64 * 1024 * 1024;

// Output arrives in arbitrary chunks; a character split between two of them must
// not become two broken halves in two events.
std::string completeCharacters(std::string& carry, const char* data, size_t size) {
    carry.append(data, size);
    size_t end = carry.size();
    size_t back = 0;
    while (back < 4 && back < end && (static_cast<unsigned char>(carry[end - 1 - back]) & 0xC0) == 0x80) ++back;
    if (back < end) {
        auto lead = static_cast<unsigned char>(carry[end - 1 - back]);
        size_t length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 1;
        if (length > back + 1) end -= back + 1;
    }
    std::string out = carry.substr(0, end);
    carry.erase(0, end);
    return out;
}

#ifdef _WIN32

class StreamChannel final : public Channel {
public:
    StreamChannel(int in, int out) : in_(in), out_(out) {}

protected:
    long receive(char* buffer, size_t size) override {
        return _read(in_, buffer, static_cast<unsigned>(std::min<size_t>(size, 1 << 20)));
    }
    bool send(const char* data, size_t size) override {
        while (size > 0) {
            int written = _write(out_, data, static_cast<unsigned>(std::min<size_t>(size, 1 << 20)));
            if (written <= 0) return false;
            data += written;
            size -= static_cast<size_t>(written);
        }
        return true;
    }

private:
    int in_, out_;
};

class SocketChannel final : public Channel {
public:
    explicit SocketChannel(SOCKET socket) : socket_(socket) {}
    ~SocketChannel() override { closesocket(socket_); }

protected:
    long receive(char* buffer, size_t size) override {
        return recv(socket_, buffer, static_cast<int>(std::min<size_t>(size, 1 << 20)), 0);
    }
    bool send(const char* data, size_t size) override {
        while (size > 0) {
            int written = ::send(socket_, data, static_cast<int>(std::min<size_t>(size, 1 << 20)), 0);
            if (written <= 0) return false;
            data += written;
            size -= static_cast<size_t>(written);
        }
        return true;
    }

private:
    SOCKET socket_;
};

#else

class StreamChannel final : public Channel {
public:
    StreamChannel(int in, int out) : in_(in), out_(out) {}

protected:
    long receive(char* buffer, size_t size) override { return static_cast<long>(::read(in_, buffer, size)); }
    bool send(const char* data, size_t size) override {
        while (size > 0) {
            ssize_t written = ::write(out_, data, size);
            if (written <= 0) return false;
            data += written;
            size -= static_cast<size_t>(written);
        }
        return true;
    }

private:
    int in_, out_;
};

class SocketChannel final : public Channel {
public:
    explicit SocketChannel(int socket) : socket_(socket) {}
    ~SocketChannel() override { ::close(socket_); }

protected:
    long receive(char* buffer, size_t size) override { return static_cast<long>(::recv(socket_, buffer, size, 0)); }
    bool send(const char* data, size_t size) override {
        while (size > 0) {
            ssize_t written = ::send(socket_, data, size, MSG_NOSIGNAL);
            if (written <= 0) return false;
            data += written;
            size -= static_cast<size_t>(written);
        }
        return true;
    }

private:
    int socket_;
};

#endif

} // namespace

bool Channel::read(std::string& message) {
    char buffer[65536];
    for (;;) {
        auto headerEnd = pending_.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            size_t length = 0;
            bool found = false;
            size_t lineStart = 0;
            while (lineStart < headerEnd) {
                size_t lineEnd = pending_.find("\r\n", lineStart);
                std::string line = pending_.substr(lineStart, lineEnd - lineStart);
                const std::string prefix = "Content-Length:";
                if (line.compare(0, prefix.size(), prefix) == 0) {
                    try {
                        length = std::stoul(line.substr(prefix.size()));
                        found = true;
                    } catch (const std::exception&) {
                        return false;
                    }
                }
                lineStart = lineEnd + 2;
            }
            if (!found || length > maxMessage) return false;
            size_t bodyStart = headerEnd + 4;
            if (pending_.size() >= bodyStart + length) {
                message = pending_.substr(bodyStart, length);
                pending_.erase(0, bodyStart + length);
                return true;
            }
        } else if (pending_.size() > 8192) {
            return false;
        }
        long received = receive(buffer, sizeof buffer);
        if (received <= 0) return false;
        pending_.append(buffer, static_cast<size_t>(received));
    }
}

void Channel::write(const std::string& message) {
    std::lock_guard<std::mutex> lock(writing_);
    std::string framed = "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;
    send(framed.data(), framed.size());
}

std::unique_ptr<Channel> Channel::standardStreams(std::function<void(const std::string&)> onOutput) {
    std::cout.flush();
    std::fflush(stdout);
#ifdef _WIN32
    int in = _dup(0), out = _dup(1);
    int pipe[2];
    if (in < 0 || out < 0 || _pipe(pipe, 65536, _O_BINARY | _O_NOINHERIT) != 0)
        throw std::runtime_error("Debug Error: cannot redirect the program's standard streams");
    _setmode(in, _O_BINARY);
    _setmode(out, _O_BINARY);
    _dup2(pipe[1], 1);
    _close(pipe[1]);
    int nothing = _open("NUL", _O_RDONLY);
    if (nothing >= 0) {
        _dup2(nothing, 0);
        _close(nothing);
    }
    // Programs started by os_run write to the handles, not to the C runtime's descriptors.
    SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(1)));
    SetStdHandle(STD_INPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(0)));
    int source = pipe[0];
    auto readOutput = [source](char* buffer, size_t size) { return static_cast<long>(_read(source, buffer, static_cast<unsigned>(size))); };
#else
    int in = ::dup(0), out = ::dup(1);
    int pipe[2];
    if (in < 0 || out < 0 || ::pipe(pipe) != 0)
        throw std::runtime_error("Debug Error: cannot redirect the program's standard streams");
    // Programs started by os_run must not inherit the protocol.
    ::fcntl(in, F_SETFD, FD_CLOEXEC);
    ::fcntl(out, F_SETFD, FD_CLOEXEC);
    ::fcntl(pipe[0], F_SETFD, FD_CLOEXEC);
    ::dup2(pipe[1], 1);
    ::close(pipe[1]);
    int nothing = ::open("/dev/null", O_RDONLY);
    if (nothing >= 0) {
        ::dup2(nothing, 0);
        ::close(nothing);
    }
    int source = pipe[0];
    auto readOutput = [source](char* buffer, size_t size) { return static_cast<long>(::read(source, buffer, size)); };
#endif
    // Output reaches the editor line by line rather than when a large buffer fills.
    std::setvbuf(stdout, nullptr, _IOLBF, 4096);
    std::thread([readOutput, onOutput] {
        std::string carry;
        char buffer[8192];
        for (;;) {
            long received = readOutput(buffer, sizeof buffer);
            if (received <= 0) return;
            std::string text = completeCharacters(carry, buffer, static_cast<size_t>(received));
            if (!text.empty()) onOutput(text);
        }
    }).detach();
    return std::make_unique<StreamChannel>(in, out);
}

std::unique_ptr<Channel> Channel::connect(const std::string& host, int port) {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("Debug Error: sockets are unavailable");
#endif
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found)
        throw std::runtime_error("Debug Error: cannot resolve " + host);
    std::unique_ptr<Channel> channel;
    for (addrinfo* address = found; address && !channel; address = address->ai_next) {
#ifdef _WIN32
        SOCKET socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == INVALID_SOCKET) continue;
        if (::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) {
            SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0);
            channel = std::make_unique<SocketChannel>(socket);
        } else {
            closesocket(socket);
        }
#else
        int socket = ::socket(address->ai_family, address->ai_socktype | SOCK_CLOEXEC, address->ai_protocol);
        if (socket < 0) continue;
        if (::connect(socket, address->ai_addr, address->ai_addrlen) == 0) {
            channel = std::make_unique<SocketChannel>(socket);
        } else {
            ::close(socket);
        }
#endif
    }
    freeaddrinfo(found);
    if (!channel) throw std::runtime_error("Debug Error: cannot connect to the editor at " + host + ":" + std::to_string(port));
    return channel;
}

} // namespace foxlang::debug
