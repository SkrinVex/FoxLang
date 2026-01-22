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
            continue;
        }

        if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            while (pos < source.length() && source[pos] != '\n') pos++;
            continue; 
        }

        if (isdigit(current)) {
            std::string num;
            while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            // Проверяем точку только если после неё есть цифра (для float)
            if (pos < source.length() && source[pos] == '.' && 
                pos + 1 < source.length() && isdigit(source[pos + 1])) {
                num += source[pos++];
                while (pos < source.length() && isdigit(source[pos])) num += source[pos++];
            }
            tokens.push_back({TokenType::NUMBER, num, line});
        } 
        else if (current == '"') {
            pos++; std::string str;
            while (pos < source.length() && source[pos] != '"') {
                if (source[pos] == '\\' && pos + 1 < source.length()) {
                    pos++; // Пропускаем обратный слеш
                    char escaped = source[pos];
                    switch (escaped) {
                        case 'n': str += '\n'; break;
                        case 't': str += '\t'; break;
                        case 'r': str += '\r'; break;
                        case '\\': str += '\\'; break;
                        case '"': str += '"'; break;
                        default: str += escaped; break;
                    }
                } else {
                    str += source[pos];
                }
                pos++;
            }
            pos++; 
            tokens.push_back({TokenType::STRING_LITERAL, str, line});
        } 
        else if (isalpha(current) || current == '_') {
            std::string id;
            while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_')) {
                id += source[pos++];
            }
            
            // Если идентификатор содержит подчеркивания, это точно не ключевое слово
            if (id.find('_') != std::string::npos) {
                tokens.push_back({TokenType::IDENTIFIER, id, line});
            }
            else if (id == "print") tokens.push_back({TokenType::PRINT, id, line});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id, line});
            else if (id == "round") tokens.push_back({TokenType::ROUND, id, line});
            else if (id == "random") tokens.push_back({TokenType::RANDOM, id, line});
            else if (id == "fox") tokens.push_back({TokenType::FOX, id, line});
            else if (id == "readfile") tokens.push_back({TokenType::READ_FILE, id, line});
            else if (id == "jsonget") tokens.push_back({TokenType::JSON_GET, id, line});
            else if (id == "strcontains") tokens.push_back({TokenType::STR_CONTAINS, id, line});
            else if (id == "strtoint") tokens.push_back({TokenType::STR_TO_INT, id, line});
            else if (id == "int") tokens.push_back({TokenType::INT_KW, id, line});
            else if (id == "float") tokens.push_back({TokenType::FLOAT_KW, id, line});
            else if (id == "string") tokens.push_back({TokenType::STRING_KW, id, line});
            else if (id == "bool") tokens.push_back({TokenType::BOOL_KW, id, line});
            else if (id == "true") tokens.push_back({TokenType::TRUE_KW, id, line});
            else if (id == "false") tokens.push_back({TokenType::FALSE_KW, id, line});
            else if (id == "void") tokens.push_back({TokenType::VOID_KW, id, line});
            else if (id == "while") tokens.push_back({TokenType::WHILE, id, line});
            else if (id == "for") tokens.push_back({TokenType::FOR, id, line});
            else if (id == "if") tokens.push_back({TokenType::IF, id, line});
            else if (id == "else") tokens.push_back({TokenType::ELSE, id, line});
            else if (id == "switch") tokens.push_back({TokenType::SWITCH, id, line});
            else if (id == "case") tokens.push_back({TokenType::CASE, id, line});
            else if (id == "default") tokens.push_back({TokenType::DEFAULT, id, line});
            else if (id == "break") tokens.push_back({TokenType::BREAK, id, line});
            else if (id == "continue") tokens.push_back({TokenType::CONTINUE, id, line});
            else if (id == "wait") tokens.push_back({TokenType::WAIT, id, line});
            else if (id == "array") tokens.push_back({TokenType::ARRAY, id, line});
            else if (id == "set") tokens.push_back({TokenType::SET, id, line});
            else if (id == "get") tokens.push_back({TokenType::GET, id, line});
            else if (id == "size") tokens.push_back({TokenType::SIZE, id, line});
            else if (id == "include") tokens.push_back({TokenType::INCLUDE, id, line});
            else if (id == "using") tokens.push_back({TokenType::USING, id, line});
            else if (id == "return") tokens.push_back({TokenType::RETURN, id, line});
            else if (id == "global") tokens.push_back({TokenType::GLOBAL, id, line});
            else if (id == "httpget") tokens.push_back({TokenType::HTTP_GET, id, line});
            else if (id == "httppost") tokens.push_back({TokenType::HTTP_POST, id, line});
            else if (id == "httpput") tokens.push_back({TokenType::HTTP_PUT, id, line});
            else if (id == "httpdelete") tokens.push_back({TokenType::HTTP_DELETE, id, line});
            else if (id == "getch") tokens.push_back({TokenType::GETCH, id, line});
            else if (id == "kbhit") tokens.push_back({TokenType::KBHIT, id, line});
            else if (id == "server_start") tokens.push_back({TokenType::SERVER_START, id, line});
            else if (id == "server_stop") tokens.push_back({TokenType::SERVER_STOP, id, line});
            else if (id == "route_get") tokens.push_back({TokenType::ROUTE_GET, id, line});
            else if (id == "route_post") tokens.push_back({TokenType::ROUTE_POST, id, line});
            else if (id == "send_response") tokens.push_back({TokenType::SEND_RESPONSE, id, line});
            else tokens.push_back({TokenType::IDENTIFIER, id, line});
        } 
        else {
            if (current == '=' && pos+1 < source.length() && source[pos+1] == '=') {
                tokens.push_back({TokenType::EQ, "==", line}); pos+=2; continue;
            }
            if (current == '!' && pos+1 < source.length() && source[pos+1] == '=') {
                tokens.push_back({TokenType::NEQ, "!=", line}); pos+=2; continue;
            }
            if (current == '+' && pos+1 < source.length() && source[pos+1] == '+') {
                tokens.push_back({TokenType::INC, "++", line}); pos+=2; continue;
            }
            if (current == '&' && pos+1 < source.length() && source[pos+1] == '&') {
                tokens.push_back({TokenType::AND, "&&", line}); pos+=2; continue;
            }
            if (current == '|' && pos+1 < source.length() && source[pos+1] == '|') {
                tokens.push_back({TokenType::OR, "||", line}); pos+=2; continue;
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
                case '.': tokens.push_back({TokenType::DOT, ".", line}); break;
                case '!': tokens.push_back({TokenType::NOT, "!", line}); break;
                case '<': tokens.push_back({TokenType::LT, "<", line}); break;
                case '>': tokens.push_back({TokenType::GT, ">", line}); break;
                case ':': tokens.push_back({TokenType::COLON, ":", line}); break;
                default: 
                    throw std::runtime_error(std::string("Runtime Error: Unknown character '") + current + "' at line " + std::to_string(line));
            }
            pos++;
        }
    }
    tokens.push_back({TokenType::END, "", line});
    return tokens;
}