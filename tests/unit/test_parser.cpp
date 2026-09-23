#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
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
    // 1. Precedence: 2 + 3 * 4 parses as 2 + (3 * 4)
    {
        foxlang::Lexer lexer("int res = 2 + 3 * 4;");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        TEST_ASSERT(program->stmts.size() == 1);
        auto* decl = dynamic_cast<foxlang::VarDeclNode*>(program->stmts[0].get());
        TEST_ASSERT(decl != nullptr);
        TEST_ASSERT(decl->name == "res");
        auto* plusOp = dynamic_cast<foxlang::BinOpNode*>(decl->expr.get());
        TEST_ASSERT(plusOp != nullptr && plusOp->op == "+");
        auto* leftNum = dynamic_cast<foxlang::NumberNode*>(plusOp->left.get());
        TEST_ASSERT(leftNum != nullptr && leftNum->val == "2");
        auto* rightMul = dynamic_cast<foxlang::BinOpNode*>(plusOp->right.get());
        TEST_ASSERT(rightMul != nullptr && rightMul->op == "*");
    }

    // 2. Adjacent string literal concatenation in AST
    {
        foxlang::Lexer lexer("string s = \"Hello\" \", \" \"World!\";");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        TEST_ASSERT(program->stmts.size() == 1);
        auto* decl = dynamic_cast<foxlang::VarDeclNode*>(program->stmts[0].get());
        TEST_ASSERT(decl != nullptr);
        auto* strNode = dynamic_cast<foxlang::StringNode*>(decl->expr.get());
        TEST_ASSERT(strNode != nullptr);
        TEST_ASSERT(strNode->val == "Hello, World!");
    }

    // 3. Unary minus and NOT
    {
        foxlang::Lexer lexer("int a = -10; bool b = !false;");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        TEST_ASSERT(program->stmts.size() == 2);
    }

    // 4. Function definition AST
    {
        foxlang::Lexer lexer("int add(int a, int b) { return a + b; }");
        foxlang::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        TEST_ASSERT(program->stmts.size() == 1);
        auto* func = dynamic_cast<foxlang::FuncDefNode*>(program->stmts[0].get());
        TEST_ASSERT(func != nullptr);
        TEST_ASSERT(func->name == "add");
        TEST_ASSERT(func->returnType == "int");
        TEST_ASSERT(func->params.size() == 2);
        TEST_ASSERT(func->params[0].name == "a" && func->params[0].type == "int");
        TEST_ASSERT(func->params[1].name == "b" && func->params[1].type == "int");
    }

    // 5. Syntax error handling
    {
        bool threw = false;
        try {
            foxlang::Lexer lexer("int a = ;");
            foxlang::Parser parser(lexer.tokenize());
            parser.parseProgram();
        } catch (const std::runtime_error& e) {
            threw = true;
        }
        TEST_ASSERT(threw);
    }

    std::cout << "TEST_PARSER_OK" << std::endl;
    return 0;
}
