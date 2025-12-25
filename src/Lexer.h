#pragma once
#include <vector>
#include <string>
#include "Token.h"

class Lexer {
    std::string source;
    size_t pos = 0;
    int line = 1;

public:
    Lexer(std::string src);
    std::vector<Token> tokenize();
};
