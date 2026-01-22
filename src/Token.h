#pragma once
#include <string>

enum class TokenType {
    NUMBER, STRING_LITERAL, 
    PLUS, MINUS, STAR, SLASH, MOD, INC,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, 
    SEMICOLON, COMMA, ASSIGN, DOT, COLON,
    EQ, NEQ, LT, GT, AND, OR, NOT, 
    
    // Ключевые слова
    PRINT, INPUT, ROUND, RANDOM, FOX, READ_FILE, JSON_GET, STR_CONTAINS, STR_TO_INT,
    INT_KW, FLOAT_KW, STRING_KW, BOOL_KW, VOID_KW, // Типы данных
    TRUE_KW, FALSE_KW, // Boolean литералы
    WHILE, FOR, IF, ELSE, SWITCH, CASE, DEFAULT,
    ARRAY, SET, GET, SIZE, 
    INCLUDE, USING, // Подключение файлов
    
    // НОВЫЕ: возврат и глобальные
    RETURN, GLOBAL,
    
    // Управление потоком
    BREAK, CONTINUE, WAIT,
    
    // Сетевые функции
    HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE,
    
    // Ввод с клавиатуры
    GETCH, KBHIT,
    
    IDENTIFIER, 
    END, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};