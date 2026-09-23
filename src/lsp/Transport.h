#pragma once
#include <string>
#include <iostream>

namespace foxlang {
namespace lsp {

class Transport {
public:
    Transport() = default;

    // Read single LSP message from stream (Content-Length framing)
    bool readMessage(std::istream& in, std::string& message);

    // Send single LSP message to stream (Content-Length framing)
    void sendMessage(std::ostream& out, const std::string& message);

    // Logging to stderr ONLY (stdout is strictly for LSP protocol)
    static void log(const std::string& msg);
};

} // namespace lsp
} // namespace foxlang
