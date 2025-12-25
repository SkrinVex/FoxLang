#pragma once
#include <string>

enum class TokenType {
    NUMBER, STRING,
    PLUS, MINUS, STAR, SLASH,
    LPAREN, RPAREN,
    LBRACE, RBRACE, SEMICOLON,
    PRINT, INPUT,
    ROUND, FOX, RANDOM, // Новые команды
    IDENTIFIER, // Для неизвестных слов
    END, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};
