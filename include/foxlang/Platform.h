#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <map>
#include "foxlang/Context.h"

namespace foxlang {
namespace platform {

// FoxLang keeps paths as UTF-8 text. Windows reads a narrow std::string as its ANSI
// code page, which cannot hold Cyrillic, so every conversion goes through these.
std::filesystem::path pathFromUtf8(const std::string& text);
std::string pathToUtf8(const std::filesystem::path& path);

// Console / Terminal input
std::string getch();
bool kbhit();

// Portion of this thread's native stack a recursing program may use, in bytes.
size_t stackBudget();

// Environment variables
bool setEnvVar(const std::string& key, const std::string& value);
std::string getEnvVar(const std::string& key);

// In-process HTTP(S); TLS and HTTP implementation are linked into the runtime.
// Any HTTP status is a response; only a failed connection or TLS check throws.
struct HttpResponse {
    int status = 0;
    std::string body;
};
HttpResponse httpRequest(const std::string& method, const std::string& url,
                         const std::string& body = "", const std::string& contentType = "application/json");
const char* thirdPartyLicenses();

// Network primitives (POSIX and Winsock; int values are managed socket handles)
int tcpConnect(const std::string& host, int port);
int tcpSend(int fd, const std::string& data);
std::string tcpRecv(int fd, int maxBytes);
bool tcpClose(int fd);
std::string dnsLookup(const std::string& host);

// HTTP server. Routes, the request being handled and the reply being built live on
// the root context, so handlers read and write them through builtins.
struct HttpRequest {
    std::string method, path, query, body; // path is still percent-encoded
    std::map<std::string, std::string> headers; // names in lower case
    std::map<std::string, std::string> params;  // from :name and *name route segments
    std::string clientIp;
};

struct HttpReply {
    int status = 200;
    std::string contentType;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct HttpRoute {
    std::string method;
    std::string pattern;               // /users/:id, /files/*path or an exact path
    std::vector<std::string> segments; // pattern split at '/'
    std::string handler;
};

struct ServerState {
    std::vector<HttpRoute> routes;
    std::vector<std::pair<std::string, std::string>> staticMounts; // URL prefix -> directory
    std::string notFoundHandler;
    std::string corsOrigin;
    bool accessLog = false;
    std::size_t maxBody = 10 * 1024 * 1024;
    bool stopRequested = false;
    HttpRequest request;
    HttpReply reply;
};
ServerState& serverState(Context& ctx);

// Routing, static files and handlers for one parsed request. Sockets and TLS stay in
// the platform layer; this part is the same on every OS and testable without them.
HttpReply handleHttpRequest(ServerState& state, Context& rootCtx);

void runHttpServer(int port, Context& rootCtx, const std::function<bool()>& shouldStop,
                   const std::string& certificate = "", const std::string& privateKey = "");

} // namespace platform
} // namespace foxlang
