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

    std::cout << "TEST_INTERPRETER_OK" << std::endl;
    return 0;
}
