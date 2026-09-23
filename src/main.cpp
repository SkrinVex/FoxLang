#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Lexer.h"
#include "Parser.h"

static constexpr const char* FOX_VERSION = "5.3.0";

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "FoxLang " << FOX_VERSION << std::endl;
        return 0;
    }

    if (argc < 2) {
        std::cout << "FoxLang " << FOX_VERSION << "\nUsage: foxlang <script.fox>\n"
                  << "       foxlang --version" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "FoxLang: could not open file '" << argv[1] << "'" << std::endl;
        return 1;
    }

    try {
        std::stringstream buffer;
        buffer << file.rdbuf();

        Lexer lexer(buffer.str());
        Parser parser(lexer.tokenize());
        parser.currentFile = argv[1];
        parser.run();
    } catch (const std::exception& e) {
        std::cerr << "FoxLang: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
