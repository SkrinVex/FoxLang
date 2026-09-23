#pragma once
#include <string>
#include <vector>
#include "foxlang/Token.h"

namespace foxlang {

class Lexer {
public:
    explicit Lexer(std::string src);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos = 0;
    int line = 1;
    int column = 1;
};

} // namespace foxlang

using foxlang::Lexer;
