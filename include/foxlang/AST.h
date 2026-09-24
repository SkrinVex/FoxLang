#pragma once
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include "foxlang/Context.h"
#include "foxlang/Platform.h"
#include "foxlang/Runtime.h"
#include "foxlang/SourceLocation.h"

namespace foxlang {

struct Node {
    virtual ~Node() = default;
    virtual Value eval(Context& ctx) = 0;
    SourceRange range;
};

// A bool is the only thing a condition may be; an int used to silently count as false.
inline bool conditionTruth(const Value& value, const char* statement) {
    if (value.type != "bool")
        throw std::runtime_error("Type Error: " + std::string(statement) + " condition must be bool, got '" + value.type + "'");
    return value.value == "true";
}

// Hot paths must not pay for an error message they will not use: the description of
// the value is assembled only when the value turns out to be unusable.
inline long long intArg(const Value& value, const char* what) {
    long long result = 0;
    if (runtime::tryInt(value, result)) return result;
    return runtime::toInt(value, what);
}

inline long long intArg(const Value& value, const char* role, const std::string& name) {
    long long result = 0;
    if (runtime::tryInt(value, result)) return result;
    return runtime::toInt(value, role + std::string(" '") + name + "'");
}

// Narrows a value to int text, leaving canonical in-range text untouched.
inline void narrowToInt(Value& value, const char* role, const std::string& name) {
    long long probe = 0;
    if (runtime::tryInt(value, probe)) return;
    value.value = runtime::intText(value, role + std::string(" '") + name + "'");
}

// Runtime errors are reported with the line of the innermost statement that raised them.
inline std::runtime_error located(const std::runtime_error& error, const SourceRange& range) {
    std::string message = error.what();
    if (range.start.line <= 0 || message.find(" [line ") != std::string::npos) return error;
    return std::runtime_error(message + " [line " + std::to_string(range.start.line) + "]");
}

// Runaway recursion used to kill the process with a stack overflow instead of an error.
// What runs out is the native stack, not a number of calls, and one call costs a
// different amount of it per platform and per compiler: counting frames was wrong on
// Windows, where a thread gets 1 MB rather than the 8 MB Linux gives. Measure the stack.
struct CallDepth {
    // Two brakes, because a stack cannot be measured the same way everywhere: the
    // distance actually travelled down the stack, and a frame count derived from the
    // same budget assuming a generous four kilobytes per call. Whichever trips first.
    static constexpr size_t frameCost = 4096;
    explicit CallDepth(const std::string& name) {
        char probe = 0;
        runtime::StackGuard& guard = runtime::stackGuard();
        if (guard.depth == 0) {
            guard.origin = &probe;
            guard.budget = platform::stackBudget();
            guard.limit = static_cast<int>(guard.budget / frameCost);
        } else {
            std::ptrdiff_t used = guard.origin - &probe; // A stack growing upwards never trips this.
            bool spent = (used > 0 && static_cast<size_t>(used) > guard.budget) || guard.depth >= guard.limit;
            if (spent)
                throw std::runtime_error("Runtime Error: call depth limit reached in '" + name + "' after " +
                                         std::to_string(guard.depth) + " nested calls (recursion without a base case?)");
        }
        ++guard.depth;
    }
    ~CallDepth() {
        runtime::StackGuard& guard = runtime::stackGuard();
        if (--guard.depth == 0) guard.origin = nullptr;
    }
    CallDepth(const CallDepth&) = delete;
    CallDepth& operator=(const CallDepth&) = delete;
};

struct FuncDefNode : Node {
    std::string returnType;
    std::string name;
    std::vector<FuncParam> params;
    std::shared_ptr<Node> body;
    SourceRange nameRange;

    FuncDefNode(std::string rt, std::string n, std::vector<FuncParam> p, std::shared_ptr<Node> b, SourceRange nr = {})
        : returnType(std::move(rt)), name(std::move(n)), params(std::move(p)), body(std::move(b)), nameRange(nr) {}

    Value eval(Context& ctx) override {
        ctx.getRoot()->defineFunc(name, std::make_shared<FuncDefNode>(returnType, name, params, body));
        return {"void", ""};
    }
};

struct ReturnNode : Node {
    std::unique_ptr<Node> expr;
    ReturnNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Value result = expr ? expr->eval(ctx) : Value{"void", ""};
        throw ReturnValue{result};
    }
};

struct FuncCallNode : Node {
    std::string name;
    std::vector<std::unique_ptr<Node>> args;
    SourceRange nameRange;

    FuncCallNode(std::string n, std::vector<std::unique_ptr<Node>> a, SourceRange nr = {})
        : name(std::move(n)), args(std::move(a)), nameRange(nr) {}

    Value eval(Context& ctx) override {
        std::vector<Value> argValues;
        argValues.reserve(args.size());
        for (auto& arg : args) argValues.push_back(arg->eval(ctx));

        if (runtime::isBuiltin(name)) {
            if (name == "get" && !argValues.empty() && argValues[0].type != "array" && ctx.getFunc("get")) {
                // Fall through to user-defined get(path, handler)
            } else {
                return runtime::callBuiltin(name, argValues, ctx);
            }
        }

        auto funcNodeBase = ctx.getFunc(name);
        if (!funcNodeBase) {
            throw std::runtime_error("Runtime Error: Function '" + name + "' not found!");
        }

        auto* funcDef = static_cast<FuncDefNode*>(funcNodeBase.get());
        if (argValues.size() != funcDef->params.size()) {
            throw std::runtime_error("Args count mismatch for '" + name + "'");
        }

        // Lexical scope: a function sees globals, never the caller's locals.
        Context funcScope;
        funcScope.parent = ctx.getRoot();
        funcScope.interpreter = ctx.interpreter;

        for (size_t i = 0; i < funcDef->params.size(); i++) {
            funcScope.defineVar(funcDef->params[i].name, funcDef->params[i].type, argValues[i]);
        }

        CallDepth guard(name);
        try {
            funcDef->body->eval(funcScope);
        } catch (const ReturnValue& ret) {
            return ret.value;
        } catch (const BreakException&) {
            throw std::runtime_error("Runtime Error: 'break' outside of loop in function '" + name + "'");
        } catch (const ContinueException&) {
            throw std::runtime_error("Runtime Error: 'continue' outside of loop in function '" + name + "'");
        }

        return {"void", ""};
    }
};

struct NumberNode : Node {
    std::string val;
    bool isFloat;
    NumberNode(std::string v) : val(std::move(v)) {
        isFloat = (val.find('.') != std::string::npos);
    }
    Value eval(Context& /*ctx*/) override { return {isFloat ? "float" : "int", val}; }
};

struct StringNode : Node {
    std::string val;
    StringNode(std::string v) : val(std::move(v)) {}
    Value eval(Context& /*ctx*/) override { return {"string", val}; }
};

struct BoolNode : Node {
    bool val;
    BoolNode(bool v) : val(v) {}
    Value eval(Context& /*ctx*/) override { return {"bool", val ? "true" : "false"}; }
};

struct VarAccessNode : Node {
    std::string name;
    SourceRange nameRange;
    VarAccessNode(std::string n, SourceRange nr = {}) : name(std::move(n)), nameRange(nr) {}
    Value eval(Context& ctx) override { return ctx.getVar(name); }
};

struct VarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e, SourceRange nr = {})
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)), nameRange(nr) {}
    Value eval(Context& ctx) override {
        if (ctx.variables.count(name))
            throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                narrowToInt(val, "variable", name);
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        // An int literal outside the int range used to be stored verbatim and only failed later.
        if (type == "int") narrowToInt(val, "variable", name);
        ctx.defineVar(name, type, val);
        return {"void", ""};
    }
};

struct GlobalVarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    GlobalVarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e, SourceRange nr = {})
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)), nameRange(nr) {}
    Value eval(Context& ctx) override {
        Context* root = ctx.getRoot();
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                narrowToInt(val, "global variable", name);
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize global variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        // An int literal outside the int range used to be stored verbatim and only failed later.
        if (type == "int") narrowToInt(val, "global variable", name);
        root->defineVar(name, type, val);
        return {"void", ""};
    }
};

struct VarAssignNode : Node {
    std::string name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    VarAssignNode(std::string n, std::unique_ptr<Node> e, SourceRange nr = {})
        : name(std::move(n)), expr(std::move(e)), nameRange(nr) {}
    Value eval(Context& ctx) override {
        ctx.setVar(name, expr->eval(ctx));
        return {"void", ""};
    }
};

struct BinOpNode : Node {
    std::string op;
    std::unique_ptr<Node> left, right;
    BinOpNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}

    Value eval(Context& ctx) override {
        // Short-circuit before the right side runs: `x != 0 && 10 / x > 1` must not divide by zero.
        if (op == "&&" || op == "||") {
            bool l = truth(left->eval(ctx), "left");
            if (l == (op == "||")) return {"bool", l ? "true" : "false"};
            return {"bool", truth(right->eval(ctx), "right") ? "true" : "false"};
        }

        Value lval = left->eval(ctx);
        Value rval = right->eval(ctx);

        if (op == "+" || op == "+=") {
            if (lval.type == "string" || rval.type == "string") {
                return {"string", lval.value + rval.value};
            }
            if (lval.type == "float" || rval.type == "float") {
                return {"float", runtime::realResult(number(lval, "left") + number(rval, "right"))};
            }
            return {"int", runtime::intResult(integer(lval, "left") + integer(rval, "right"), op)};
        }

        if (op == "-" || op == "-=" || op == "*" || op == "*=" || op == "/" || op == "/=" || op == "%") {
            if (lval.type == "float" || rval.type == "float") {
                double l = number(lval, "left"), r = number(rval, "right");
                if (r == 0.0 && (op == "/" || op == "/=" || op == "%")) {
                    throw std::runtime_error("Runtime Error: Division by zero");
                }
                double result = (op == "-" || op == "-=") ? l - r :
                                (op == "*" || op == "*=") ? l * r :
                                (op == "/" || op == "/=") ? l / r : std::fmod(l, r);
                return {"float", runtime::realResult(result)};
            }
            long long l = integer(lval, "left"), r = integer(rval, "right");
            if ((op == "/" || op == "/=" || op == "%") && r == 0) {
                throw std::runtime_error("Runtime Error: Division by zero");
            }
            long long result = (op == "-" || op == "-=") ? l - r :
                               (op == "*" || op == "*=") ? l * r :
                               (op == "/" || op == "/=") ? l / r : l % r;
            return {"int", runtime::intResult(result, op)};
        }

        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            bool result;
            if (lval.type == "string" && rval.type == "string") {
                result = (op == "==") ? lval.value == rval.value :
                         (op == "!=") ? lval.value != rval.value :
                         (op == "<") ? lval.value < rval.value :
                         (op == "<=") ? lval.value <= rval.value :
                         (op == ">=") ? lval.value >= rval.value :
                         lval.value > rval.value;
            } else if (lval.type == "bool" && rval.type == "bool") {
                bool l = truth(lval, "left"), r = truth(rval, "right");
                result = (op == "==") ? l == r :
                         (op == "!=") ? l != r :
                         (op == "<") ? l < r :
                         (op == "<=") ? l <= r :
                         (op == ">=") ? l >= r :
                         l > r;
            } else {
                double l = number(lval, "left"), r = number(rval, "right");
                result = (op == "==") ? l == r :
                         (op == "!=") ? l != r :
                         (op == "<") ? l < r :
                         (op == "<=") ? l <= r :
                         (op == ">=") ? l >= r :
                         l > r;
            }
            return {"bool", result ? "true" : "false"};
        }

        return {"void", ""};
    }

private:
    std::string describe(const char* side) const { return std::string(side) + " operand of '" + op + "'"; }
    // Operands are parsed without building any message; only the failing path describes itself.
    double number(const Value& value, const char* side) const {
        double result = 0;
        if (runtime::tryNumber(value, result)) return result;
        return runtime::toNumber(value, describe(side));
    }
    long long integer(const Value& value, const char* side) const {
        long long result = 0;
        if (runtime::tryInt(value, result)) return result;
        return runtime::toInt(value, describe(side));
    }
    bool truth(const Value& value, const char* side) const {
        if (value.type != "bool") throw std::runtime_error("Type Error: " + describe(side) + " must be bool, got '" + value.type + "'");
        return value.value == "true";
    }
};

struct UnaryOpNode : Node {
    std::string op;
    std::unique_ptr<Node> operand;
    UnaryOpNode(std::string o, std::unique_ptr<Node> n) : op(std::move(o)), operand(std::move(n)) {}
    Value eval(Context& ctx) override {
        Value val = operand->eval(ctx);
        if (op == "!") {
            bool b = (val.value == "true");
            return {"bool", b ? "false" : "true"};
        }
        return val;
    }
};

struct PostIncNode : Node {
    std::string name;
    PostIncNode(std::string n) : name(std::move(n)) {}
    Value eval(Context& ctx) override {
        Value current = ctx.getVar(name);
        long long val = intArg(current, "variable", name);
        ctx.setVar(name, {"int", runtime::intResult(val + 1, "++")});
        return {"int", Text::integer(val)};
    }
};

struct ArrayDeclNode : Node {
    std::string name;
    std::unique_ptr<Node> sizeNode;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s) : name(std::move(n)), sizeNode(std::move(s)) {}
    Value eval(Context& ctx) override {
        int sz = static_cast<int>(intArg(sizeNode->eval(ctx), "size of array", name));
        if (sz < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
        ctx.declareArray(name, static_cast<size_t>(sz));
        return {"void", ""};
    }
};

struct ArraySetNode : Node {
    std::string name;
    std::unique_ptr<Node> index, value;
    ArraySetNode(std::string n, std::unique_ptr<Node> i, std::unique_ptr<Node> v)
        : name(std::move(n)), index(std::move(i)), value(std::move(v)) {}
    Value eval(Context& ctx) override {
        Value arrVal = ctx.getVar(name);
        if (arrVal.type != "array") throw std::runtime_error("Runtime Error: '" + name + "' is not an array");
        int idx = static_cast<int>(intArg(index->eval(ctx), "index of array", name));
        auto& arr = ctx.getRoot()->arrays[arrVal.value];
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
            throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(idx));
        }
        arr[idx] = value->eval(ctx);
        return {"void", ""};
    }
};

struct ArrayGetNode : Node {
    std::string name;
    std::unique_ptr<Node> index;
    ArrayGetNode(std::string n, std::unique_ptr<Node> i) : name(std::move(n)), index(std::move(i)) {}
    Value eval(Context& ctx) override {
        Value arrVal = ctx.getVar(name);
        if (arrVal.type != "array") throw std::runtime_error("Runtime Error: '" + name + "' is not an array");
        int idx = static_cast<int>(intArg(index->eval(ctx), "index of array", name));
        auto& arr = ctx.getRoot()->arrays[arrVal.value];
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
            throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(idx));
        }
        return arr[idx];
    }
};

struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> stmts;
    // The program and an imported module are the global scope itself, not a block inside it.
    bool scoped = true;
    Value eval(Context& ctx) override {
        Context inner;
        if (scoped) {
            inner.parent = &ctx;
            inner.interpreter = ctx.interpreter;
        }
        Context& scope = scoped ? inner : ctx;
        for (auto& stmt : stmts) {
            if (!stmt) continue;
            // return/break/continue travel as their own types and pass through untouched.
            try {
                stmt->eval(scope);
            } catch (const std::runtime_error& error) {
                throw located(error, stmt->range);
            }
        }
        return {"void", ""};
    }
};

struct IfNode : Node {
    std::unique_ptr<Node> condition, thenB, elseB;
    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e = nullptr)
        : condition(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
    Value eval(Context& ctx) override {
        bool cond = conditionTruth(condition->eval(ctx), "if");
        if (cond) {
            if (thenB) thenB->eval(ctx);
        } else if (elseB) {
            elseB->eval(ctx);
        }
        return {"void", ""};
    }
};

struct WhileNode : Node {
    std::unique_ptr<Node> condition, body;
    WhileNode(std::unique_ptr<Node> c, std::unique_ptr<Node> b)
        : condition(std::move(c)), body(std::move(b)) {}
    Value eval(Context& ctx) override {
        while (conditionTruth(condition->eval(ctx), "while")) {
            try {
                if (body) body->eval(ctx);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
        return {"void", ""};
    }
};

struct ForNode : Node {
    std::unique_ptr<Node> init, condition, step, body;
    ForNode(std::unique_ptr<Node> i, std::unique_ptr<Node> c, std::unique_ptr<Node> s, std::unique_ptr<Node> b)
        : init(std::move(i)), condition(std::move(c)), step(std::move(s)), body(std::move(b)) {}
    Value eval(Context& ctx) override {
        Context loop;
        loop.parent = &ctx;
        loop.interpreter = ctx.interpreter;
        if (init) init->eval(loop);
        while (!condition || conditionTruth(condition->eval(loop), "for")) {
            try {
                if (body) body->eval(loop);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // continue: evaluate step and continue
            }
            if (step) step->eval(loop);
        }
        return {"void", ""};
    }
};

struct BreakNode : Node {
    Value eval(Context& /*ctx*/) override {
        throw BreakException{};
    }
};

struct ContinueNode : Node {
    Value eval(Context& /*ctx*/) override {
        throw ContinueException{};
    }
};

struct WaitNode : Node {
    std::unique_ptr<Node> timeExpr;
    WaitNode(std::unique_ptr<Node> t) : timeExpr(std::move(t)) {}
    Value eval(Context& ctx) override {
        int milliseconds = static_cast<int>(intArg(timeExpr->eval(ctx), "wait() milliseconds"));
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return {"void", ""};
    }
};

struct SwitchNode : Node {
    std::unique_ptr<Node> expr;
    std::vector<std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>>> cases;
    std::unique_ptr<Node> defaultCase;

    SwitchNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}

    Value eval(Context& ctx) override {
        Value switchValue = expr->eval(ctx);
        bool executed = false;
        bool fallthrough = false;

        for (auto& caseItem : cases) {
            if (!executed && !fallthrough) {
                Value caseValue = caseItem.first->eval(ctx);
                if (switchValue.value == caseValue.value) {
                    executed = true;
                    fallthrough = true;
                }
            }

            if (fallthrough) {
                try {
                    caseItem.second->eval(ctx);
                } catch (const BreakException&) {
                    fallthrough = false;
                    break;
                }
            }
        }

        if (!executed && defaultCase) {
            defaultCase->eval(ctx);
        }

        return {"void", ""};
    }
};

struct InputNode : Node {
    Value eval(Context& /*ctx*/) override {
        std::string input;
        std::getline(std::cin, input);
        return {"string", input};
    }
};

struct ReadFileNode : Node {
    std::unique_ptr<Node> filename;
    ReadFileNode(std::unique_ptr<Node> fn) : filename(std::move(fn)) {}

    Value eval(Context& ctx) override {
        Value fnVal = filename->eval(ctx);
        std::vector<Value> args{fnVal};
        return runtime::callBuiltin("read_file", args, ctx);
    }
};

// Forward declaration of interpreter execution hook
void executeIncludeHook(const std::string& path, Context& ctx, const std::string& currentFile, bool importOnly);
void executeUsingHook(const std::string& libName, Context& ctx, const std::string& currentFile);

struct UsingNode : Node {
    std::string libName;
    std::string currentFile;
    UsingNode(std::string lib, std::string curFile = "")
        : libName(std::move(lib)), currentFile(std::move(curFile)) {}
    Value eval(Context& ctx) override {
        executeUsingHook(libName, ctx, currentFile);
        return {"void", ""};
    }
};

struct IncludeNode : Node {
    std::string filename;
    std::string currentFile;
    IncludeNode(std::string file, std::string curFile = "")
        : filename(std::move(file)), currentFile(std::move(curFile)) {}
    Value eval(Context& ctx) override {
        executeIncludeHook(filename, ctx, currentFile, true);
        return {"void", ""};
    }
};

} // namespace foxlang

// Compatibility aliases
using foxlang::Node;
using foxlang::FuncDefNode;
using foxlang::ReturnNode;
using foxlang::FuncCallNode;
using foxlang::NumberNode;
using foxlang::StringNode;
using foxlang::BoolNode;
using foxlang::VarAccessNode;
using foxlang::VarDeclNode;
using foxlang::GlobalVarDeclNode;
using foxlang::VarAssignNode;
using foxlang::BinOpNode;
using foxlang::UnaryOpNode;
using foxlang::PostIncNode;
using foxlang::ArrayDeclNode;
using foxlang::ArraySetNode;
using foxlang::ArrayGetNode;
using foxlang::BlockNode;
using foxlang::IfNode;
using foxlang::WhileNode;
using foxlang::ForNode;
using foxlang::BreakNode;
using foxlang::ContinueNode;
using foxlang::WaitNode;
using foxlang::SwitchNode;
using foxlang::InputNode;
using foxlang::ReadFileNode;
using foxlang::UsingNode;
using foxlang::IncludeNode;
