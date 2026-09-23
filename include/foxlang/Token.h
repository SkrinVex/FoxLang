#pragma once
#include <string>

namespace foxlang {

enum class TokenType {
    NUMBER, STRING_LITERAL, 
    PLUS, MINUS, STAR, SLASH, MOD, INC,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, 
    SEMICOLON, COMMA, ASSIGN, DOT, COLON,
    EQ, NEQ, LT, GT, LTE, GTE, AND, OR, NOT, 
    PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN,
    
    // Ключевые слова
    PRINT, INPUT, ROUND, RANDOM, FOX, READ_FILE, JSON_GET, STR_CONTAINS, STR_TO_INT,
    INT_KW, FLOAT_KW, STRING_KW, BOOL_KW, VOID_KW,
    TRUE_KW, FALSE_KW,
    WHILE, FOR, IF, ELSE, SWITCH, CASE, DEFAULT,
    ARRAY, SET, GET, SIZE, 
    INCLUDE, USING,
    
    RETURN, GLOBAL,
    
    BREAK, CONTINUE, WAIT,
    
    HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE,
    
    SERVER_START, SERVER_STOP, ROUTE_GET, ROUTE_POST, SEND_RESPONSE,
    
    GETCH, KBHIT,
    
    IDENTIFIER, 
    END, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line = 1;
    int column = 1;
};

const char* tokenTypeName(TokenType type);

} // namespace foxlang

// Backwards compatibility alias in global namespace if needed
using foxlang::TokenType;
using foxlang::Token;
