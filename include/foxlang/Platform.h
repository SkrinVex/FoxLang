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

// Shell command execution (used by HTTP curl client)
std::string shellQuote(const std::string& value);
int executeCommandCapture(const std::string& cmd, std::string& output);

// Network primitives (POSIX TCP / DNS)
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
