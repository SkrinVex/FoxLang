#include "foxlang/Lexer.h"
#include <iostream>
#include <cassert>
#include <stdexcept>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int main() {
    // 1. Numbers and floats
    {
        foxlang::Lexer lexer("123 45.67");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 3); // 2 numbers + END
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::NUMBER);
        TEST_ASSERT(tokens[0].value == "123");
        TEST_ASSERT(tokens[1].type == foxlang::TokenType::NUMBER);
        TEST_ASSERT(tokens[1].value == "45.67");
    }

    // 2. String literals and escapes
    {
        foxlang::Lexer lexer("\"hello\\nworld\\t\\\"\\\\\"");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 2);
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::STRING_LITERAL);
        TEST_ASSERT(tokens[0].value == "hello\nworld\t\"\\");
    }

    // 3. Comments and whitespace
    {
        foxlang::Lexer lexer("// comment\n  42 // inline\n");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 2);
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::NUMBER);
        TEST_ASSERT(tokens[0].value == "42");
        TEST_ASSERT(tokens[0].line == 2);
    }

    // 4. Identifiers with underscores
    {
        foxlang::Lexer lexer("user_name _temp counter123");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 4);
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[0].value == "user_name");
        TEST_ASSERT(tokens[1].type == foxlang::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[1].value == "_temp");
        TEST_ASSERT(tokens[2].type == foxlang::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[2].value == "counter123");
    }

    // 5. Operators and compound assignments
    {
        foxlang::Lexer lexer("++ += -= *= /= == != <= >= && || !");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 13);
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::INC);
        TEST_ASSERT(tokens[1].type == foxlang::TokenType::PLUS_ASSIGN);
        TEST_ASSERT(tokens[2].type == foxlang::TokenType::MINUS_ASSIGN);
        TEST_ASSERT(tokens[3].type == foxlang::TokenType::STAR_ASSIGN);
        TEST_ASSERT(tokens[4].type == foxlang::TokenType::SLASH_ASSIGN);
        TEST_ASSERT(tokens[5].type == foxlang::TokenType::EQ);
        TEST_ASSERT(tokens[6].type == foxlang::TokenType::NEQ);
        TEST_ASSERT(tokens[7].type == foxlang::TokenType::LTE);
        TEST_ASSERT(tokens[8].type == foxlang::TokenType::GTE);
        TEST_ASSERT(tokens[9].type == foxlang::TokenType::AND);
        TEST_ASSERT(tokens[10].type == foxlang::TokenType::OR);
        TEST_ASSERT(tokens[11].type == foxlang::TokenType::NOT);
    }

    // 6. Keywords
    {
        foxlang::Lexer lexer("int float string bool void while for if else switch case default return break continue");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::INT_KW);
        TEST_ASSERT(tokens[1].type == foxlang::TokenType::FLOAT_KW);
        TEST_ASSERT(tokens[2].type == foxlang::TokenType::STRING_KW);
        TEST_ASSERT(tokens[3].type == foxlang::TokenType::BOOL_KW);
        TEST_ASSERT(tokens[4].type == foxlang::TokenType::VOID_KW);
        TEST_ASSERT(tokens[5].type == foxlang::TokenType::WHILE);
        TEST_ASSERT(tokens[6].type == foxlang::TokenType::FOR);
        TEST_ASSERT(tokens[7].type == foxlang::TokenType::IF);
        TEST_ASSERT(tokens[8].type == foxlang::TokenType::ELSE);
        TEST_ASSERT(tokens[9].type == foxlang::TokenType::SWITCH);
        TEST_ASSERT(tokens[10].type == foxlang::TokenType::CASE);
        TEST_ASSERT(tokens[11].type == foxlang::TokenType::DEFAULT);
        TEST_ASSERT(tokens[12].type == foxlang::TokenType::RETURN);
        TEST_ASSERT(tokens[13].type == foxlang::TokenType::BREAK);
        TEST_ASSERT(tokens[14].type == foxlang::TokenType::CONTINUE);
    }

    // 7. Invalid character error handling
    {
        bool threw = false;
        try {
            foxlang::Lexer lexer("int a = @;");
            lexer.tokenize();
        } catch (const std::runtime_error& e) {
            threw = true;
            std::string msg = e.what();
            TEST_ASSERT(msg.find("Unknown character '@'") != std::string::npos);
        }
        TEST_ASSERT(threw);
    }

    // 8. Builtin functions are plain identifiers; only syntax has keywords
    {
        foxlang::Lexer lexer("print input size get set wait fox array global");
        auto tokens = lexer.tokenize();
        for (int i = 0; i < 7; ++i) TEST_ASSERT(tokens[i].type == foxlang::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[7].type == foxlang::TokenType::ARRAY);
        TEST_ASSERT(tokens[8].type == foxlang::TokenType::GLOBAL);
    }

    // 9. Decrement, remainder assignment and brackets
    {
        foxlang::Lexer lexer("i-- n %= 2 a[0] - -1");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[1].type == foxlang::TokenType::DEC);
        TEST_ASSERT(tokens[3].type == foxlang::TokenType::MOD_ASSIGN);
        TEST_ASSERT(tokens[6].type == foxlang::TokenType::LBRACKET);
        TEST_ASSERT(tokens[8].type == foxlang::TokenType::RBRACKET);
        TEST_ASSERT(tokens[9].type == foxlang::TokenType::MINUS);
        TEST_ASSERT(tokens[10].type == foxlang::TokenType::MINUS);
    }

    // 10. Unicode escapes: \uXXXX, \u{...} and UTF-16 surrogate pairs
    {
        foxlang::Lexer lexer("\"\\u041b\\u{1F98A}\\uD83E\\uDD8A\\0\"");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[0].type == foxlang::TokenType::STRING_LITERAL);
        TEST_ASSERT(tokens[0].value == std::string("Л🦊🦊") + std::string(1, '\0'));
        bool threw = false;
        try {
            foxlang::Lexer bad("\"\\uZZ\"");
            bad.tokenize();
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()).find("Invalid \\u escape") != std::string::npos;
        }
        TEST_ASSERT(threw);
        foxlang::Lexer collecting("\"\\uD800\"", true);
        collecting.tokenize();
        TEST_ASSERT(collecting.getDiagnostics().size() == 1);
    }

    std::cout << "TEST_LEXER_OK" << std::endl;
    return 0;
}
