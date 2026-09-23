#include "foxlang/Platform.h"
#include "foxlang/AST.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
    #include <conio.h>
    #include <cstdio>
    #include <windows.h>
    #define popen _popen
    #define pclose _pclose
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif

namespace foxlang {
namespace platform {

std::string getch() {
#ifdef _WIN32
    return std::string(1, static_cast<char>(_getch()));
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return "";
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (ch == EOF) return "";
    return std::string(1, static_cast<char>(ch));
#endif
}

bool kbhit() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    struct termios oldt, newt;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    timeval tv{0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ready > 0;
#endif
}

bool setEnvVar(const std::string& key, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(key.c_str(), value.c_str()) == 0;
#else
    return setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
}

std::string getEnvVar(const std::string& key) {
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : "";
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '\"') out += "\\\"";
        else if (ch == '\\') out += "\\\\";
        else out += ch;
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') out += "'\\''";
        else out += ch;
    }
    out += "'";
    return out;
#endif
}

int executeCommandCapture(const std::string& cmd, std::string& output) {
    output.clear();
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int status = pclose(pipe);
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return status;
#endif
}

int tcpConnect(const std::string& host, int port) {
#ifdef _WIN32
    (void)host; (void)port;
    return -1;
#else
    std::string portStr = std::to_string(port);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return -1;

    int fd = -1;
    for (addrinfo* p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
#endif
}

int tcpSend(int fd, const std::string& data) {
#ifdef _WIN32
    (void)fd; (void)data;
    return -1;
#else
    ssize_t sent = send(fd, data.data(), data.size(), 0);
    return sent < 0 ? -1 : static_cast<int>(sent);
#endif
}

std::string tcpRecv(int fd, int maxBytes) {
#ifdef _WIN32
    (void)fd; (void)maxBytes;
    return "";
#else
    int bufSize = std::max(1, maxBytes);
    std::string out(bufSize, '\0');
    ssize_t n = recv(fd, out.data(), out.size(), 0);
    if (n <= 0) return "";
    out.resize(static_cast<size_t>(n));
    return out;
#endif
}

bool tcpClose(int fd) {
#ifdef _WIN32
    (void)fd;
    return false;
#else
    return close(fd) == 0;
#endif
}

std::string dnsLookup(const std::string& host) {
#ifdef _WIN32
    (void)host;
    return "";
#else
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) return "";
    char buf[INET6_ADDRSTRLEN] = {0};
    std::string out;
    for (addrinfo* p = res; p && out.empty(); p = p->ai_next) {
        void* addr = (p->ai_family == AF_INET)
            ? static_cast<void*>(&reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr)
            : static_cast<void*>(&reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_addr);
        if (inet_ntop(p->ai_family, addr, buf, sizeof(buf))) out = buf;
    }
    freeaddrinfo(res);
    return out;
#endif
}

bool isHttpServerSupported() {
#ifdef _WIN32
    return false;
#else
    return true;
#endif
}

void runHttpServer(int port, Context& rootCtx, const std::function<bool()>& shouldStop) {
#ifdef _WIN32
    (void)port; (void)rootCtx; (void)shouldStop;
    throw std::runtime_error("HTTP server runtime is currently available on Linux/POSIX builds");
#else
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) throw std::runtime_error("HTTP Server Error: socket() failed");

    int yes = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(serverFd);
        throw std::runtime_error("HTTP Server Error: bind() failed on port " + std::to_string(port));
    }
    if (listen(serverFd, 32) < 0) {
        close(serverFd);
        throw std::runtime_error("HTTP Server Error: listen() failed");
    }

    std::cerr << "[HTTP] Listening on 0.0.0.0:" << port << std::endl;

    for (;;) {
        if (shouldStop && shouldStop()) break;
        if (rootCtx.variables.count("__server_stop_requested") && rootCtx.variables["__server_stop_requested"].value == "true") {
            break;
        }

        // Use select with a 250ms timeout so we can react to shouldStop or stop signals
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(serverFd, &readFds);
        timeval tv{0, 250000}; // 250ms

        int ready = select(serverFd + 1, &readFds, nullptr, nullptr, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ready == 0) {
            // Timeout expired; check stop condition and loop
            continue;
        }

        int client = accept(serverFd, nullptr, nullptr);
        if (client < 0) continue;

        std::string req;
        char buf[4096];
        ssize_t n = 0;
        size_t headerEnd = std::string::npos;
        size_t contentLength = 0;

        while ((n = recv(client, buf, sizeof(buf), 0)) > 0) {
            req.append(buf, static_cast<size_t>(n));
            headerEnd = req.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                size_t cl = req.find("Content-Length:");
                if (cl == std::string::npos) cl = req.find("content-length:");
                if (cl != std::string::npos && cl < headerEnd) {
                    try {
                        contentLength = static_cast<size_t>(std::stoul(req.substr(cl + 15)));
                    } catch (...) {
                        contentLength = 0;
                    }
                }
                if (req.size() >= headerEnd + 4 + contentLength) break;
            }
            if (req.size() > 1024 * 1024) break; // 1MB max body limit
        }

        std::istringstream first(req.substr(0, req.find("\r\n")));
        std::string method, path, proto;
        first >> method >> path >> proto;

        size_t q = path.find('?');
        if (q != std::string::npos) path = path.substr(0, q);

        std::string body = headerEnd == std::string::npos ? "" : req.substr(headerEnd + 4, contentLength);

        rootCtx.variables["__http_method"] = {"string", method};
        rootCtx.variables["__http_path"] = {"string", path};
        rootCtx.variables["__http_body"] = {"string", body};
        rootCtx.variables["__http_status"] = {"int", "200"};
        rootCtx.variables["__http_response"] = {"string", ""};

        std::string key = "__route_" + method + "_" + path;
        int status = 404;
        std::string response = "{\"error\":\"Not Found\"}";

        if (rootCtx.variables.count(key)) {
            std::string handlerName = rootCtx.variables[key].value;
            auto fn = rootCtx.getFunc(handlerName);
            if (fn) {
                auto def = static_cast<FuncDefNode*>(fn.get());
                Context scope;
                scope.parent = &rootCtx;
                scope.interpreter = rootCtx.interpreter;
                try {
                    def->body->eval(scope);
                } catch (const ReturnValue&) {
                    // Normal return
                } catch (const std::exception& e) {
                    std::cerr << "[HTTP ERROR] Handler exception: " << e.what() << std::endl;
                }
                try {
                    status = std::stoi(rootCtx.variables["__http_status"].value);
                } catch (...) {
                    status = 200;
                }
                response = rootCtx.variables["__http_response"].value;
            } else {
                status = 500;
                response = "{\"error\":\"Handler not found\"}";
            }
        }

        std::string reason = (status == 200) ? "OK" : (status == 404) ? "Not Found" : "Error";
        std::string out = "HTTP/1.1 " + std::to_string(status) + " " + reason +
                          "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                          std::to_string(response.size()) + "\r\nConnection: close\r\n\r\n" + response;
        send(client, out.data(), out.size(), 0);
        close(client);

        if ((rootCtx.variables.count("__server_stop_requested") && rootCtx.variables["__server_stop_requested"].value == "true") || (shouldStop && shouldStop())) {
            break;
        }
    }

    close(serverFd);
#endif
}

} // namespace platform
} // namespace foxlang
