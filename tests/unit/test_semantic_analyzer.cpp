#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/SemanticAnalyzer.h"
#include "foxlang/Project.h"
#include <filesystem>
#include <set>
#include <fstream>
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
    // Graphics metadata is available without opening a window during analysis.
    {
        foxlang::Lexer lexer("using graphics; open_window(960, 640, \"Fox\"); draw_text(1, 2, \"Hi\", 2, rgb(255, 0, 0)); bool ready = window_poll(); close_window();");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        foxlang::SemanticAnalyzer analyzer;
        analyzer.analyze(program.get());
        TEST_ASSERT(analyzer.getDiagnostics().empty());
    }
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

    auto analyze = [](const std::string& src, foxlang::SemanticAnalyzer& analyzer) {
        foxlang::Lexer lexer(src, true);
        foxlang::Parser parser(lexer.tokenize(), "memory.fox");
        std::vector<foxlang::Diagnostic> syntax;
        auto prog = parser.parseProgramWithDiagnostics(syntax);
        analyzer.analyze(prog.get());
        return syntax.size();
    };
    auto mentions = [](const foxlang::SemanticAnalyzer& analyzer, const std::string& text) {
        for (const auto& d : analyzer.getDiagnostics())
            if (d.message.find(text) != std::string::npos) return true;
        return false;
    };

    // 11. Functions may call functions defined later in the file
    {
        foxlang::SemanticAnalyzer analyzer;
        TEST_ASSERT(analyze("void main() { helper(2); }\nvoid helper(int n) { print(n); }\nmain();", analyzer) == 0);
        TEST_ASSERT(analyzer.getDiagnostics().empty());
    }

    // 11b. A function body sees globals declared below it; top-level code does not
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("void show() { print(limit, names); }\nint early = later;\nint limit = 3;\narray names;\nint later = 1;\nshow();", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().size() == 1);
        TEST_ASSERT(mentions(analyzer, "Undefined variable 'later'"));
        TEST_ASSERT(analyzer.getDiagnostics()[0].range.start.line == 2);
    }

    // 12. Builtin calls are checked against the catalog, including optional and variadic parameters
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("print(); print(1, \"a\", true); string a = input(); string b = input(\"?\");\n"
                "string c = input(\"a\", \"b\");\nint n = size();", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().size() == 2);
        TEST_ASSERT(mentions(analyzer, "'input' expects 0 to 1 arguments, but got 2"));
        TEST_ASSERT(mentions(analyzer, "'size' expects 1 arguments, but got 0"));
    }

    // 13. Module functions are documented by their /// comments; module variables are known
    {
        foxlang::SemanticAnalyzer analyzer;
        std::string src = "using string;\nusing math;\nstring p = pad_left(\"7\", 3, \"0\");\nfloat t = PI * 2;";
        analyze(src, analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().empty());
        auto hover = analyzer.getHover(3, 13, src);
        TEST_ASSERT(hover.found && hover.markdown.find("Дополняет строку") != std::string::npos);
        auto wrapper = analyzer.getHover(3, 13, src);
        TEST_ASSERT(wrapper.markdown.find("pad_left(string text, int width, string fill) -> string") != std::string::npos);
    }

    // 14. A thin wrapper inherits the documentation of the builtin it calls
    {
        foxlang::SemanticAnalyzer analyzer;
        std::string src = "using string;\nstring u = upper(\"a\");";
        analyze(src, analyzer);
        auto hover = analyzer.getHover(2, 12, src);
        TEST_ASSERT(hover.found && hover.markdown.find("прописными") != std::string::npos);
    }

    // 15. A builtin and a module function sharing a name accept either signature
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("using server;\narray a = [1];\nint x = get(a, 0);\nget(\"/\", \"h\");\nget(1, 2, 3);", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().size() == 1);
        TEST_ASSERT(analyzer.getDiagnostics()[0].range.start.line == 5);
    }

    // 16. return outside a function, missing return values, undefined array names
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("return 1;\nint f() { return; }\nmissing[0] = 1;\nint y = other[2];\nq++;", analyzer);
        TEST_ASSERT(mentions(analyzer, "'return' outside of a function"));
        TEST_ASSERT(mentions(analyzer, "must return a value"));
        TEST_ASSERT(mentions(analyzer, "Undefined variable 'missing'"));
        TEST_ASSERT(mentions(analyzer, "Undefined variable 'other'"));
        TEST_ASSERT(mentions(analyzer, "Undefined variable 'q'"));
    }

    // 16b. Lambdas, func variables and function names used as values
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("int add(int a, int b) { return a + b; }\nfunc plus = add;\nfunc p = print;\n"
                "func twice = (int x) => x * 2;\nvoid run(f, func g) { f(1); g(2); }\n"
                "void local() { func fact = (int n) => { if (n <= 1) { return 1; } return n * fact(n - 1); }; fact(3); }\n"
                "print(plus(1, 2), twice(3), array_map([1], x => x + 1));", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().empty());
        foxlang::SemanticAnalyzer wrong;
        analyze("int n = 1;\nn(2);\nfunc f = (x) => y;", wrong);
        TEST_ASSERT(mentions(wrong, "'n' is a int variable, not a function"));
        TEST_ASSERT(mentions(wrong, "Undefined variable 'y'"));
    }

    // 16c. null only where the type says T?
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("string? a = null;\na = null;\nint? f(Point? p) { return null; }\nstruct Point { int x; Point? next; }\n"
                "Point q = Point(1);\nprint(q.next?.x ?? 0, f(null));", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().empty());
        foxlang::SemanticAnalyzer wrong;
        analyze("string a = null;\nint b = 1;\nb = null;\nint f(string s) { return null; }\nf(null);", wrong);
        TEST_ASSERT(mentions(wrong, "Variable 'a' of type string cannot be null"));
        TEST_ASSERT(mentions(wrong, "Variable 'b' of type int cannot be null"));
        TEST_ASSERT(mentions(wrong, "Function returning int cannot return null"));
        TEST_ASSERT(mentions(wrong, "Parameter 's' of 'f' has type string and cannot be null"));
    }

    // 17. Unknown modules are reported as warnings
    {
        foxlang::SemanticAnalyzer analyzer;
        analyze("using definitely_missing_module;", analyzer);
        TEST_ASSERT(analyzer.getDiagnostics().size() == 1);
        TEST_ASSERT(analyzer.getDiagnostics()[0].severity == foxlang::DiagnosticSeverity::Warning);
    }

    // 18. Every standard module describes itself
    {
        const auto& modules = foxlang::standardModules();
        TEST_ASSERT(modules.size() == 16);
        for (const auto& module : modules) TEST_ASSERT(!module.documentation.empty());
    }

    // 19. Files of one program see each other; two programs sharing a library do not
    {
        namespace fs = std::filesystem;
        // Paths are UTF-8 text in FoxLang; on Windows a narrow literal would be read as ANSI.
        fs::path dir = fs::temp_directory_path() / fs::u8path("foxlang project тест");
        auto utf8 = [](const fs::path& path) { return foxlang::platform::pathToUtf8(path); };
        fs::remove_all(dir);
        fs::create_directories(dir / "lib");
        auto write = [&](const std::string& name, const std::string& text) { std::ofstream(dir / fs::u8path(name)) << text; };
        write("main.fox", "include(\"lib/utils.fox\");\ninclude(\"render.fox\");\nint counter = 0;\n");
        write("lib/utils.fox", "string shout(string t) { counter++; return t; }\n");
        write("render.fox", "void show() { print(shout(\"x\"), counter); }\n");
        write("other.fox", "include(\"lib/utils.fox\");\nint private_value = 1;\n");
        write("alone.fox", "int single = 1;\n");
        auto sources = std::make_shared<foxlang::OverlaySources>(foxlang::filesystemSources());
        foxlang::ProjectIndex index(sources);
        index.setRoot(utf8(dir));
        auto name = [](const std::string& path) { return foxlang::platform::pathToUtf8(foxlang::platform::pathFromUtf8(path).filename()); };
        auto peers = index.peers(utf8(dir / "render.fox"));
        TEST_ASSERT(peers.size() == 2);
        std::set<std::string> names;
        for (const auto& p : peers) names.insert(name(p));
        TEST_ASSERT(names == (std::set<std::string>{"main.fox", "utils.fox"}));
        // utils.fox belongs to both programs; main.fox and other.fox stay apart.
        TEST_ASSERT(index.peers(utf8(dir / "lib" / "utils.fox")).size() == 3);
        TEST_ASSERT(index.peers(utf8(dir / "other.fox")).size() == 1);
        TEST_ASSERT(index.peers(utf8(dir / "alone.fox")).empty());

        std::string code = "void show() { print(shout(\"x\"), counter); }\n";
        foxlang::Lexer lexer(code, true);
        foxlang::Parser parser(lexer.tokenize(), utf8(dir / "render.fox"));
        std::vector<foxlang::Diagnostic> syntax;
        auto prog = parser.parseProgramWithDiagnostics(syntax);
        foxlang::SemanticAnalyzer analyzer(utf8(dir / "render.fox"), "", sources);
        analyzer.addProjectFiles(peers);
        analyzer.analyze(prog.get());
        TEST_ASSERT(analyzer.getDiagnostics().empty());
        auto definition = analyzer.getDefinition(1, 22);
        TEST_ASSERT(definition.found && name(definition.fileUri) == "utils.fox");

        // An unsaved buffer counts: removing the include detaches render.fox.
        sources->set(utf8(dir / "main.fox"), "include(\"lib/utils.fox\");\nint counter = 0;\n");
        TEST_ASSERT(index.peers(utf8(dir / "render.fox")).empty());
        fs::remove_all(dir);
    }

        std::cout << "SEMANTIC_ANALYZER_TEST_OK" << std::endl;
    return 0;
}
