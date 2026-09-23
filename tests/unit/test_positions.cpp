#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/SourceLocation.h"
#include <iostream>
#include <cassert>
#include <string>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int main() {
    // 1. First line positions
    {
        std::string src = "int x = 42;";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 6); // int, x, =, 42, ;, END

        // 'int': line 1, col 1..3
        TEST_ASSERT(tokens[0].range.start.line == 1);
        TEST_ASSERT(tokens[0].range.start.column == 1);
        TEST_ASSERT(tokens[0].range.end.line == 1);
        TEST_ASSERT(tokens[0].range.end.column == 4);

        // 'x': line 1, col 5..5
        TEST_ASSERT(tokens[1].range.start.line == 1);
        TEST_ASSERT(tokens[1].range.start.column == 5);
        TEST_ASSERT(tokens[1].range.end.line == 1);
        TEST_ASSERT(tokens[1].range.end.column == 6);

        // '42': line 1, col 9..10
        TEST_ASSERT(tokens[3].range.start.line == 1);
        TEST_ASSERT(tokens[3].range.start.column == 9);
        TEST_ASSERT(tokens[3].range.end.line == 1);
        TEST_ASSERT(tokens[3].range.end.column == 11);
    }

    // 2. Multi-line positions
    {
        std::string src = "int a = 1;\n\nint b = 2;\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        // int, a, =, 1, ;, int, b, =, 2, ;, END
        TEST_ASSERT(tokens.size() == 11);
        TEST_ASSERT(tokens[5].range.start.line == 3);
        TEST_ASSERT(tokens[5].range.start.column == 1);
        TEST_ASSERT(tokens[6].value == "b");
        TEST_ASSERT(tokens[6].range.start.line == 3);
        TEST_ASSERT(tokens[6].range.start.column == 5);
    }

    // 3. UTF-8 Cyrillic source (1 UTF-16 code unit per Cyrillic character)
    {
        // "Привет" is 6 Cyrillic characters (12 bytes UTF-8, 6 code units UTF-16)
        std::string src = "string s = \"Привет\";";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 6);
        // "Привет" starts at col 12, length 8 code units (including quotes)
        TEST_ASSERT(tokens[3].range.start.line == 1);
        TEST_ASSERT(tokens[3].range.start.column == 12);
        TEST_ASSERT(tokens[3].range.end.column == 20); // 12 + 8 (2 quotes + 6 chars)
    }

    // 4. UTF-8 Emoji source (4 bytes UTF-8, 2 UTF-16 code units / surrogate pair)
    {
        // 🦊 is 4 bytes UTF-8 (U+1F98A), 2 UTF-16 code units
        std::string src = "string fox = \"🦊\";";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        TEST_ASSERT(tokens.size() == 6);
        // "🦊" starts at col 14, 2 quotes + 2 code units = 4 code units
        TEST_ASSERT(tokens[3].range.start.line == 1);
        TEST_ASSERT(tokens[3].range.start.column == 14);
        TEST_ASSERT(tokens[3].range.end.column == 18); // 14 + 4
    }

    // 5. Nested expressions AST ranges
    {
        std::string src = "int res = 1 + (2 * 3);";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        TEST_ASSERT(program != nullptr);
        TEST_ASSERT(program->stmts.size() == 1);

        auto decl = dynamic_cast<foxlang::VarDeclNode*>(program->stmts[0].get());
        TEST_ASSERT(decl != nullptr);
        TEST_ASSERT(decl->name == "res");
        TEST_ASSERT(decl->nameRange.start.line == 1 && decl->nameRange.start.column == 5);
        TEST_ASSERT(decl->nameRange.end.column == 8);

        // Overall statement range spans from 'int' to ';'
        TEST_ASSERT(decl->range.start.line == 1 && decl->range.start.column == 1);
        TEST_ASSERT(decl->range.end.column == 23);
    }

    // 6. Function declaration ranges (full span and name span)
    {
        std::string src = "int add(int a, int b) {\n    return a + b;\n}";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto program = parser.parseProgram();
        TEST_ASSERT(program != nullptr);
        TEST_ASSERT(program->stmts.size() == 1);

        auto fn = dynamic_cast<foxlang::FuncDefNode*>(program->stmts[0].get());
        TEST_ASSERT(fn != nullptr);
        TEST_ASSERT(fn->name == "add");
        // nameRange: 'add'
        TEST_ASSERT(fn->nameRange.start.line == 1 && fn->nameRange.start.column == 5);
        TEST_ASSERT(fn->nameRange.end.column == 8);
        // full range: 'int' to '}'
        TEST_ASSERT(fn->range.start.line == 1 && fn->range.start.column == 1);
        TEST_ASSERT(fn->range.end.line == 3 && fn->range.end.column == 2);
    }

    // 7. Error locations and diagnostics with recovery
    {
        std::string badSrc = "int a = ;\nint b = 42;";
        foxlang::Lexer lexer(badSrc, true);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        std::vector<foxlang::Diagnostic> diags;
        auto prog = parser.parseProgramWithDiagnostics(diags);
        TEST_ASSERT(!diags.empty());
        TEST_ASSERT(diags[0].severity == foxlang::DiagnosticSeverity::Error);
        TEST_ASSERT(diags[0].range.start.line == 1);
        // Parser should have recovered and parsed the second statement
        TEST_ASSERT(prog != nullptr);
    }

    // 8. UTF-8 byte offset to SourcePosition utility functions
    {
        std::string text = "Привет 🦊 FoxLang\nСтрока 2";
        // Convert byte offset to line/col
        // 'Привет ' is 12 + 1 = 13 bytes.
        // In UTF-16 code units: 6 chars + 1 space = 7 code units (col 8, 1-based)
        foxlang::SourcePosition pos = foxlang::utf::byteOffsetToSourcePosition(text, 13);
        TEST_ASSERT(pos.line == 1);
        TEST_ASSERT(pos.column == 8);
    }

    std::cout << "POSITIONS_TEST_OK" << std::endl;
    return 0;
}
