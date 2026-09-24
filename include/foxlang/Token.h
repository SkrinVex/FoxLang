#pragma once
#include <string>
#include "foxlang/SourceLocation.h"

namespace foxlang {

// Only syntax has its own token. Builtin functions are ordinary identifiers, so a
// program calls print, size or read_file exactly like a function it defined itself.
enum class TokenType {
    NUMBER, STRING_LITERAL,
    PLUS, MINUS, STAR, SLASH, MOD, INC, DEC,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA, ASSIGN, DOT, COLON,
    EQ, NEQ, LT, GT, LTE, GTE, AND, OR, NOT,
    PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, MOD_ASSIGN,

    INT_KW, FLOAT_KW, STRING_KW, BOOL_KW, VOID_KW, ARRAY,
    TRUE_KW, FALSE_KW,
    WHILE, FOR, IF, ELSE, SWITCH, CASE, DEFAULT,
    INCLUDE, USING, RETURN, GLOBAL, BREAK, CONTINUE,

    IDENTIFIER,
    END, ERROR
};

struct Token {
    TokenType type = TokenType::END;
    std::string value;
    int line = 1;
    int column = 1;
    SourceRange range;

    Token() = default;
    Token(TokenType t, std::string v, int l, int c, SourceRange r = {})
        : type(t), value(std::move(v)), line(l), column(c), range(r) {
        if (range.start.line == 0) {
            range.start.line = l;
            range.start.column = c;
            range.end.line = l;
            range.end.column = c + static_cast<int>(value.size());
        }
    }
};

const char* tokenTypeName(TokenType type);

// Keywords of the language, in the order editors list them.
const char* const* keywordList();

} // namespace foxlang

using foxlang::TokenType;
using foxlang::Token;
