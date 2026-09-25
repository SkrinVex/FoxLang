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

// Where a variable lives, decided once by the resolver (Resolver.cpp) before the code
// first runs: a numbered slot of the function's frame, a global, or unknown (code
// the resolver has not seen, such as the debugger's console), found by name.
struct VarRef {
    static constexpr int byName = -2;
    static constexpr int global = -1;
    int slot = byName;
    // A global is found by name once and then read through this pointer; the root's
    // generation changes when its variables are cleared.
    Value* cached = nullptr;
    const Context* cachedRoot = nullptr;
    unsigned cachedGeneration = 0;

    // The variable itself, or null when it does not exist.
    Value* find(Context& ctx, const std::string& name);
};

// The numbered slots of a function body or of the program's blocks.
struct FrameLayout {
    std::vector<std::string> names;
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
    // The FoxLang function of this name, found once per version of the function table.
    // Weak: a recursive function's own call must not keep the function alive forever.
    std::weak_ptr<Node> function;
    const Context* functionRoot = nullptr;
    unsigned functionGeneration = 0;
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
    VarRef ref;
    explicit VarAccessNode(std::string n, SourceRange nr = {}) : name(std::move(n)), nameRange(nr) {}
    Value eval(Context& ctx) override;
};

struct VarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    bool global = false;
    int slot = VarRef::byName;
    bool duplicate = false; // declared twice in one scope: an error when it runs
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e, SourceRange nr = {}, bool isGlobal = false)
        : type(std::move(t)), name(std::move(n)), expr(std::move(e)), nameRange(nr), global(isGlobal) {}
    Value eval(Context& ctx) override;
};

struct VarAssignNode : Node {
    std::string name;
    std::unique_ptr<Node> expr;
    SourceRange nameRange;
    VarRef ref;
    VarAssignNode(std::string n, std::unique_ptr<Node> e, SourceRange nr = {})
        : name(std::move(n)), expr(std::move(e)), nameRange(nr) {}
    Value eval(Context& ctx) override;
};

struct BinOpNode : Node {
    std::string op;
    std::unique_ptr<Node> left, right;
    BinOpNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r);
    Value eval(Context& ctx) override;

private:
    // The operator decided once when the node is built, not by comparing text on every run.
    enum class Kind { And, Or, Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge, Unknown } kind;
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
    VarRef ref;
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
    int slot = VarRef::byName;
    bool duplicate = false;
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
    int errorSlot = VarRef::byName;
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
    // Slots declared inside this block, emptied when it ends; the layout of a function
    // body or of the program, set by the resolver.
    int firstSlot = 0, endSlot = 0;
    std::shared_ptr<FrameLayout> layout;
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
        runtime::StackGuard& guard = runtime::stackGuard();
        while (conditionTruth(condition->eval(ctx), "while")) {
            if (body) body->eval(ctx);
            if (guard.flow == runtime::StackGuard::Flow::None) continue;
            if (guard.flow == runtime::StackGuard::Flow::Return) break;
            bool stop = guard.flow == runtime::StackGuard::Flow::Break;
            guard.flow = runtime::StackGuard::Flow::None;
            if (stop) break;
        }
        return {"void", ""};
    }
};

struct ForNode : Node {
    std::unique_ptr<Node> init, condition, step, body;
    int firstSlot = 0, endSlot = 0; // the loop's own variables
    ForNode(std::unique_ptr<Node> i, std::unique_ptr<Node> c, std::unique_ptr<Node> s, std::unique_ptr<Node> b)
        : init(std::move(i)), condition(std::move(c)), step(std::move(s)), body(std::move(b)) {}
    Value eval(Context& ctx) override {
        Context loop(ctx);
        if (init) init->eval(loop);
        runtime::StackGuard& guard = runtime::stackGuard();
        while (!condition || conditionTruth(condition->eval(loop), "for")) {
            if (body) body->eval(loop);
            if (guard.flow != runtime::StackGuard::Flow::None) {
                if (guard.flow == runtime::StackGuard::Flow::Return) break;
                bool stop = guard.flow == runtime::StackGuard::Flow::Break;
                guard.flow = runtime::StackGuard::Flow::None;
                if (stop) break;
                // continue still runs the step
            }
            if (step) step->eval(loop);
        }
        for (int slot = firstSlot; slot < endSlot && loop.slots; ++slot) loop.slots[slot] = Value();
        return {"void", ""};
    }
};

struct BreakNode : Node {
    Value eval(Context& /*ctx*/) override {
        runtime::stackGuard().flow = runtime::StackGuard::Flow::Break;
        return {"void", ""};
    }
};

struct ContinueNode : Node {
    Value eval(Context& /*ctx*/) override {
        runtime::stackGuard().flow = runtime::StackGuard::Flow::Continue;
        return {"void", ""};
    }
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
