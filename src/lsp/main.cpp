#include <iostream>
#include <cstdio>
#include <string>
#include "foxlang/FoxLang.h"
#include "Transport.h"
#include "LspServer.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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

    // LSP framing counts bytes and supplies its own CRLF. Windows CRT text
    // translation would turn CRLF into CRCRLF and corrupt Content-Length frames.
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1) {
        std::cerr << "foxlang-lsp: cannot configure binary standard streams\n";
        return 1;
    }
#endif
    foxlang::lsp::Transport transport;
    foxlang::lsp::LspServer server(transport);
    return server.run(std::cin, std::cout);
}
