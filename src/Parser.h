#pragma once
#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>
#include <string> // Не забудь

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    
public:
    Context globalContext;
    
    // НОВЫЕ ПОЛЯ ДЛЯ ПУТЕЙ И ИМПОРТА
    std::string currentFile; // Путь к файлу, который сейчас парсится
    bool importMode = false; // Если true, то выполняем только объявления (vars/funcs)

    Parser(std::vector<Token> t);
    
    Token consume(TokenType type);
    std::unique_ptr<Node> primary();
    std::unique_ptr<Node> multiplication();
    std::unique_ptr<Node> expression();
    std::unique_ptr<Node> comparison();

    std::unique_ptr<Node> statement();
    std::unique_ptr<BlockNode> parseBlock();
    
    void run(); 
};