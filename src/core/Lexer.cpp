#include "foxlang/Lexer.h"
#include <cctype>
#include <stdexcept>

namespace foxlang {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::MOD: return "MOD";
        case TokenType::INC: return "INC";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LTE: return "LTE";
        case TokenType::GTE: return "GTE";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TokenType::STAR_ASSIGN: return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN: return "SLASH_ASSIGN";
        case TokenType::PRINT: return "PRINT";
        case TokenType::INPUT: return "INPUT";
        case TokenType::ROUND: return "ROUND";
        case TokenType::RANDOM: return "RANDOM";
        case TokenType::FOX: return "FOX";
        case TokenType::READ_FILE: return "READ_FILE";
        case TokenType::JSON_GET: return "JSON_GET";
        case TokenType::STR_CONTAINS: return "STR_CONTAINS";
        case TokenType::STR_TO_INT: return "STR_TO_INT";
        case TokenType::INT_KW: return "INT_KW";
        case TokenType::FLOAT_KW: return "FLOAT_KW";
        case TokenType::STRING_KW: return "STRING_KW";
        case TokenType::BOOL_KW: return "BOOL_KW";
        case TokenType::VOID_KW: return "VOID_KW";
        case TokenType::TRUE_KW: return "TRUE_KW";
        case TokenType::FALSE_KW: return "FALSE_KW";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::SWITCH: return "SWITCH";
        case TokenType::CASE: return "CASE";
        case TokenType::DEFAULT: return "DEFAULT";
        case TokenType::ARRAY: return "ARRAY";
        case TokenType::SET: return "SET";
        case TokenType::GET: return "GET";
        case TokenType::SIZE: return "SIZE";
        case TokenType::INCLUDE: return "INCLUDE";
        case TokenType::USING: return "USING";
        case TokenType::RETURN: return "RETURN";
        case TokenType::GLOBAL: return "GLOBAL";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::WAIT: return "WAIT";
        case TokenType::HTTP_GET: return "HTTP_GET";
        case TokenType::HTTP_POST: return "HTTP_POST";
        case TokenType::HTTP_PUT: return "HTTP_PUT";
        case TokenType::HTTP_DELETE: return "HTTP_DELETE";
        case TokenType::SERVER_START: return "SERVER_START";
        case TokenType::SERVER_STOP: return "SERVER_STOP";
        case TokenType::ROUTE_GET: return "ROUTE_GET";
        case TokenType::ROUTE_POST: return "ROUTE_POST";
        case TokenType::SEND_RESPONSE: return "SEND_RESPONSE";
        case TokenType::GETCH: return "GETCH";
        case TokenType::KBHIT: return "KBHIT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::END: return "END";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

Lexer::Lexer(std::string src) : source(std::move(src)) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (pos < source.length()) {
        char current = source[pos];
        int startCol = column;

        if (std::isspace(static_cast<unsigned char>(current))) {
            if (current == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            pos++;
            continue;
        }

        if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            while (pos < source.length() && source[pos] != '\n') {
                pos++;
                column++;
            }
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current))) {
            std::string num;
            while (pos < source.length() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                num += source[pos++];
                column++;
            }
            if (pos < source.length() && source[pos] == '.' &&
                pos + 1 < source.length() && std::isdigit(static_cast<unsigned char>(source[pos + 1]))) {
                num += source[pos++];
                column++;
                while (pos < source.length() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                    num += source[pos++];
                    column++;
                }
            }
            tokens.push_back({TokenType::NUMBER, num, line, startCol});
        } 
        else if (current == '"') {
            pos++;
            column++;
            std::string str;
            while (pos < source.length() && source[pos] != '"') {
                if (source[pos] == '\\' && pos + 1 < source.length()) {
                    pos++;
                    column++;
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
                    if (source[pos] == '\n') {
                        line++;
                        column = 0;
                    }
                    str += source[pos];
                }
                pos++;
                column++;
            }
            if (pos < source.length() && source[pos] == '"') {
                pos++;
                column++;
            }
            tokens.push_back({TokenType::STRING_LITERAL, str, line, startCol});
        } 
        else if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
            std::string id;
            while (pos < source.length() && (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_')) {
                id += source[pos++];
                column++;
            }

            if (id.find('_') != std::string::npos) {
                tokens.push_back({TokenType::IDENTIFIER, id, line, startCol});
            }
            else if (id == "print") tokens.push_back({TokenType::PRINT, id, line, startCol});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id, line, startCol});
            else if (id == "fox") tokens.push_back({TokenType::FOX, id, line, startCol});
            else if (id == "readfile") tokens.push_back({TokenType::READ_FILE, id, line, startCol});
            else if (id == "int") tokens.push_back({TokenType::INT_KW, id, line, startCol});
            else if (id == "float") tokens.push_back({TokenType::FLOAT_KW, id, line, startCol});
            else if (id == "string") tokens.push_back({TokenType::STRING_KW, id, line, startCol});
            else if (id == "bool") tokens.push_back({TokenType::BOOL_KW, id, line, startCol});
            else if (id == "true") tokens.push_back({TokenType::TRUE_KW, id, line, startCol});
            else if (id == "false") tokens.push_back({TokenType::FALSE_KW, id, line, startCol});
            else if (id == "void") tokens.push_back({TokenType::VOID_KW, id, line, startCol});
            else if (id == "while") tokens.push_back({TokenType::WHILE, id, line, startCol});
            else if (id == "for") tokens.push_back({TokenType::FOR, id, line, startCol});
            else if (id == "if") tokens.push_back({TokenType::IF, id, line, startCol});
            else if (id == "else") tokens.push_back({TokenType::ELSE, id, line, startCol});
            else if (id == "switch") tokens.push_back({TokenType::SWITCH, id, line, startCol});
            else if (id == "case") tokens.push_back({TokenType::CASE, id, line, startCol});
            else if (id == "default") tokens.push_back({TokenType::DEFAULT, id, line, startCol});
            else if (id == "break") tokens.push_back({TokenType::BREAK, id, line, startCol});
            else if (id == "continue") tokens.push_back({TokenType::CONTINUE, id, line, startCol});
            else if (id == "wait") tokens.push_back({TokenType::WAIT, id, line, startCol});
            else if (id == "array") tokens.push_back({TokenType::ARRAY, id, line, startCol});
            else if (id == "set") tokens.push_back({TokenType::SET, id, line, startCol});
            else if (id == "include") tokens.push_back({TokenType::INCLUDE, id, line, startCol});
            else if (id == "using") tokens.push_back({TokenType::USING, id, line, startCol});
            else if (id == "return") tokens.push_back({TokenType::RETURN, id, line, startCol});
            else if (id == "global") tokens.push_back({TokenType::GLOBAL, id, line, startCol});
            else if (id == "httpget") tokens.push_back({TokenType::HTTP_GET, id, line, startCol});
            else if (id == "httppost") tokens.push_back({TokenType::HTTP_POST, id, line, startCol});
            else if (id == "httpput") tokens.push_back({TokenType::HTTP_PUT, id, line, startCol});
            else if (id == "httpdelete") tokens.push_back({TokenType::HTTP_DELETE, id, line, startCol});
            else if (id == "getch") tokens.push_back({TokenType::GETCH, id, line, startCol});
            else if (id == "kbhit") tokens.push_back({TokenType::KBHIT, id, line, startCol});
            else if (id == "server_start") tokens.push_back({TokenType::SERVER_START, id, line, startCol});
            else if (id == "server_stop") tokens.push_back({TokenType::SERVER_STOP, id, line, startCol});
            else if (id == "route_get") tokens.push_back({TokenType::ROUTE_GET, id, line, startCol});
            else if (id == "route_post") tokens.push_back({TokenType::ROUTE_POST, id, line, startCol});
            else if (id == "send_response") tokens.push_back({TokenType::SEND_RESPONSE, id, line, startCol});
            else tokens.push_back({TokenType::IDENTIFIER, id, line, startCol});
        } 
        else {
            if (current == '=' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::EQ, "==", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '!' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::NEQ, "!=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '<' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::LTE, "<=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '>' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::GTE, ">=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '+' && pos + 1 < source.length() && source[pos + 1] == '+') {
                tokens.push_back({TokenType::INC, "++", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '+' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::PLUS_ASSIGN, "+=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '-' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::MINUS_ASSIGN, "-=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '*' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::STAR_ASSIGN, "*=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '=') {
                tokens.push_back({TokenType::SLASH_ASSIGN, "/=", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '&' && pos + 1 < source.length() && source[pos + 1] == '&') {
                tokens.push_back({TokenType::AND, "&&", line, startCol}); pos += 2; column += 2; continue;
            }
            if (current == '|' && pos + 1 < source.length() && source[pos + 1] == '|') {
                tokens.push_back({TokenType::OR, "||", line, startCol}); pos += 2; column += 2; continue;
            }

            switch (current) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line, startCol}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line, startCol}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line, startCol}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line, startCol}); break;
                case '%': tokens.push_back({TokenType::MOD, "%", line, startCol}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line, startCol}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line, startCol}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line, startCol}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line, startCol}); break;
                case '[': tokens.push_back({TokenType::LBRACKET, "[", line, startCol}); break;
                case ']': tokens.push_back({TokenType::RBRACKET, "]", line, startCol}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line, startCol}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line, startCol}); break;
                case '=': tokens.push_back({TokenType::ASSIGN, "=", line, startCol}); break;
                case '.': tokens.push_back({TokenType::DOT, ".", line, startCol}); break;
                case '!': tokens.push_back({TokenType::NOT, "!", line, startCol}); break;
                case '<': tokens.push_back({TokenType::LT, "<", line, startCol}); break;
                case '>': tokens.push_back({TokenType::GT, ">", line, startCol}); break;
                case ':': tokens.push_back({TokenType::COLON, ":", line, startCol}); break;
                default: 
                    throw std::runtime_error(std::string("Runtime Error: Unknown character '") + current + "' at line " + std::to_string(line));
            }
            pos++;
            column++;
        }
    }
    tokens.push_back({TokenType::END, "", line, column});
    return tokens;
}

} // namespace foxlang
