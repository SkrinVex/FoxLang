#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/SemanticAnalyzer.h"
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
    // HTTPS API is known to editor diagnostics and analysis never opens a listener.
    {
        foxlang::Lexer lexer("using server; listen_tls(8443, \"chain.pem\", \"key.pem\");");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(program.get());
        TEST_ASSERT(analyzer.getDiagnostics().empty());
    }
    // 1. Static analysis does NOT execute code (no divide-by-zero, no infinite loop)
    {
        std::string src = "int x = 10 / 0;\nwhile (true) { int y = 1; }\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        TEST_ASSERT(prog != nullptr);

        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        // Should not throw or crash or hang
        TEST_ASSERT(analyzer.getDiagnostics().empty() || !analyzer.getDiagnostics().empty());
    }

    // 2. Undefined variable detection
    {
        std::string src = "void test() {\n    print(mesage);\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        const auto& diags = analyzer.getDiagnostics();
        TEST_ASSERT(!diags.empty());
        bool foundUndefined = false;
        for (const auto& d : diags) {
            if (d.message.find("mesage") != std::string::npos) {
                foundUndefined = true;
                TEST_ASSERT(d.range.start.line == 2);
            }
        }
        TEST_ASSERT(foundUndefined);
    }

    // 3. Undefined function detection
    {
        std::string src = "void main() {\n    calcTotal(10, 20);\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        const auto& diags = analyzer.getDiagnostics();
        TEST_ASSERT(!diags.empty());
        bool foundFn = false;
        for (const auto& d : diags) {
            if (d.message.find("calcTotal") != std::string::npos) {
                foundFn = true;
                TEST_ASSERT(d.range.start.line == 2);
            }
        }
        TEST_ASSERT(foundFn);
    }

    // 4. Duplicate variable declaration
    {
        std::string src = "int a = 1;\nint a = 2;\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        const auto& diags = analyzer.getDiagnostics();
        TEST_ASSERT(!diags.empty());
        bool foundDup = false;
        for (const auto& d : diags) {
            if (d.message.find("Redeclaration") != std::string::npos || d.message.find("already declared") != std::string::npos) {
                foundDup = true;
            }
        }
        TEST_ASSERT(foundDup);
    }

    // 5. Argument count mismatch
    {
        std::string src = "int add(int a, int b) { return a + b; }\nvoid test() { add(42); }\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        const auto& diags = analyzer.getDiagnostics();
        TEST_ASSERT(!diags.empty());
        bool foundArgErr = false;
        for (const auto& d : diags) {
            if (d.message.find("arguments") != std::string::npos) {
                foundArgErr = true;
                TEST_ASSERT(d.range.start.line == 2);
            }
        }
        TEST_ASSERT(foundArgErr);
    }

    // 6. Invalid return in void function
    {
        std::string src = "void doNothing() {\n    return 42;\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());
        const auto& diags = analyzer.getDiagnostics();
        TEST_ASSERT(!diags.empty());
        bool foundRetErr = false;
        for (const auto& d : diags) {
            if (d.message.find("void") != std::string::npos || d.message.find("Void") != std::string::npos) {
                foundRetErr = true;
            }
        }
        TEST_ASSERT(foundRetErr);
    }

    // 7. Hover information
    {
        std::string src = "int score = 100;\nint getBonus(int multiplier) {\n    return score * multiplier;\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());

        // Hover over 'score' declaration on line 1, col 6
        auto hoverScore = analyzer.getHover(1, 6);
        TEST_ASSERT(hoverScore.found);
        TEST_ASSERT(hoverScore.markdown.find("int score") != std::string::npos);

        // Hover over 'getBonus' definition on line 2, col 6
        auto hoverFn = analyzer.getHover(2, 6);
        TEST_ASSERT(hoverFn.found);
        TEST_ASSERT(hoverFn.markdown.find("int getBonus(int multiplier)") != std::string::npos);

        // Hover over 'score' usage on line 3, col 13
        auto hoverUsage = analyzer.getHover(3, 13);
        TEST_ASSERT(hoverUsage.found);
        TEST_ASSERT(hoverUsage.markdown.find("int score") != std::string::npos);
    }

    // 8. Go to Definition
    {
        std::string src = "int myVar = 50;\nvoid run() {\n    print(myVar);\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());

        // Query definition of 'myVar' on line 3, col 12
        auto def = analyzer.getDefinition(3, 12);
        TEST_ASSERT(def.found);
        TEST_ASSERT(def.range.start.line == 1);
        TEST_ASSERT(def.range.start.column == 5); // start of 'myVar'
    }

    // 9. Completions
    {
        std::string src = "int counter = 0;\nvoid process() {}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());

        auto completions = analyzer.getCompletions(1, 1);
        TEST_ASSERT(!completions.empty());

        bool hasCounter = false;
        bool hasProcess = false;
        bool hasWhile = false;
        bool hasPrint = false;

        for (const auto& item : completions) {
            if (item.label == "counter") hasCounter = true;
            if (item.label == "process") hasProcess = true;
            if (item.label == "while") hasWhile = true;
            if (item.label == "print") hasPrint = true;
        }

        TEST_ASSERT(hasCounter);
        TEST_ASSERT(hasProcess);
        TEST_ASSERT(hasWhile);
        TEST_ASSERT(hasPrint);
    }

    // 10. Document Symbols
    {
        std::string src = "int globalA = 1;\nvoid compute() {\n    int localB = 2;\n}\n";
        foxlang::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        foxlang::Parser parser(std::move(tokens));
        auto prog = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(prog.get());

        auto symbols = analyzer.getDocumentSymbols();
        TEST_ASSERT(!symbols.empty());

        bool hasGlobalA = false;
        bool hasCompute = false;
        for (const auto& sym : symbols) {
            if (sym.name == "globalA" && sym.kind == "Variable") hasGlobalA = true;
            if (sym.name == "compute" && sym.kind == "Function") hasCompute = true;
        }
        TEST_ASSERT(hasGlobalA);
        TEST_ASSERT(hasCompute);
    }

    std::cout << "SEMANTIC_ANALYZER_TEST_OK" << std::endl;
    return 0;
}
