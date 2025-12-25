#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    Context globalContext; 

    Token consume(TokenType type);
    std::unique_ptr<Node> primary();
    std::unique_ptr<Node> multiplication();
    std::unique_ptr<Node> expression();
    
    std::unique_ptr<Node> statement();
    // ИЗМЕНЕНИЕ: теперь возвращает unique_ptr
    std::unique_ptr<BlockNode> parseBlock(); 

public:
    Parser(std::vector<Token> t);
    void run(); 
};