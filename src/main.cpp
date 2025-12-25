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

    // 1. Читаем файл
    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();

    // 2. Запускаем конвейер
    Lexer lexer(code);
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(tokens);
    
    // ВАЖНО: Передаем имя файла, чтобы парсер знал, где он находится
    parser.currentFile = argv[1]; 

    parser.run();

    return 0;
}