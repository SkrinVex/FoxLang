#include "foxlang/AST.h"
#include "foxlang/Debug.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace foxlang {

namespace {

// Keeps the debugger's view of scopes and frames matched when a block or a call
// ends, by an error as much as by reaching its end.
struct ScopeMark {
    DebugHook& hook;
    ~ScopeMark() { hook.leaveScope(); }
};

struct FrameMark {
    DebugHook* hook;
    ~FrameMark() {
        if (hook) hook->leaveFunction();
    }
};

// A block run under a debugger: every statement is a place to stop, and an error is
// reported while the scopes it happened in still exist.
void evalDebugged(BlockNode& block, Context& scope, DebugHook& hook) {
    runtime::StackGuard& guard = runtime::stackGuard();
    hook.enterScope(scope);
    ScopeMark mark{hook};
    for (auto& stmt : block.stmts) {
        if (!stmt) continue;
        if (stmt->range.start.line > 0) {
            guard.line = stmt->range.start.line;
            guard.file = block.file;
            // A function definition only registers the function; stepping over it is noise.
            if (!dynamic_cast<const FuncDefNode*>(stmt.get())) hook.statement(block.file, stmt->range.start.line);
        }
        try {
            stmt->eval(scope);
        } catch (const std::exception& error) {
            hook.error(error.what(), guard.tryDepth > 0);
            throw;
        }
    }
}

} // namespace

Value FuncDefNode::eval(Context& ctx) {
    ctx.getRoot()->defineFunc(name, std::make_shared<FuncDefNode>(returnType, name, params, body, nameRange));
    return {"void", ""};
}

Value FuncDefNode::invoke(std::vector<Value> args, Context& caller) const {
    if (args.size() != params.size())
        throw std::runtime_error("Runtime Error: function '" + name + "' expects " + std::to_string(params.size()) +
                                 " arguments, got " + std::to_string(args.size()));

    // Lexical scope: a function sees globals, never the caller's locals. The scope is
    // gone before the result is checked, so a returned local is no longer shared.
    auto scope = std::make_unique<Context>();
    scope->parent = caller.getRoot();
    scope->interpreter = caller.interpreter;
    // Arrays, maps and structs are passed by reference: the function may change them.
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        runtime::coerce(param.type, args[i], "parameter '" + param.name + "' of '" + name + "'");
        scope->defineVar(param.name, param.type, args[i]);
    }

    CallDepth guard(name);
    DebugHook* hook = runtime::debugHook();
    FrameMark frame{hook};
    if (hook) {
        auto* block = dynamic_cast<const BlockNode*>(body.get());
        hook->enterFunction(name, block ? block->file : nullptr, range.start.line, *scope);
    }
    // After a normal return the caller's statement is the one running again, so an
    // error later in it must not be reported at the callee's last line.
    runtime::StackGuard& location = runtime::stackGuard();
    int callerLine = location.line;
    const std::string* callerFile = location.file;
    Value result{"void", ""};
    try {
        body->eval(*scope);
    } catch (ReturnValue& ret) {
        result = std::move(ret.value);
    } catch (const BreakException&) {
        throw std::runtime_error("Runtime Error: 'break' outside of loop in function '" + name + "'");
    } catch (const ContinueException&) {
        throw std::runtime_error("Runtime Error: 'continue' outside of loop in function '" + name + "'");
    }

    location.line = callerLine;
    location.file = callerFile;
    scope.reset();
    if (returnType == "void") return {"void", ""};
    if (result.type == "void")
        throw std::runtime_error("Runtime Error: function '" + name + "' must return " + returnType + " but ended without a value");
    runtime::coerce(returnType, result, "return value of '" + name + "'");
    // A container that is still someone else's (a global, a parameter) leaves as a copy.
    runtime::own(result);
    return result;
}

Value ReturnNode::eval(Context& ctx) {
    Value result = expr ? expr->eval(ctx) : Value{"void", ""};
    throw ReturnValue{std::move(result)};
}

Value FuncCallNode::eval(Context& ctx) {
    std::vector<Value> argValues;
    argValues.reserve(args.size());
    for (auto& arg : args) argValues.push_back(arg->eval(ctx));

    if (!resolved) {
        builtin = runtime::findBuiltin(name);
        resolved = true;
    }
    auto function = ctx.getFunc(name);
    // A builtin and a FoxLang function may share a name, like get(items, i) and the
    // server's get(path, handler). The builtin wins whenever the arguments fit it.
    if (builtin && (!function || runtime::acceptsArguments(*builtin, argValues)))
        return runtime::invoke(*builtin, argValues, ctx);
    if (!function) {
        // A struct's name called like a function builds a value of it.
        if (auto type = ctx.getStruct(name)) return runtime::construct(*type, std::move(argValues), ctx);
        throw std::runtime_error("Runtime Error: Function '" + name + "' not found!");
    }
    return static_cast<const FuncDefNode*>(function.get())->invoke(std::move(argValues), ctx);
}

NumberNode::NumberNode(std::string v) : val(std::move(v)) {
    isFloat = val.find('.') != std::string::npos;
    if (isFloat) {
        literal = {"float", Text::real(std::strtod(val.c_str(), nullptr))};
    } else {
        errno = 0;
        long long parsed = std::strtoll(val.c_str(), nullptr, 10);
        // Keep the digits when they do not even fit 64 bits; storing them reports the range.
        literal = errno == ERANGE ? Value{"int", val} : Value{"int", Text::integer(parsed)};
    }
}

Value VarDeclNode::eval(Context& ctx) {
    Context& scope = global ? *ctx.getRoot() : ctx;
    if (!global && scope.variables.count(name))
        throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    Value val = expr ? expr->eval(ctx) : runtime::zeroValue(type, ctx);
    runtime::coerce(type, val, std::string(global ? "global variable '" : "variable '") + name + "'");
    runtime::own(val);
    scope.defineVar(name, type, val);
    return {"void", ""};
}

namespace {
std::string containerName(const Value& value) {
    if (value.type == "array") return "an array";
    if (value.type == "map") return "a map";
    return "a struct '" + value.type + "'";
}
} // namespace

Value BinOpNode::eval(Context& ctx) {
    // Short-circuit before the right side runs: `x != 0 && 10 / x > 1` must not divide by zero.
    if (op == "&&" || op == "||") {
        bool l = truth(left->eval(ctx), "left");
        if (l == (op == "||")) return {"bool", l ? "true" : "false"};
        return {"bool", truth(right->eval(ctx), "right") ? "true" : "false"};
    }

    Value lval = left->eval(ctx);
    Value rval = right->eval(ctx);

    bool equality = op == "==" || op == "!=";
    bool concatenation = (op == "+" || op == "+=") && (lval.type == "string" || rval.type == "string");
    if ((lval.ref || rval.ref) && !equality && !concatenation)
        throw std::runtime_error("Type Error: operator '" + op + "' cannot be applied to " + containerName(lval.ref ? lval : rval));

    if (op == "+" || op == "+=") {
        if (concatenation) return {"string", runtime::display(lval) + runtime::display(rval)};
        if (lval.type == "float" || rval.type == "float")
            return {"float", runtime::realResult(number(lval, "left") + number(rval, "right"))};
        return {"int", runtime::intResult(integer(lval, "left") + integer(rval, "right"), op)};
    }

    if (op == "-" || op == "-=" || op == "*" || op == "*=" || op == "/" || op == "/=" || op == "%" || op == "%=") {
        bool division = op == "/" || op == "/=" || op == "%" || op == "%=";
        bool subtract = op == "-" || op == "-=";
        bool multiply = op == "*" || op == "*=";
        if (lval.type == "float" || rval.type == "float") {
            double l = number(lval, "left"), r = number(rval, "right");
            if (r == 0.0 && division) throw std::runtime_error("Runtime Error: Division by zero");
            double result = subtract ? l - r : multiply ? l * r : (op == "/" || op == "/=") ? l / r : std::fmod(l, r);
            return {"float", runtime::realResult(result)};
        }
        long long l = integer(lval, "left"), r = integer(rval, "right");
        if (r == 0 && division) throw std::runtime_error("Runtime Error: Division by zero");
        long long result = subtract ? l - r : multiply ? l * r : (op == "/" || op == "/=") ? l / r : l % r;
        return {"int", runtime::intResult(result, op)};
    }

    if (equality || op == "<" || op == ">" || op == "<=" || op == ">=") {
        auto decide = [&](auto l, auto r) {
            return op == "==" ? l == r : op == "!=" ? l != r : op == "<" ? l < r :
                   op == "<=" ? l <= r : op == ">=" ? l >= r : l > r;
        };
        bool result;
        if (lval.type == "string" && rval.type == "string") {
            result = decide(lval.value.str(), rval.value.str());
        } else if (lval.type == "bool" && rval.type == "bool") {
            result = decide(truth(lval, "left"), truth(rval, "right"));
        } else if (lval.ref || rval.ref) {
            // Arrays, maps and structs are equal when their contents are.
            if (!equality) throw std::runtime_error("Type Error: operator '" + op + "' cannot be applied to " + containerName(lval.ref ? lval : rval));
            result = runtime::deepEqual(lval, rval) == (op == "==");
        } else {
            result = decide(number(lval, "left"), number(rval, "right"));
        }
        return {"bool", result ? "true" : "false"};
    }

    throw std::runtime_error("Runtime Error: unknown operator '" + op + "'");
}

double BinOpNode::number(const Value& value, const char* side) const {
    double result = 0;
    if (runtime::tryNumber(value, result)) return result;
    return runtime::toNumber(value, describe(side));
}

long long BinOpNode::integer(const Value& value, const char* side) const {
    long long result = 0;
    if (runtime::tryInt(value, result)) return result;
    return runtime::toInt(value, describe(side));
}

bool BinOpNode::truth(const Value& value, const char* side) const {
    if (value.type != "bool") throw std::runtime_error("Type Error: " + describe(side) + " must be bool, got '" + value.type + "'");
    return value.value == "true";
}

Value UnaryOpNode::eval(Context& ctx) {
    Value val = operand->eval(ctx);
    if (op == "!") {
        if (val.type != "bool") throw std::runtime_error("Type Error: operand of '!' must be bool, got '" + val.type + "'");
        return {"bool", val.value == "true" ? "false" : "true"};
    }
    if (val.type == "int") return {"int", runtime::intResult(-intArg(val, "operand of unary '-'"), "-")};
    if (val.type == "float") return {"float", runtime::realResult(-runtime::toNumber(val, "operand of unary '-'"))};
    throw std::runtime_error("Type Error: operand of unary '-' must be a number, got '" + val.type + "'");
}

Value PostIncNode::eval(Context& ctx) {
    Value current = ctx.getVar(name);
    const char* op = delta > 0 ? "++" : "--";
    if (current.type == "float") {
        double value = runtime::toNumber(current, "variable '" + name + "'");
        ctx.setVar(name, {"float", runtime::realResult(value + delta)});
        return current;
    }
    if (current.type != "int")
        throw std::runtime_error("Type Error: '" + std::string(op) + "' needs an int or float variable, '" + name + "' is " + current.type);
    long long value = intArg(current, "variable", name);
    ctx.setVar(name, {"int", runtime::intResult(value + delta, op)});
    return {"int", Text::integer(value)};
}

Value ArrayDeclNode::eval(Context& ctx) {
    Context& scope = global ? *ctx.getRoot() : ctx;
    if (!global && scope.variables.count(name))
        throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    Value value;
    if (initializer) {
        value = initializer->eval(ctx);
        runtime::coerce("array", value, "initializer of array '" + name + "'");
        runtime::own(value);
    } else {
        long long size = sizeNode ? intArg(sizeNode->eval(ctx), "size of array", name) : 0;
        if (size < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
        value = runtime::makeArray(std::vector<Value>(static_cast<size_t>(size), {"int", Text::integer(0)}));
    }
    scope.defineVar(name, "array", value);
    return {"void", ""};
}

Value ArrayLiteralNode::eval(Context& ctx) {
    std::vector<Value> items;
    items.reserve(elements.size());
    for (auto& element : elements) {
        items.push_back(element->eval(ctx));
        runtime::own(items.back());
    }
    return runtime::makeArray(std::move(items));
}

namespace {

std::string keyOf(const Value& key) {
    if (key.type == "string" || key.type == "int") return key.value.str();
    throw std::runtime_error("Type Error: a map key must be string or int, got '" + key.type + "'");
}

size_t positionIn(const Object& array, const Value& indexValue) {
    long long index = intArg(indexValue, "array index");
    if (index < 0 || static_cast<unsigned long long>(index) >= array.items.size())
        throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(index) + " (size " +
                                 std::to_string(array.items.size()) + ")");
    return static_cast<size_t>(index);
}

size_t fieldIn(const Object& object, const std::string& name) {
    const auto& fields = object.structType->fields;
    for (size_t i = 0; i < fields.size(); ++i)
        if (fields[i].name == name) return i;
    throw std::runtime_error("Runtime Error: struct '" + object.structType->name + "' has no field '" + name + "'");
}

Value& missingKey(const std::string& key) {
    throw std::runtime_error("Runtime Error: map has no key '" + key + "' (has(map, key) checks, get_or(map, key, default) reads safely)");
}

// base[index] on a container value; create inserts a new map key.
Value& element(Value& base, const Value& index, bool create) {
    if (!base.ref || base.ref->kind == Object::Kind::Struct)
        throw std::runtime_error("Type Error: '[]' needs an array or a map, got '" + base.type + "'");
    Object& object = *base.ref;
    if (object.kind == Object::Kind::Array) return object.items[positionIn(object, index)];
    std::string key = keyOf(index);
    if (create) return object.slot(key);
    long at = object.find(key);
    return at < 0 ? missingKey(key) : object.items[static_cast<size_t>(at)];
}

Value& member(Value& base, const std::string& name, bool create, const std::string** declared = nullptr) {
    if (base.ref && base.ref->kind == Object::Kind::Struct) {
        size_t at = fieldIn(*base.ref, name);
        if (declared) *declared = &base.ref->structType->fields[at].type;
        return base.ref->items[at];
    }
    if (base.ref && base.ref->kind == Object::Kind::Map) {
        if (create) return base.ref->slot(name);
        long at = base.ref->find(name);
        return at < 0 ? missingKey(name) : base.ref->items[static_cast<size_t>(at)];
    }
    throw std::runtime_error("Type Error: '." + name + "' needs a struct or a map, got '" + base.type + "'");
}

// The slot an assignment writes to: a variable, then elements and fields inside it.
// A struct field reports its declared type, which the stored value must keep.
Value& place(Node& target, Context& ctx, bool create, const std::string** declared = nullptr) {
    if (auto* variable = dynamic_cast<VarAccessNode*>(&target)) {
        Value* found = ctx.findVar(variable->name);
        if (!found) throw std::runtime_error("Runtime Error: Variable '" + variable->name + "' not found!");
        return *found;
    }
    if (auto* index = dynamic_cast<IndexNode*>(&target)) {
        Value key = index->index->eval(ctx);
        return element(place(*index->base, ctx, false), key, create);
    }
    if (auto* field = dynamic_cast<FieldNode*>(&target))
        return member(place(*field->base, ctx, false), field->name, create, declared);
    throw std::runtime_error("Syntax Error: only a variable, an element or a field can be assigned to");
}

} // namespace

Value IndexNode::eval(Context& ctx) {
    Value container = base->eval(ctx);
    return element(container, index->eval(ctx), false);
}

Value FieldNode::eval(Context& ctx) {
    Value container = base->eval(ctx);
    return member(container, name, false);
}

Value SetNode::eval(Context& ctx) {
    // The value is computed first: it may change the containers the target lives in.
    Value assigned = value->eval(ctx);
    const std::string* declared = nullptr;
    Value& slot = place(*target, ctx, op == "=", &declared);
    if (op != "=") {
        struct Literal : Node {
            Value held;
            explicit Literal(Value v) : held(std::move(v)) {}
            Value eval(Context&) override { return held; }
        };
        BinOpNode combine(op, std::make_unique<Literal>(slot), std::make_unique<Literal>(assigned));
        assigned = combine.eval(ctx);
    }
    // A struct field keeps its declared type; elements and map values take any value.
    if (declared) runtime::coerce(*declared, assigned, "field '" + static_cast<FieldNode*>(target.get())->name + "'");
    runtime::own(assigned);
    slot = std::move(assigned);
    return {"void", ""};
}

Value MapLiteralNode::eval(Context& ctx) {
    Value map = runtime::makeMap();
    for (auto& entry : entries) {
        std::string key = keyOf(entry.first->eval(ctx));
        Value value = entry.second->eval(ctx);
        runtime::own(value);
        map.ref->slot(key) = std::move(value);
    }
    return map;
}

Value StructDefNode::eval(Context& ctx) {
    ctx.getRoot()->structs[type->name] = type;
    return {"void", ""};
}

Value TryNode::eval(Context& ctx) {
    runtime::StackGuard& guard = runtime::stackGuard();
    // finally runs however the protected code ends: normally, by an error, by
    // return, break, continue or exit.
    auto cleanupAfter = [&](auto&& run) {
        try {
            run();
        } catch (...) {
            if (cleanup) cleanup->eval(ctx);
            throw;
        }
        if (cleanup) cleanup->eval(ctx);
    };
    cleanupAfter([&] {
        std::string message;
        bool failed = false;
        {
            struct Depth {
                int& count;
                explicit Depth(int& c) : count(c) { ++count; }
                ~Depth() { --count; }
            } depth(guard.tryDepth);
            try {
                body->eval(ctx);
            } catch (const std::exception& error) {
                if (!handler) throw;
                message = error.what();
                failed = true;
            }
        }
        if (!failed) return;
        Context scope;
        scope.parent = &ctx;
        scope.interpreter = ctx.interpreter;
        if (!errorName.empty()) scope.defineVar(errorName, "string", {"string", message});
        handler->eval(scope);
    });
    return {"void", ""};
}

Value ThrowNode::eval(Context& ctx) {
    Value value = message->eval(ctx);
    throw std::runtime_error(runtime::display(value));
}

Value BlockNode::eval(Context& ctx) {
    Context inner;
    if (scoped) {
        inner.parent = &ctx;
        inner.interpreter = ctx.interpreter;
    }
    Context& scope = scoped ? inner : ctx;
    if (DebugHook* hook = runtime::debugHook()) {
        evalDebugged(*this, scope, *hook);
        return {"void", ""};
    }
    runtime::StackGuard& guard = runtime::stackGuard();
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        // Remembering the line costs one store. Catching here to rethrow with the
        // line attached cost an exception per nested block, and an error leaving a
        // deep recursion had to pass through every one of them.
        if (stmt->range.start.line > 0) {
            guard.line = stmt->range.start.line;
            guard.file = file;
        }
        stmt->eval(scope);
    }
    return {"void", ""};
}

Value SwitchNode::eval(Context& ctx) {
    Value switchValue = expr->eval(ctx);
    bool matched = false;
    try {
        for (auto& caseItem : cases) {
            if (!matched) {
                Value caseValue = caseItem.first->eval(ctx);
                double l = 0, r = 0;
                bool numeric = runtime::tryNumber(switchValue, l) && runtime::tryNumber(caseValue, r) &&
                               switchValue.type != "string" && caseValue.type != "string";
                matched = numeric ? l == r : runtime::deepEqual(switchValue, caseValue);
            }
            // Without break, execution falls through into the following cases.
            if (matched) caseItem.second->eval(ctx);
        }
        if (defaultCase) defaultCase->eval(ctx);
    } catch (const BreakException&) {
    }
    return {"void", ""};
}

} // namespace foxlang
