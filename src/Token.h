#pragma once
#include <string>

enum class TokenType {
    NUMBER, STRING_LITERAL, 
    PLUS, MINUS, STAR, SLASH, 
    LPAREN, RPAREN, LBRACE, RBRACE, 
    SEMICOLON, COMMA, ASSIGN, // =, , ;
    
    // Ключевые слова
    PRINT, INPUT, ROUND, RANDOM, FOX,
    INT_KW, STRING_KW, VOID_KW, // int, string, void
    
    IDENTIFIER, // Имена переменных и функций
    END, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};