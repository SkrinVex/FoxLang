#pragma once
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>
#include "foxlang/Context.h"
#include "foxlang/Runtime.h"

namespace foxlang {

struct Node {
    virtual ~Node() = default;
    virtual Value eval(Context& ctx) = 0;
};

struct FuncDefNode : Node {
    std::string returnType;
    std::string name;
    std::vector<FuncParam> params;
    std::shared_ptr<Node> body;

    FuncDefNode(std::string rt, std::string n, std::vector<FuncParam> p, std::shared_ptr<Node> b)
        : returnType(std::move(rt)), name(std::move(n)), params(std::move(p)), body(std::move(b)) {}

    Value eval(Context& ctx) override {
        ctx.defineFunc(name, std::make_shared<FuncDefNode>(returnType, name, params, body));
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

    FuncCallNode(std::string n, std::vector<std::unique_ptr<Node>> a)
        : name(std::move(n)), args(std::move(a)) {}

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

        Context funcScope;
        funcScope.parent = &ctx;
        funcScope.interpreter = ctx.interpreter;

        for (size_t i = 0; i < funcDef->params.size(); i++) {
            funcScope.defineVar(funcDef->params[i].name, funcDef->params[i].type, argValues[i]);
        }

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
    VarAccessNode(std::string n) : name(std::move(n)) {}
    Value eval(Context& ctx) override { return ctx.getVar(name); }
};

struct VarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e)
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                val.value = std::to_string(static_cast<int>(std::stod(val.value)));
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        ctx.defineVar(name, type, val);
        return {"void", ""};
    }
};

struct GlobalVarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    GlobalVarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e)
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Context* root = ctx.getRoot();
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                val.value = std::to_string(static_cast<int>(std::stod(val.value)));
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize global variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        root->defineVar(name, type, val);
        return {"void", ""};
    }
};

struct VarAssignNode : Node {
    std::string name;
    std::unique_ptr<Node> expr;
    VarAssignNode(std::string n, std::unique_ptr<Node> e) : name(std::move(n)), expr(std::move(e)) {}
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
        Value lval = left->eval(ctx);
        Value rval = right->eval(ctx);

        if (op == "+" || op == "+=") {
            if (lval.type == "string" || rval.type == "string") {
                return {"string", lval.value + rval.value};
            }
            if (lval.type == "float" || rval.type == "float") {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                return {"float", runtime::formatNumber(l + r)};
            }
            int l = std::stoi(lval.value), r = std::stoi(rval.value);
            return {"int", std::to_string(l + r)};
        }

        if (op == "-" || op == "-=" || op == "*" || op == "*=" || op == "/" || op == "/=" || op == "%") {
            if (lval.type == "float" || rval.type == "float") {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                if ((op == "/" || op == "/=") && r == 0.0) {
                    throw std::runtime_error("Runtime Error: Division by zero");
                }
                double result = (op == "-" || op == "-=") ? l - r :
                                (op == "*" || op == "*=") ? l * r :
                                (op == "/" || op == "/=") ? l / r : std::fmod(l, r);
                return {"float", runtime::formatNumber(result)};
            }
            int l = std::stoi(lval.value), r = std::stoi(rval.value);
            if ((op == "/" || op == "/=" || op == "%") && r == 0) {
                throw std::runtime_error("Runtime Error: Division by zero");
            }
            int result = (op == "-" || op == "-=") ? l - r :
                         (op == "*" || op == "*=") ? l * r :
                         (op == "/" || op == "/=") ? l / r : l % r;
            return {"int", std::to_string(result)};
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
                bool l = (lval.value == "true"), r = (rval.value == "true");
                result = (op == "==") ? l == r :
                         (op == "!=") ? l != r :
                         (op == "<") ? l < r :
                         (op == "<=") ? l <= r :
                         (op == ">=") ? l >= r :
                         l > r;
            } else {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                result = (op == "==") ? l == r :
                         (op == "!=") ? l != r :
                         (op == "<") ? l < r :
                         (op == "<=") ? l <= r :
                         (op == ">=") ? l >= r :
                         l > r;
            }
            return {"bool", result ? "true" : "false"};
        }

        if (op == "&&" || op == "||") {
            bool l = (lval.value == "true"), r = (rval.value == "true");
            bool result = (op == "&&") ? (l && r) : (l || r);
            return {"bool", result ? "true" : "false"};
        }

        return {"void", ""};
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
        int val = std::stoi(current.value);
        ctx.setVar(name, {"int", std::to_string(val + 1)});
        return {"int", std::to_string(val)};
    }
};

struct ArrayDeclNode : Node {
    std::string name;
    std::unique_ptr<Node> sizeNode;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s) : name(std::move(n)), sizeNode(std::move(s)) {}
    Value eval(Context& ctx) override {
        int sz = std::stoi(sizeNode->eval(ctx).value);
        if (sz < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
        static std::atomic<int> arrayCounter{0};
        std::string arrayId = "__arr_" + std::to_string(arrayCounter++);
        ctx.getRoot()->arrays[arrayId] = std::vector<Value>(sz, {"int", "0"});
        ctx.defineVar(name, "array", {"array", arrayId});
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
        int idx = std::stoi(index->eval(ctx).value);
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
        int idx = std::stoi(index->eval(ctx).value);
        auto& arr = ctx.getRoot()->arrays[arrVal.value];
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
            throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(idx));
        }
        return arr[idx];
    }
};

struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> stmts;
    Value eval(Context& ctx) override {
        for (auto& stmt : stmts) {
            if (stmt) stmt->eval(ctx);
        }
        return {"void", ""};
    }
};

struct IfNode : Node {
    std::unique_ptr<Node> condition, thenB, elseB;
    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e = nullptr)
        : condition(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
    Value eval(Context& ctx) override {
        bool cond = (condition->eval(ctx).value == "true");
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
        while (condition->eval(ctx).value == "true") {
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
        if (init) init->eval(ctx);
        while (!condition || condition->eval(ctx).value == "true") {
            try {
                if (body) body->eval(ctx);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // continue: evaluate step and continue
            }
            if (step) step->eval(ctx);
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
        int milliseconds = std::stoi(timeExpr->eval(ctx).value);
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
