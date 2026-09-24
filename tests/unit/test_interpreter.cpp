#include "foxlang/FoxLang.h"
#include <iostream>
#include <cassert>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int main() {
    foxlang::Interpreter interpreter;

    // 1. Basic execution and variable retrieval
    {
        auto res = interpreter.runSource("int x = 40 + 2; string s = \"FoxLang\";");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interpreter.getGlobal("x").value == "42");
        TEST_ASSERT(interpreter.getGlobal("s").value == "FoxLang");
    }

    // 2. Set global and use in script
    {
        interpreter.setGlobal("injected", "int", "100");
        auto res = interpreter.runSource("int total = injected + 50;");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interpreter.getGlobal("total").value == "150");
    }

    // 3. Reset clears state
    {
        interpreter.reset();
        bool threw = false;
        try {
            interpreter.getGlobal("x");
        } catch (const std::exception&) {
            threw = true;
        }
        TEST_ASSERT(threw);
    }

    // 4. Function call and recursion
    {
        const char* code = R"(
            int fact(int n) {
                if (n <= 1) { return 1; }
                return n * fact(n - 1);
            }
            int r = fact(5);
        )";
        auto res = interpreter.runSource(code);
        TEST_ASSERT(res.success);
        TEST_ASSERT(interpreter.getGlobal("r").value == "120");
    }

    // 5. Division by zero cleanly caught
    {
        auto res = interpreter.runSource("int bad = 10 / 0;");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Division by zero") != std::string::npos);
    }

    // 6. Array bounds cleanly caught
    {
        const char* code = R"(
            array a 3;
            set(a, 10, 999);
        )";
        auto res = interpreter.runSource(code);
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("Array index out of bounds") != std::string::npos);
    }

    // 7. Isolation between multiple Interpreter instances in same process
    {
        foxlang::Interpreter interpA;
        foxlang::Interpreter interpB;

        auto resA = interpA.runSource("int varA = 111; int getVal() { return 1000; }");
        auto resB = interpB.runSource("int varB = 222; int getVal() { return 2000; }");

        TEST_ASSERT(resA.success && resB.success);
        TEST_ASSERT(interpA.getGlobal("varA").value == "111");
        TEST_ASSERT(interpB.getGlobal("varB").value == "222");

        // interpA should not see varB and interpB should not see varA
        bool aHasB = false;
        try { interpA.getGlobal("varB"); aHasB = true; } catch (...) {}
        TEST_ASSERT(!aHasB);

        bool bHasA = false;
        try { interpB.getGlobal("varA"); bHasA = true; } catch (...) {}
        TEST_ASSERT(!bHasA);

        auto callA = interpA.runSource("int checkA = getVal();");
        auto callB = interpB.runSource("int checkB = getVal();");
        TEST_ASSERT(callA.success && callB.success);
        TEST_ASSERT(interpA.getGlobal("checkA").value == "1000");
        TEST_ASSERT(interpB.getGlobal("checkB").value == "2000");
    }

    // 8. Repeated runSource() on the same Interpreter
    {
        foxlang::Interpreter interp;
        auto res1 = interp.runSource("int counter = 10; int addStep(int x) { return x + 5; }");
        TEST_ASSERT(res1.success);

        auto res2 = interp.runSource("counter = addStep(counter);");
        TEST_ASSERT(res2.success);
        TEST_ASSERT(interp.getGlobal("counter").value == "15");

        auto res3 = interp.runSource("counter = addStep(counter);");
        TEST_ASSERT(res3.success);
        TEST_ASSERT(interp.getGlobal("counter").value == "20");
    }

    // 9. Function defined inside nested block
    {
        foxlang::Interpreter interp;
        const char* code = R"(
            if (true) {
                int nestedFunc(int val) {
                    return val * 3;
                }
            }
            int result = nestedFunc(4);
        )";
        auto res = interp.runSource(code);
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getGlobal("result").value == "12");
    }

    // 10. Exception handling via public API
    {
        foxlang::Interpreter interp;
        // Syntax error
        auto resSyntax = interp.runSource("int x = ;");
        TEST_ASSERT(!resSyntax.success);
        TEST_ASSERT(resSyntax.exitCode != 0);
        TEST_ASSERT(!resSyntax.errorMessage.empty());

        // Type error
        auto resType = interp.runSource("bool b = 123;");
        TEST_ASSERT(!resType.success);
        TEST_ASSERT(resType.exitCode != 0);
        TEST_ASSERT(resType.errorMessage.find("Type Error") != std::string::npos);

        // Undefined variable
        auto resUndef = interp.runSource("int z = not_exists + 1;");
        TEST_ASSERT(!resUndef.success);
        TEST_ASSERT(resUndef.exitCode != 0);
        TEST_ASSERT(resUndef.errorMessage.find("Variable 'not_exists' not found") != std::string::npos);

        // Non-existent file
        auto resFile = interp.runFile("non_existent_file_12345.fox");
        TEST_ASSERT(!resFile.success);
        TEST_ASSERT(resFile.exitCode != 0);
        TEST_ASSERT(resFile.errorMessage.find("could not open file") != std::string::npos);
    }

    // 11. Float arithmetic keeps full double precision between operations
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource("float third = 1.0 / 3.0; float back = third * 3.0; float sum = 0.1 + 0.2;");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getGlobal("third").value == "0.3333333333333333");
        TEST_ASSERT(interp.getGlobal("back").value == "1");
        TEST_ASSERT(interp.getGlobal("sum").value == "0.30000000000000004");
    }

    // 12. int stays inside its range instead of wrapping or leaking std::stoi
    {
        foxlang::Interpreter interp;
        auto tooBig = interp.runSource("int big = 3000000000;");
        TEST_ASSERT(!tooBig.success);
        TEST_ASSERT(tooBig.errorMessage.find("does not fit in int") != std::string::npos);

        auto overflow = interp.runSource("int a = 2000000000; int b = a + a;");
        TEST_ASSERT(!overflow.success);
        TEST_ASSERT(overflow.errorMessage.find("int overflow") != std::string::npos);
    }

    // 13. Runtime errors name the line they happened on
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource("int ok = 1;\nint bad = 10 / 0;");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("[line 2]") != std::string::npos);
    }

    // 14. Runaway recursion is an error, not a stack overflow
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource("int forever(int n) { return forever(n + 1); } int r = forever(0);");
        TEST_ASSERT(!res.success);
        TEST_ASSERT(res.errorMessage.find("call depth limit") != std::string::npos);
    }

    // 15. Functions see globals, never the caller's locals
    {
        foxlang::Interpreter interp;
        const char* code = R"(
            global int shared = 7;
            int readShared() { return shared; }
            int caller() { int shared = 99; return readShared(); }
            int seen = caller();
        )";
        auto res = interp.runSource(code);
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getGlobal("seen").value == "7");

        auto leak = interp.runSource(
            "int reader() { return hidden; } int owner() { int hidden = 1; return reader(); } int x = owner();");
        TEST_ASSERT(!leak.success);
        TEST_ASSERT(leak.errorMessage.find("Variable 'hidden' not found") != std::string::npos);
    }

    // 16. Array buffers are released with their scope and on re-declaration
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource(R"(
            void churn() { array tmp 16; set(tmp, 0, 1); }
            void loop() { int i = 0; while (i < 500) { array local 16; churn(); i++; } }
            loop();
        )");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getContext().arrays.empty());

        auto kept = interp.runSource("array survivor 4; set(survivor, 0, 5);");
        TEST_ASSERT(kept.success);
        TEST_ASSERT(interp.getContext().arrays.size() == 1);
    }

    // 17. A block is a scope of its own, and the program top level is not
    {
        foxlang::Interpreter interp;
        auto leak = interp.runSource("int i = 0; while (i < 1) { int inner = 5; i++; } int seen = inner;");
        TEST_ASSERT(!leak.success);
        TEST_ASSERT(leak.errorMessage.find("Variable 'inner' not found") != std::string::npos);

        foxlang::Interpreter globals;
        auto top = globals.runSource("int kept = 3; { int hidden = 4; kept = kept + hidden; }");
        TEST_ASSERT(top.success);
        TEST_ASSERT(globals.getGlobal("kept").value == "7");

        auto twice = globals.runSource("int kept = 1;");
        TEST_ASSERT(!twice.success);
        TEST_ASSERT(twice.errorMessage.find("already declared in this scope") != std::string::npos);
    }

    // 18. && and || stop before evaluating the right side
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource(
            "int zero = 0; bool guarded = zero != 0 && 10 / zero > 1; bool other = zero == 0 || 10 / zero > 1;");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getGlobal("guarded").value == "false");
        TEST_ASSERT(interp.getGlobal("other").value == "true");
    }

    // 19. A number carries its binary form but still prints the text it always did
    {
        foxlang::Interpreter interp;
        auto res = interp.runSource(
            "int n = 21 * 2; string label = \"n = \" + n;"
            "float quarter = 1.0 / 4.0; string shown = \"\" + quarter;"
            "float tiny = 2.0 / 3.0; string precise = \"\" + tiny;"
            "bool same = n == 42;");
        TEST_ASSERT(res.success);
        TEST_ASSERT(interp.getGlobal("label").value == "n = 42");
        TEST_ASSERT(interp.getGlobal("shown").value == "0.25");
        TEST_ASSERT(interp.getGlobal("precise").value == "0.6666666666666666");
        TEST_ASSERT(interp.getGlobal("same").value == "true");
    }

    std::cout << "TEST_INTERPRETER_OK" << std::endl;
    return 0;
}
