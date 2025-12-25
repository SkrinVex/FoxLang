#include <iostream>
#include <fstream>
#include <sstream>
#include "Lexer.h"
#include "Parser.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: foxlang <script.fox>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: File not found!" << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    Lexer lexer(buffer.str());
    auto tokens = lexer.tokenize();

    Parser parser(tokens);
    parser.run();

    return 0;
}
