#include "Transport.h"
#include <sstream>

namespace foxlang {
namespace lsp {

bool Transport::readMessage(std::istream& in, std::string& message) {
    message.clear();
    std::string headerLine;
    size_t contentLength = 0;

    // Read headers until blank line
    while (std::getline(in, headerLine)) {
        // Strip trailing \r if present
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }

        if (headerLine.empty()) {
            // End of headers
            break;
        }

        const std::string clPrefix = "Content-Length: ";
        if (headerLine.compare(0, clPrefix.size(), clPrefix) == 0) {
            try {
                contentLength = std::stoul(headerLine.substr(clPrefix.size()));
            } catch (...) {
                contentLength = 0;
            }
        }
    }

    if (contentLength == 0) {
        return false;
    }

    // Read message body
    message.resize(contentLength);
    in.read(&message[0], contentLength);
    return in.gcount() == static_cast<std::streamsize>(contentLength);
}

void Transport::sendMessage(std::ostream& out, const std::string& message) {
    out << "Content-Length: " << message.size() << "\r\n\r\n" << message << std::flush;
}

void Transport::log(const std::string& msg) {
    std::cerr << "[foxlang-lsp] " << msg << std::endl;
}

} // namespace lsp
} // namespace foxlang
