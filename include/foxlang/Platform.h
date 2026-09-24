#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include "foxlang/Context.h"

namespace foxlang {
namespace platform {

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

// HTTP server. Routes, the request being handled and the response being built
// live on the root context, so handlers read them through builtins.
struct HttpRequest {
    std::string method, path, query, body;
    std::map<std::string, std::string> headers; // names in lower case
};

struct ServerState {
    std::map<std::string, std::string> routes; // "METHOD /path" -> handler function
    bool stopRequested = false;
    HttpRequest request;
    int status = 200;
    std::string contentType;
    std::string response;
};
ServerState& serverState(Context& ctx);

void runHttpServer(int port, Context& rootCtx, const std::function<bool()>& shouldStop,
                   const std::string& certificate = "", const std::string& privateKey = "");

} // namespace platform
} // namespace foxlang
