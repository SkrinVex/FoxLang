#include "skrumi/Lexer.h"
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
        skrumi::Lexer lexer("123 45.67");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 3); // 2 numbers + END
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::NUMBER);
        TEST_ASSERT(tokens[0].value == "123");
        TEST_ASSERT(tokens[1].type == skrumi::TokenType::NUMBER);
        TEST_ASSERT(tokens[1].value == "45.67");
    }

    // 2. String literals and escapes
    {
        skrumi::Lexer lexer("\"hello\\nworld\\t\\\"\\\\\"");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 2);
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::STRING_LITERAL);
        TEST_ASSERT(tokens[0].value == "hello\nworld\t\"\\");
    }

    // 3. Comments and whitespace
    {
        skrumi::Lexer lexer("// comment\n  42 // inline\n");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 2);
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::NUMBER);
        TEST_ASSERT(tokens[0].value == "42");
        TEST_ASSERT(tokens[0].line == 2);
    }

    // 4. Identifiers with underscores
    {
        skrumi::Lexer lexer("user_name _temp counter123");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 4);
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[0].value == "user_name");
        TEST_ASSERT(tokens[1].type == skrumi::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[1].value == "_temp");
        TEST_ASSERT(tokens[2].type == skrumi::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[2].value == "counter123");
    }

    // 5. Operators and compound assignments
    {
        skrumi::Lexer lexer("++ += -= *= /= == != <= >= && || !");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 13);
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::INC);
        TEST_ASSERT(tokens[1].type == skrumi::TokenType::PLUS_ASSIGN);
        TEST_ASSERT(tokens[2].type == skrumi::TokenType::MINUS_ASSIGN);
        TEST_ASSERT(tokens[3].type == skrumi::TokenType::STAR_ASSIGN);
        TEST_ASSERT(tokens[4].type == skrumi::TokenType::SLASH_ASSIGN);
        TEST_ASSERT(tokens[5].type == skrumi::TokenType::EQ);
        TEST_ASSERT(tokens[6].type == skrumi::TokenType::NEQ);
        TEST_ASSERT(tokens[7].type == skrumi::TokenType::LTE);
        TEST_ASSERT(tokens[8].type == skrumi::TokenType::GTE);
        TEST_ASSERT(tokens[9].type == skrumi::TokenType::AND);
        TEST_ASSERT(tokens[10].type == skrumi::TokenType::OR);
        TEST_ASSERT(tokens[11].type == skrumi::TokenType::NOT);
    }

    // 6. Keywords
    {
        skrumi::Lexer lexer("int float string bool void while for if else switch case default return break continue");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::INT_KW);
        TEST_ASSERT(tokens[1].type == skrumi::TokenType::FLOAT_KW);
        TEST_ASSERT(tokens[2].type == skrumi::TokenType::STRING_KW);
        TEST_ASSERT(tokens[3].type == skrumi::TokenType::BOOL_KW);
        TEST_ASSERT(tokens[4].type == skrumi::TokenType::VOID_KW);
        TEST_ASSERT(tokens[5].type == skrumi::TokenType::WHILE);
        TEST_ASSERT(tokens[6].type == skrumi::TokenType::FOR);
        TEST_ASSERT(tokens[7].type == skrumi::TokenType::IF);
        TEST_ASSERT(tokens[8].type == skrumi::TokenType::ELSE);
        TEST_ASSERT(tokens[9].type == skrumi::TokenType::SWITCH);
        TEST_ASSERT(tokens[10].type == skrumi::TokenType::CASE);
        TEST_ASSERT(tokens[11].type == skrumi::TokenType::DEFAULT);
        TEST_ASSERT(tokens[12].type == skrumi::TokenType::RETURN);
        TEST_ASSERT(tokens[13].type == skrumi::TokenType::BREAK);
        TEST_ASSERT(tokens[14].type == skrumi::TokenType::CONTINUE);
    }

    // 7. Invalid character error handling
    {
        bool threw = false;
        try {
            skrumi::Lexer lexer("int a = @;");
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
        skrumi::Lexer lexer("print input size get set wait fox array global");
        auto tokens = lexer.tokenize();
        for (int i = 0; i < 7; ++i) TEST_ASSERT(tokens[i].type == skrumi::TokenType::IDENTIFIER);
        TEST_ASSERT(tokens[7].type == skrumi::TokenType::ARRAY);
        TEST_ASSERT(tokens[8].type == skrumi::TokenType::GLOBAL);
    }

    // 9. Decrement, remainder assignment and brackets
    {
        skrumi::Lexer lexer("i-- n %= 2 a[0] - -1");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[1].type == skrumi::TokenType::DEC);
        TEST_ASSERT(tokens[3].type == skrumi::TokenType::MOD_ASSIGN);
        TEST_ASSERT(tokens[6].type == skrumi::TokenType::LBRACKET);
        TEST_ASSERT(tokens[8].type == skrumi::TokenType::RBRACKET);
        TEST_ASSERT(tokens[9].type == skrumi::TokenType::MINUS);
        TEST_ASSERT(tokens[10].type == skrumi::TokenType::MINUS);
    }

    // 10. Unicode escapes: \uXXXX, \u{...} and UTF-16 surrogate pairs
    {
        skrumi::Lexer lexer("\"\\u041b\\u{1F98A}\\uD83E\\uDD8A\\0\"");
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens[0].type == skrumi::TokenType::STRING_LITERAL);
        TEST_ASSERT(tokens[0].value == std::string("Л🦊🦊") + std::string(1, '\0'));
        bool threw = false;
        try {
            skrumi::Lexer bad("\"\\uZZ\"");
            bad.tokenize();
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()).find("Invalid \\u escape") != std::string::npos;
        }
        TEST_ASSERT(threw);
        skrumi::Lexer collecting("\"\\uD800\"", true);
        collecting.tokenize();
        TEST_ASSERT(collecting.getDiagnostics().size() == 1);
    }

    std::cout << "TEST_LEXER_OK" << std::endl;
    return 0;
}
