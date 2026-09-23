#include "foxlang/Platform.h"
#include "foxlang/AST.h"
#include <algorithm>
#include <charconv>
#include <climits>
#include <cctype>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace foxlang::platform {
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalidSocket = INVALID_SOCKET;
struct NetworkRuntime {
    NetworkRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("Network Error: WSAStartup failed");
    }
    ~NetworkRuntime() { WSACleanup(); }
};
void initialize() { static NetworkRuntime runtime; }
void closeSocket(NativeSocket fd) { closesocket(fd); }
bool interrupted() { return WSAGetLastError() == WSAEINTR; }
#else
using NativeSocket = int;
constexpr NativeSocket invalidSocket = -1;
void initialize() {}
void closeSocket(NativeSocket fd) { close(fd); }
bool interrupted() { return errno == EINTR; }
#endif

struct Socket {
    NativeSocket fd;
    explicit Socket(NativeSocket value) : fd(value) {}
    ~Socket() { if (fd != invalidSocket) closeSocket(fd); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};

struct Registry {
    std::mutex mutex;
    std::map<int, std::unique_ptr<Socket>> sockets;
    int next = 1;
};
Registry& registry() { static Registry sockets; return sockets; }

void timeouts(NativeSocket fd) {
#ifdef _WIN32
    DWORD timeout = 5000;
#else
    timeval timeout{5, 0};
#endif
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
}

bool sendAll(NativeSocket fd, const std::string& bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        int count = static_cast<int>(std::min<std::size_t>(bytes.size() - offset, INT_MAX));
#ifdef MSG_NOSIGNAL
        auto n = send(fd, bytes.data() + offset, count, MSG_NOSIGNAL);
#else
        auto n = send(fd, bytes.data() + offset, count, 0);
#endif
        if (n < 0 && interrupted()) continue;
        if (n <= 0) return false;
        offset += static_cast<std::size_t>(n);
    }
    return true;
}

int readable(NativeSocket fd) {
#ifndef _WIN32
    if (fd >= FD_SETSIZE) throw std::runtime_error("Network Error: socket exceeds select capacity");
#endif
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeval timeout{0, 250000};
#ifdef _WIN32
    return select(0, &fds, nullptr, nullptr, &timeout);
#else
    return select(fd + 1, &fds, nullptr, nullptr, &timeout);
#endif
}

std::string trim(std::string value) {
    auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}

struct Request {
    std::string method, path, body;
    int error = 0;
};

Request readRequest(NativeSocket fd) {
    constexpr std::size_t maxHeaders = 64 * 1024, maxBody = 1024 * 1024;
    Request request;
    std::string bytes;
    char buffer[4096];
    std::size_t headerEnd = std::string::npos, length = 0;
    while (true) {
        auto n = recv(fd, buffer, sizeof(buffer), 0);
        if (n < 0 && interrupted()) continue;
        if (n <= 0) { request.error = 400; return request; }
        bytes.append(buffer, static_cast<std::size_t>(n));
        if (headerEnd == std::string::npos) {
            headerEnd = bytes.find("\r\n\r\n");
            if ((headerEnd == std::string::npos && bytes.size() > maxHeaders) ||
                (headerEnd != std::string::npos && headerEnd > maxHeaders)) {
                request.error = 431; return request;
            }
            if (headerEnd == std::string::npos) continue;
            std::istringstream headers(bytes.substr(0, headerEnd));
            std::string line, protocol, extra;
            std::getline(headers, line);
            std::istringstream first(line);
            first >> request.method >> request.path >> protocol;
            if (request.method.empty() || request.path.empty() || request.path[0] != '/' ||
                (protocol != "HTTP/1.1" && protocol != "HTTP/1.0") || (first >> extra)) {
                request.error = 400; return request;
            }
            bool hasLength = false;
            while (std::getline(headers, line)) {
                auto colon = line.find(':');
                if (colon == std::string::npos) { request.error = 400; return request; }
                auto key = line.substr(0, colon);
                auto value = trim(line.substr(colon + 1));
                std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (key == "transfer-encoding") { request.error = 501; return request; }
                if (key == "content-length") {
                    auto parsed = std::from_chars(value.data(), value.data() + value.size(), length);
                    if (hasLength || value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                        request.error = 400; return request;
                    }
                    if (length > maxBody) { request.error = 413; return request; }
                    hasLength = true;
                }
            }
        }
        if (bytes.size() >= headerEnd + 4 + length) break;
    }
    request.path = request.path.substr(0, request.path.find('?'));
    request.body = bytes.substr(headerEnd + 4, length);
    return request;
}

void respond(NativeSocket fd, int status, const std::string& response) {
    std::string reason = status == 200 ? "OK" : status == 201 ? "Created" : "Error";
    sendAll(fd, "HTTP/1.1 " + std::to_string(status) + " " + reason +
        "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
        std::to_string(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
}
}

int tcpConnect(const std::string& host, int port) {
    if (port < 1 || port > 65535 || host.find('\0') != std::string::npos) return -1;
    initialize();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* raw = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &raw)) return -1;
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
    for (auto* address = raw; address; address = address->ai_next) {
        auto socket = std::make_unique<Socket>(::socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (socket->fd == invalidSocket) continue;
        if (connect(socket->fd, address->ai_addr, static_cast<int>(address->ai_addrlen))) continue;
        timeouts(socket->fd);
        auto& sockets = registry();
        std::lock_guard<std::mutex> lock(sockets.mutex);
        if (sockets.next == INT_MAX) return -1;
        int id = sockets.next++;
        sockets.sockets.emplace(id, std::move(socket));
        return id;
    }
    return -1;
}

int tcpSend(int id, const std::string& data) {
    if (data.size() > INT_MAX) return -1;
    auto& sockets = registry();
    std::lock_guard<std::mutex> lock(sockets.mutex);
    auto socket = sockets.sockets.find(id);
    if (socket == sockets.sockets.end() || !sendAll(socket->second->fd, data)) return -1;
    return static_cast<int>(data.size());
}

std::string tcpRecv(int id, int maxBytes) {
    if (maxBytes < 1 || maxBytes > 16 * 1024 * 1024) return "";
    auto& sockets = registry();
    std::lock_guard<std::mutex> lock(sockets.mutex);
    auto socket = sockets.sockets.find(id);
    if (socket == sockets.sockets.end()) return "";
    std::string bytes(static_cast<std::size_t>(maxBytes), '\0');
    auto n = recv(socket->second->fd, bytes.data(), maxBytes, 0);
    if (n <= 0) return "";
    bytes.resize(static_cast<std::size_t>(n));
    return bytes;
}

bool tcpClose(int id) {
    auto& sockets = registry();
    std::lock_guard<std::mutex> lock(sockets.mutex);
    return sockets.sockets.erase(id) != 0;
}

std::string dnsLookup(const std::string& host) {
    if (host.find('\0') != std::string::npos) return "";
    initialize();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* raw = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &raw)) return "";
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
    for (auto* entry = raw; entry; entry = entry->ai_next) {
        char text[INET6_ADDRSTRLEN]{};
        const void* addr = entry->ai_family == AF_INET
            ? static_cast<void*>(&reinterpret_cast<sockaddr_in*>(entry->ai_addr)->sin_addr)
            : static_cast<void*>(&reinterpret_cast<sockaddr_in6*>(entry->ai_addr)->sin6_addr);
        if (inet_ntop(entry->ai_family, addr, text, sizeof(text))) return text;
    }
    return "";
}

bool isHttpServerSupported() { return true; }

void runHttpServer(int port, Context& rootCtx, const std::function<bool()>& shouldStop) {
    if (port < 1 || port > 65535) throw std::runtime_error("HTTP Server Error: port must be 1..65535");
    initialize();
    Socket server(::socket(AF_INET, SOCK_STREAM, 0));
    if (server.fd == invalidSocket) throw std::runtime_error("HTTP Server Error: socket() failed");
    int yes = 1;
#ifdef _WIN32
    setsockopt(server.fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
    setsockopt(server.fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(server.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))) throw std::runtime_error("HTTP Server Error: bind() failed on port " + std::to_string(port));
    if (listen(server.fd, 32)) throw std::runtime_error("HTTP Server Error: listen() failed");
    std::cerr << "[HTTP] Listening on 0.0.0.0:" << port << std::endl;
    auto stopped = [&] {
        auto flag = rootCtx.variables.find("__server_stop_requested");
        return (shouldStop && shouldStop()) || (flag != rootCtx.variables.end() && flag->second.value == "true");
    };
    while (!stopped()) {
        int ready = readable(server.fd);
        if (ready < 0 && interrupted()) continue;
        if (ready < 0) throw std::runtime_error("HTTP Server Error: select() failed");
        if (!ready) continue;
        Socket client(accept(server.fd, nullptr, nullptr));
        if (client.fd == invalidSocket) continue;
        timeouts(client.fd);
        auto request = readRequest(client.fd);
        if (request.error) { respond(client.fd, request.error, "{\"error\":\"Invalid request\"}"); continue; }
        rootCtx.variables["__http_method"] = {"string", request.method};
        rootCtx.variables["__http_path"] = {"string", request.path};
        rootCtx.variables["__http_body"] = {"string", request.body};
        rootCtx.variables["__http_status"] = {"int", "200"};
        rootCtx.variables["__http_response"] = {"string", ""};
        int status = 404;
        std::string response = "{\"error\":\"Not Found\"}";
        auto route = rootCtx.variables.find("__route_" + request.method + "_" + request.path);
        if (route != rootCtx.variables.end()) {
            auto function = rootCtx.getFunc(route->second.value);
            auto* definition = dynamic_cast<FuncDefNode*>(function.get());
            if (definition) {
                Context scope;
                scope.parent = &rootCtx;
                scope.interpreter = rootCtx.interpreter;
                bool failed = false;
                try { definition->body->eval(scope); }
                catch (const ReturnValue&) {}
                catch (const std::exception& error) {
                    failed = true;
                    std::cerr << "[HTTP ERROR] Handler exception: " << error.what() << std::endl;
                }
                if (failed) { status = 500; response = "{\"error\":\"Handler failed\"}"; }
                else {
                    try { status = std::stoi(rootCtx.variables["__http_status"].value); }
                    catch (...) { status = 500; }
                    if (status < 100 || status > 599) status = 500;
                    response = rootCtx.variables["__http_response"].value;
                }
            } else { status = 500; response = "{\"error\":\"Handler not found\"}"; }
        }
        respond(client.fd, status, response);
    }
}
} // namespace foxlang::platform
