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

    std::cout << "TEST_INTERPRETER_OK" << std::endl;
    return 0;
}
