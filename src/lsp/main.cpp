#include <iostream>
#include <string>
#include "foxlang/FoxLang.h"
#include "Transport.h"
#include "LspServer.h"

int main(int argc, char* argv[]) {
    const std::string version = foxlang::Interpreter::getVersion();

    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "foxlang-lsp " << version << std::endl;
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "foxlang-lsp " << version << "\n\n"
                      << "Usage:\n"
                      << "  foxlang-lsp            Run Language Server over standard I/O (JSON-RPC)\n"
                      << "  foxlang-lsp --version  Show version\n"
                      << "  foxlang-lsp --help     Show this help\n\n"
                      << "Editor integration:\n"
                      << "  Communicates via JSON-RPC over stdin/stdout with Content-Length framing.\n"
                      << "  All diagnostic logging is sent to stderr.\n";
            return 0;
        }
    }

    foxlang::lsp::Transport transport;
    foxlang::lsp::LspServer server(transport);
    return server.run(std::cin, std::cout);
}
