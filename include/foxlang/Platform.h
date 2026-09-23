#pragma once
#include <string>
#include <vector>
#include <functional>
#include "foxlang/Context.h"

namespace foxlang {
namespace platform {

// Console / Terminal input
std::string getch();
bool kbhit();

// Environment variables
bool setEnvVar(const std::string& key, const std::string& value);
std::string getEnvVar(const std::string& key);

// In-process HTTP(S); TLS and HTTP implementation are linked into the runtime.
std::string httpRequest(const std::string& method, const std::string& url,
                        const std::string& body = "", const std::string& contentType = "application/json",
                        bool failOnHttpError = true);
const char* thirdPartyLicenses();

// Network primitives (POSIX and Winsock; int values are managed socket handles)
int tcpConnect(const std::string& host, int port);
int tcpSend(int fd, const std::string& data);
std::string tcpRecv(int fd, int maxBytes);
bool tcpClose(int fd);
std::string dnsLookup(const std::string& host);

// HTTP Server
bool isHttpServerSupported();
void runHttpServer(int port, Context& rootCtx, const std::function<bool()>& shouldStop);

} // namespace platform
} // namespace foxlang
