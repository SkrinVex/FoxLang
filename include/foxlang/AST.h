#pragma once
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <stdexcept>
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

// Runaway recursion used to kill the process with a stack overflow instead of an error.
// What runs out is the native stack, not a number of calls, and one call costs a
// different amount of it per platform and per compiler: counting frames was wrong on
// Windows, where a thread gets 1 MB rather than the 8 MB Linux gives. Measure the stack.
struct CallDepth {
    // A stack cannot be measured the same way everywhere, so nothing here is trusted
    // alone. The call count starts at a pessimistic four kilobytes per frame and is
    // recalculated from what the stack actually did once there are frames to average;
    // the travelled distance is checked as well. Whichever brake trips first wins.
    static constexpr size_t assumedFrameCost = 4096;
    static constexpr int calibrateAt = 64;
    explicit CallDepth(const std::string& name) {
        char probe = 0;
        runtime::StackGuard& guard = runtime::stackGuard();
        if (guard.depth == 0) {
            guard.origin = &probe;
            guard.budget = platform::stackBudget();
            guard.limit = static_cast<int>(guard.budget / assumedFrameCost);
        } else {
            std::ptrdiff_t used = guard.origin - &probe; // A stack growing upwards never trips this.
            if (guard.depth == calibrateAt && used > 0) {
                size_t measured = static_cast<size_t>(used) / calibrateAt;
                if (measured > 64) guard.limit = static_cast<int>(guard.budget / measured);
            }
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

    Value eval(Context& ctx) override;
    // Runs the body in a fresh scope below the globals and converts the result to
    // the declared return type. Used by calls and by the HTTP server for handlers.
    Value invoke(std::vector<Value> args, Context& caller) const;
};

struct ReturnNode : Node {
    std::unique_ptr<Node> expr;
    explicit ReturnNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    Value eval(Context& ctx) override;
};

struct FuncCallNode : Node {
    std::string name;
    std::vector<std::unique_ptr<Node>> args;
    SourceRange nameRange;

    FuncCallNode(std::string n, std::vector<std::unique_ptr<Node>> a, SourceRange nr = {})
        : name(std::move(n)), args(std::move(a)), nameRange(nr) {}

    Value eval(Context& ctx) override;

private:
    const runtime::Builtin* builtin = nullptr;
    bool resolved = false;
};

struct NumberNode : Node {
    std::string val;
    bool isFloat;
    explicit NumberNode(std::string v);
    Value eval(Context& /*ctx*/) override { return literal; }
private:
    Value literal;
};

struct StringNode : Node {
    std::string val;
    explicit StringNode(std::string v) : val(std::move(v)) {}
    Value eval(Context& /*ctx*/) override { return {"string", val}; }
};

struct BoolNode : Node {
    bool val;
    explicit BoolNode(bool v) : val(v) {}
    Value eval(Context& /*ctx*/) override { return {"bool", val ? "true" : "false"}; }
};

struct VarAccessNode : Node {
    std::string name;
    SourceRange nameRange;
    explicit VarAccessNode(std::string n, SourceRange nr = {}) : name(std::move(n)), nameRange(nr) {}
    Value eval(Context& ctx) override { return ctx.getVar(name); }
};

struct VarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    bool global = false;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e, SourceRange nr = {}, bool isGlobal = false)
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)), nameRange(nr), global(isGlobal) {}
    Value eval(Context& ctx) override;
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
    Value eval(Context& ctx) override;

private:
    std::string describe(const char* side) const { return std::string(side) + " operand of '" + op + "'"; }
    double number(const Value& value, const char* side) const;
    long long integer(const Value& value, const char* side) const;
    bool truth(const Value& value, const char* side) const;
};

// Unary minus and logical not.
struct UnaryOpNode : Node {
    std::string op;
    std::unique_ptr<Node> operand;
    UnaryOpNode(std::string o, std::unique_ptr<Node> n) : op(std::move(o)), operand(std::move(n)) {}
    Value eval(Context& ctx) override;
};

// Postfix i++ and i--: returns the old value, stores the new one.
struct PostIncNode : Node {
    std::string name;
    int delta;
    explicit PostIncNode(std::string n, int d = 1) : name(std::move(n)), delta(d) {}
    Value eval(Context& ctx) override;
};

// array name size;   array name = expression;   array name;
struct ArrayDeclNode : Node {
    std::string name;
    std::unique_ptr<Node> sizeNode;
    std::unique_ptr<Node> initializer;
    SourceRange nameRange;
    bool global = false;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s, std::unique_ptr<Node> init = nullptr, SourceRange nr = {})
        : name(std::move(n)), sizeNode(std::move(s)), initializer(std::move(init)), nameRange(nr) {}
    Value eval(Context& ctx) override;
};

// [a, b, c] creates a temporary array owned by the scope that evaluates it.
struct ArrayLiteralNode : Node {
    std::vector<std::unique_ptr<Node>> elements;
    Value eval(Context& ctx) override;
};

// base[index]: an array element by position or a map value by key.
struct IndexNode : Node {
    std::unique_ptr<Node> base, index;
    IndexNode(std::unique_ptr<Node> b, std::unique_ptr<Node> i) : base(std::move(b)), index(std::move(i)) {}
    Value eval(Context& ctx) override;
};

// base.name: a struct field, or a map value whose key is a plain word.
struct FieldNode : Node {
    std::unique_ptr<Node> base;
    std::string name;
    SourceRange nameRange;
    FieldNode(std::unique_ptr<Node> b, std::string n, SourceRange nr = {})
        : base(std::move(b)), name(std::move(n)), nameRange(nr) {}
    Value eval(Context& ctx) override;
};

// target = value (also +=, -=, *=, /=, %=), where the target is a chain of indexes and
// fields that starts at a variable: items[i] = 1;  user.name = "Ann";  m["a"][0] += 2;
struct SetNode : Node {
    std::unique_ptr<Node> target, value;
    std::string op;
    SetNode(std::unique_ptr<Node> t, std::unique_ptr<Node> v, std::string o = "=")
        : target(std::move(t)), value(std::move(v)), op(std::move(o)) {}
    Value eval(Context& ctx) override;
};

// {"key": value, ...} creates a map.
struct MapLiteralNode : Node {
    std::vector<std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>>> entries;
    Value eval(Context& ctx) override;
};

// struct Name { type field; type field = default; }
struct StructDefNode : Node {
    std::shared_ptr<StructType> type;
    SourceRange nameRange;
    std::vector<SourceRange> fieldRanges;
    Value eval(Context& ctx) override;
};

// try { ... } catch (string error) { ... } finally { ... }
struct TryNode : Node {
    std::unique_ptr<Node> body, handler, cleanup;
    std::string errorName;
    SourceRange errorRange;
    Value eval(Context& ctx) override;
};

// throw expression;  raises an error with the value as its message.
struct ThrowNode : Node {
    std::unique_ptr<Node> message;
    explicit ThrowNode(std::unique_ptr<Node> m) : message(std::move(m)) {}
    Value eval(Context& ctx) override;
};

struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> stmts;
    const std::string* file = nullptr; // Source file of the statements, for error locations.
    // The program and an imported module are the global scope itself, not a block inside it.
    bool scoped = true;
    Value eval(Context& ctx) override;
};

struct IfNode : Node {
    std::unique_ptr<Node> condition, thenB, elseB;
    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e = nullptr)
        : condition(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
    Value eval(Context& ctx) override {
        if (conditionTruth(condition->eval(ctx), "if")) {
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
                // continue still runs the step
            }
            if (step) step->eval(loop);
        }
        return {"void", ""};
    }
};

struct BreakNode : Node {
    Value eval(Context& /*ctx*/) override { throw BreakException{}; }
};

struct ContinueNode : Node {
    Value eval(Context& /*ctx*/) override { throw ContinueException{}; }
};

struct SwitchNode : Node {
    std::unique_ptr<Node> expr;
    std::vector<std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>>> cases;
    std::unique_ptr<Node> defaultCase;

    explicit SwitchNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    Value eval(Context& ctx) override;
};

// Forward declaration of interpreter execution hooks
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
