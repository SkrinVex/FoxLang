#pragma once
#include <string>
#include <iostream>
#include "Json.h"
#include "Transport.h"
#include "DocumentManager.h"

namespace foxlang {
namespace lsp {

class LspServer {
public:
    explicit LspServer(Transport& transport);

    // Process a single JSON-RPC message
    void processMessage(const std::string& rawMessage, std::ostream& out);

    // Main event loop (reads from std::cin, writes to std::cout)
    int run(std::istream& in, std::ostream& out);

    bool shouldExit() const { return exitRequested; }

private:
    Transport& transport;
    DocumentManager docManager;
    bool isInitialized = false;
    bool isShutdown = false;
    bool exitRequested = false;

    void handleRequest(const JsonValue& msg, std::ostream& out);
    void handleNotification(const JsonValue& msg, std::ostream& out);

    void sendResponse(const JsonValue& id, const JsonValue& result, std::ostream& out);
    void sendError(const JsonValue& id, int code, const std::string& message, std::ostream& out);
    void sendNotification(const std::string& method, const JsonValue& params, std::ostream& out);

    void publishDiagnostics(const std::string& uri, std::ostream& out);
};

} // namespace lsp
} // namespace foxlang
