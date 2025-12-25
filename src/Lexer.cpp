#include "Lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(std::string src) : source(src) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < source.length()) {
        char current = source[pos];
        
        // 1. Пропуск пробелов
        if (isspace(current)) {
            if (current == '\n') line++;
            pos++;
            continue;
        }

        // 2. ОБРАБОТКА КОММЕНТАРИЕВ (НОВОЕ)
        // Если видим '/', и следующий символ тоже '/', то это комментарий
        if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            while (pos < source.length() && source[pos] != '\n') {
                pos++; // Пропускаем всё до конца строки
            }
            continue; // Переходим к следующей итерации цикла
        }

        // 3. Числа
        if (isdigit(current)) {
            std::string num;
            while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            if (pos < source.length() && source[pos] == '.') {
                num += source[pos++];
                while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            }
            tokens.push_back({TokenType::NUMBER, num, line});
        } 
        // 4. Строки
        else if (current == '"') {
            pos++; std::string str;
            while (pos < source.length() && source[pos] != '"') str += source[pos++];
            pos++; 
            tokens.push_back({TokenType::STRING_LITERAL, str, line});
        } 
        // 5. Идентификаторы и Ключевые слова
        else if (isalpha(current)) {
            std::string id;
            while (pos < source.length() && isalnum(source[pos])) id += source[pos++];
            
            if (id == "print") tokens.push_back({TokenType::PRINT, id, line});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id, line});
            else if (id == "round") tokens.push_back({TokenType::ROUND, id, line});
            else if (id == "random") tokens.push_back({TokenType::RANDOM, id, line});
            else if (id == "fox") tokens.push_back({TokenType::FOX, id, line});
            else if (id == "int") tokens.push_back({TokenType::INT_KW, id, line});
            else if (id == "string") tokens.push_back({TokenType::STRING_KW, id, line});
            else if (id == "void") tokens.push_back({TokenType::VOID_KW, id, line});
            else tokens.push_back({TokenType::IDENTIFIER, id, line});
        } 
        // 6. Операторы
        else {
            switch (current) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line}); break; // Это деление, если не сработало условие выше
                case '(': tokens.push_back({TokenType::LPAREN, "(", line}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line}); break;
                case '=': tokens.push_back({TokenType::ASSIGN, "=", line}); break;
                default: 
                    std::cerr << "Lexer Error: Unknown char '" << current << "' at line " << line << std::endl;
                    exit(1);
            }
            pos++;
        }
    }
    tokens.push_back({TokenType::END, "", line});
    return tokens;
}