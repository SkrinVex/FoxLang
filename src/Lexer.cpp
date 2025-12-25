#include "Lexer.h"
#include <cctype>
#include <iostream>

Lexer::Lexer(std::string src) : source(src) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < source.length()) {
        char current = source[pos];

        if (isspace(current)) {
            if (current == '\n') line++;
            pos++;
        }
        else if (isdigit(current)) {
            std::string num;
            while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            // Точка для дробных чисел (например, 3.14)
            if (pos < source.length() && source[pos] == '.') {
                num += source[pos++];
                while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            }
            tokens.push_back({TokenType::NUMBER, num, line});
        }
        else if (current == '"') {
            pos++; std::string str;
            while (pos < source.length() && source[pos] != '"') str += source[pos++];
            pos++;
            tokens.push_back({TokenType::STRING, str, line});
        }
        else if (isalpha(current)) {
            std::string id;
            while (pos < source.length() && isalpha(source[pos])) id += source[pos++];

            if (id == "print") tokens.push_back({TokenType::PRINT, id, line});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id, line});
            else if (id == "round") tokens.push_back({TokenType::ROUND, id, line});
            else if (id == "random") tokens.push_back({TokenType::RANDOM, id, line});
            else if (id == "fox") tokens.push_back({TokenType::FOX, id, line});
            else tokens.push_back({TokenType::IDENTIFIER, id, line}); // Неизвестное слово
        }
        else {
            switch (current) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line}); break;
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
