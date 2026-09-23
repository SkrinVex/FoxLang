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

Lexer::Lexer(std::string src, bool collectDiags)
    : source(std::move(src)), collectDiagnostics(collectDiags) {}

SourcePosition Lexer::currentPosition() const {
    return {line, column, pos};
}

void Lexer::advanceChar() {
    if (pos >= source.size()) return;
    char current = source[pos];
    if (current == '\n') {
        line++;
        column = 1;
        pos++;
        return;
    }
    unsigned char b = static_cast<unsigned char>(current);
    if (b < 0x80) {
        column += 1;
        pos += 1;
    } else if ((b & 0xE0) == 0xC0 && pos + 1 < source.size()) {
        column += 1;
        pos += 2;
    } else if ((b & 0xF0) == 0xE0 && pos + 2 < source.size()) {
        column += 1;
        pos += 3;
    } else if ((b & 0xF8) == 0xF0 && pos + 3 < source.size()) {
        column += 2; // 4-byte UTF-8 emoji corresponds to 2 UTF-16 code units
        pos += 4;
    } else {
        column += 1;
        pos += 1;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    diagnostics.clear();

    while (pos < source.length()) {
        char current = source[pos];

        if (std::isspace(static_cast<unsigned char>(current))) {
            advanceChar();
            continue;
        }

        // Single-line comments: //
        if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
            while (pos < source.length() && source[pos] != '\n') {
                advanceChar();
            }
            continue;
        }

        SourcePosition startPos = currentPosition();

        if (std::isdigit(static_cast<unsigned char>(current))) {
            std::string num;
            while (pos < source.length() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                num += source[pos];
                advanceChar();
            }
            if (pos < source.length() && source[pos] == '.' &&
                pos + 1 < source.length() && std::isdigit(static_cast<unsigned char>(source[pos + 1]))) {
                num += source[pos];
                advanceChar();
                while (pos < source.length() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                    num += source[pos];
                    advanceChar();
                }
            }
            SourcePosition endPos = currentPosition();
            tokens.push_back({TokenType::NUMBER, num, startPos.line, startPos.column, {startPos, endPos}});
        } 
        else if (current == '"') {
            advanceChar(); // consume opening quote
            std::string str;
            bool closed = false;
            while (pos < source.length()) {
                if (source[pos] == '"') {
                    closed = true;
                    advanceChar(); // consume closing quote
                    break;
                }
                if (source[pos] == '\\' && pos + 1 < source.length()) {
                    advanceChar(); // consume backslash
                    char escaped = source[pos];
                    switch (escaped) {
                        case 'n': str += '\n'; break;
                        case 't': str += '\t'; break;
                        case 'r': str += '\r'; break;
                        case '\\': str += '\\'; break;
                        case '"': str += '"'; break;
                        default: str += escaped; break;
                    }
                    advanceChar();
                } else {
                    size_t curPos = pos;
                    advanceChar();
                    str.append(source.data() + curPos, pos - curPos);
                }
            }
            SourcePosition endPos = currentPosition();
            if (!closed) {
                std::string msg = "Unclosed string literal";
                if (collectDiagnostics) {
                    diagnostics.push_back({DiagnosticSeverity::Error, msg, {startPos, endPos}});
                } else {
                    throw std::runtime_error("Syntax Error: " + msg + " at line " + std::to_string(startPos.line));
                }
            }
            tokens.push_back({TokenType::STRING_LITERAL, str, startPos.line, startPos.column, {startPos, endPos}});
        } 
        else if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
            std::string id;
            while (pos < source.length() && (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_')) {
                id += source[pos];
                advanceChar();
            }
            SourcePosition endPos = currentPosition();
            SourceRange range{startPos, endPos};

            TokenType type = TokenType::IDENTIFIER;
            if (id.find('_') != std::string::npos) {
                // Builtins with underscore
                if (id == "read_file") type = TokenType::READ_FILE;
                else if (id == "json_get") type = TokenType::JSON_GET;
                else if (id == "str_contains") type = TokenType::STR_CONTAINS;
                else if (id == "str_to_int") type = TokenType::STR_TO_INT;
                else if (id == "server_start") type = TokenType::SERVER_START;
                else if (id == "server_stop") type = TokenType::SERVER_STOP;
                else if (id == "route_get") type = TokenType::ROUTE_GET;
                else if (id == "route_post") type = TokenType::ROUTE_POST;
                else if (id == "send_response") type = TokenType::SEND_RESPONSE;
                else type = TokenType::IDENTIFIER;
            }
            else if (id == "print") type = TokenType::PRINT;
            else if (id == "input") type = TokenType::INPUT;
            else if (id == "round") type = TokenType::ROUND;
            else if (id == "random") type = TokenType::RANDOM;
            else if (id == "fox") type = TokenType::FOX;
            else if (id == "readfile") type = TokenType::READ_FILE;
            else if (id == "int") type = TokenType::INT_KW;
            else if (id == "float") type = TokenType::FLOAT_KW;
            else if (id == "string") type = TokenType::STRING_KW;
            else if (id == "bool") type = TokenType::BOOL_KW;
            else if (id == "void") type = TokenType::VOID_KW;
            else if (id == "true") type = TokenType::TRUE_KW;
            else if (id == "false") type = TokenType::FALSE_KW;
            else if (id == "while") type = TokenType::WHILE;
            else if (id == "for") type = TokenType::FOR;
            else if (id == "if") type = TokenType::IF;
            else if (id == "else") type = TokenType::ELSE;
            else if (id == "switch") type = TokenType::SWITCH;
            else if (id == "case") type = TokenType::CASE;
            else if (id == "default") type = TokenType::DEFAULT;
            else if (id == "break") type = TokenType::BREAK;
            else if (id == "continue") type = TokenType::CONTINUE;
            else if (id == "wait") type = TokenType::WAIT;
            else if (id == "array") type = TokenType::ARRAY;
            else if (id == "set") type = TokenType::SET;
            else if (id == "include") type = TokenType::INCLUDE;
            else if (id == "using") type = TokenType::USING;
            else if (id == "return") type = TokenType::RETURN;
            else if (id == "global") type = TokenType::GLOBAL;
            else if (id == "httpget") type = TokenType::HTTP_GET;
            else if (id == "httppost") type = TokenType::HTTP_POST;
            else if (id == "httpput") type = TokenType::HTTP_PUT;
            else if (id == "httpdelete") type = TokenType::HTTP_DELETE;
            else if (id == "getch") type = TokenType::GETCH;
            else if (id == "kbhit") type = TokenType::KBHIT;
            else type = TokenType::IDENTIFIER;

            tokens.push_back({type, id, startPos.line, startPos.column, range});
        } 
        else {
            // Check 2-character operators
            if (current == '=' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::EQ, "==", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '!' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::NEQ, "!=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '<' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::LTE, "<=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '>' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::GTE, ">=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '+' && pos + 1 < source.length() && source[pos + 1] == '+') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::INC, "++", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '+' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::PLUS_ASSIGN, "+=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '-' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::MINUS_ASSIGN, "-=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '*' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::STAR_ASSIGN, "*=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '/' && pos + 1 < source.length() && source[pos + 1] == '=') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::SLASH_ASSIGN, "/=", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '&' && pos + 1 < source.length() && source[pos + 1] == '&') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::AND, "&&", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }
            if (current == '|' && pos + 1 < source.length() && source[pos + 1] == '|') {
                advanceChar(); advanceChar();
                tokens.push_back({TokenType::OR, "||", startPos.line, startPos.column, {startPos, currentPosition()}});
                continue;
            }

            TokenType singleType = TokenType::ERROR;
            std::string singleVal(1, current);
            switch (current) {
                case '+': singleType = TokenType::PLUS; break;
                case '-': singleType = TokenType::MINUS; break;
                case '*': singleType = TokenType::STAR; break;
                case '/': singleType = TokenType::SLASH; break;
                case '%': singleType = TokenType::MOD; break;
                case '(': singleType = TokenType::LPAREN; break;
                case ')': singleType = TokenType::RPAREN; break;
                case '{': singleType = TokenType::LBRACE; break;
                case '}': singleType = TokenType::RBRACE; break;
                case '[': singleType = TokenType::LBRACKET; break;
                case ']': singleType = TokenType::RBRACKET; break;
                case ';': singleType = TokenType::SEMICOLON; break;
                case ',': singleType = TokenType::COMMA; break;
                case '=': singleType = TokenType::ASSIGN; break;
                case '.': singleType = TokenType::DOT; break;
                case '!': singleType = TokenType::NOT; break;
                case '<': singleType = TokenType::LT; break;
                case '>': singleType = TokenType::GT; break;
                case ':': singleType = TokenType::COLON; break;
                default: 
                    advanceChar();
                    SourcePosition badEnd = currentPosition();
                    std::string err = std::string("Unknown character '") + current + "'";
                    if (collectDiagnostics) {
                        diagnostics.push_back({DiagnosticSeverity::Error, err, {startPos, badEnd}});
                        tokens.push_back({TokenType::ERROR, singleVal, startPos.line, startPos.column, {startPos, badEnd}});
                        continue;
                    } else {
                        throw std::runtime_error(std::string("Runtime Error: Unknown character '") + current + "' at line " + std::to_string(startPos.line));
                    }
            }
            advanceChar();
            tokens.push_back({singleType, singleVal, startPos.line, startPos.column, {startPos, currentPosition()}});
        }
    }

    SourcePosition eofPos = currentPosition();
    tokens.push_back({TokenType::END, "", eofPos.line, eofPos.column, {eofPos, eofPos}});
    return tokens;
}

} // namespace foxlang
