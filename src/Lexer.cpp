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

        // 2. ОБРАБОТКА КОММЕНТАРИЕВ
        if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            while (pos < source.length() && source[pos] != '\n') {
                pos++; 
            }
            continue; 
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
        // ИСПРАВЛЕНИЕ ТУТ: Добавлена проверка || current == '_'
        else if (isalpha(current) || current == '_') {
            std::string id;
            // И ТУТ: Добавлена проверка || source[pos] == '_'
            while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
                id += source[pos++];
            }
            
            if (id == "print") tokens.push_back({TokenType::PRINT, id, line});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id, line});
            else if (id == "round") tokens.push_back({TokenType::ROUND, id, line});
            else if (id == "random") tokens.push_back({TokenType::RANDOM, id, line});
            else if (id == "fox") tokens.push_back({TokenType::FOX, id, line});
            else if (id == "int") tokens.push_back({TokenType::INT_KW, id, line});
            else if (id == "string") tokens.push_back({TokenType::STRING_KW, id, line});
            else if (id == "void") tokens.push_back({TokenType::VOID_KW, id, line});
            // Ключевые слова v4.0
            else if (id == "while") tokens.push_back({TokenType::WHILE, id, line});
            else if (id == "if") tokens.push_back({TokenType::IF, id, line});
            else if (id == "else") tokens.push_back({TokenType::ELSE, id, line});
            else if (id == "array") tokens.push_back({TokenType::ARRAY, id, line});
            else if (id == "set") tokens.push_back({TokenType::SET, id, line});
            else if (id == "get") tokens.push_back({TokenType::GET, id, line});
            else if (id == "size") tokens.push_back({TokenType::SIZE, id, line});
            else if (id == "include") tokens.push_back({TokenType::INCLUDE, id, line});
            else tokens.push_back({TokenType::IDENTIFIER, id, line});
        } 
        // 6. Операторы
        else {
            if (current == '=' && pos+1 < source.length() && source[pos+1] == '=') {
                tokens.push_back({TokenType::EQ, "==", line}); pos+=2; continue;
            }
            if (current == '!' && pos+1 < source.length() && source[pos+1] == '=') {
                tokens.push_back({TokenType::NEQ, "!=", line}); pos+=2; continue;
            }

            switch (current) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line}); break;
                case '%': tokens.push_back({TokenType::MOD, "%", line}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line}); break;
                case '[': tokens.push_back({TokenType::LBRACKET, "[", line}); break;
                case ']': tokens.push_back({TokenType::RBRACKET, "]", line}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line}); break;
                case '=': tokens.push_back({TokenType::ASSIGN, "=", line}); break;
                case '<': tokens.push_back({TokenType::LT, "<", line}); break;
                case '>': tokens.push_back({TokenType::GT, ">", line}); break;
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