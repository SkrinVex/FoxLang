#pragma once
#include <string>

enum class TokenType {
    NUMBER, STRING_LITERAL, 
    PLUS, MINUS, STAR, SLASH, MOD, 
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, 
    SEMICOLON, COMMA, ASSIGN, 
    EQ, NEQ, LT, GT, 
    
    // Ключевые слова
    PRINT, INPUT, ROUND, RANDOM, FOX,
    INT_KW, STRING_KW, VOID_KW, // Типы данных
    WHILE, IF, ELSE,
    ARRAY, SET, GET, SIZE, 
    INCLUDE, 
    
    // НОВЫЕ: возврат и глобальные
    RETURN, GLOBAL,
    
    IDENTIFIER, 
    END, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};