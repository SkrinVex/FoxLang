#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    Token consume(TokenType type);
    std::unique_ptr<Node> primary();
    std::unique_ptr<Node> multiplication();
    std::unique_ptr<Node> expression();
    void statement(); // Обработка отдельной команды с точкой с запятой
    void block();     // Обработка блока { ... }

public:
    Parser(std::vector<Token> t);
    void run();
};
