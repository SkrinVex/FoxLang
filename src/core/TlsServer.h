#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace foxlang::platform {
// The HTTP parser/routes use the same byte stream for plain and TLS connections.
// Callbacks return bytes, zero for EOF, or a negative result for an I/O failure.
struct TlsIo {
    void* context;
    int (*read)(void*, unsigned char*, std::size_t);
    int (*write)(void*, const unsigned char*, std::size_t);
};

class TlsSession;
class TlsServer {
    struct State;
    std::unique_ptr<State> state;
    friend class TlsSession;
public:
    TlsServer(const std::string& certificate, const std::string& privateKey);
    ~TlsServer();
    TlsServer(const TlsServer&) = delete;
    TlsServer& operator=(const TlsServer&) = delete;
};

class TlsSession {
    struct State;
    std::unique_ptr<State> state;
public:
    TlsSession(TlsServer& server, TlsIo io);
    ~TlsSession();
    bool handshake();
    int read(unsigned char* bytes, std::size_t size);
    int write(const unsigned char* bytes, std::size_t size);
    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;
};
}
