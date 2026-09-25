#include "foxlang/AST.h"
#include "foxlang/Debug.h"
#include "foxlang/Resolver.h"
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

// The slots declared in a block are emptied when it ends, however it ends: their
// values (and the containers they hold) do not outlive the block.
struct SlotReset {
    Value* slots;
    int first, end;
    ~SlotReset() {
        if (slots)
            for (int slot = first; slot < end; ++slot) slots[slot] = Value();
    }
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
        if (guard.flow != runtime::StackGuard::Flow::None) return;
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

    // Lexical scope: a function sees globals, never the caller's locals. The scope ends
    // before the result is checked, so the function's locals are gone by then.
    auto scope = std::make_unique<Context>();
    scope->parent = caller.getRoot();
    scope->interpreter = caller.interpreter;
    // The body's variables are numbered once; each call gets its own row of slots,
    // the parameters first. Arrays, maps and structs are shared with the caller.
    auto* block = dynamic_cast<BlockNode*>(body.get());
    if (block && !block->layout) resolveFunction(const_cast<FuncDefNode&>(*this));
    std::vector<Value> frameSlots(block && block->layout ? block->layout->names.size() : 0);
    if (block && block->layout) {
        scope->slots = frameSlots.data();
        scope->slotNames = &block->layout->names;
        scope->frame = true;
    }
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        runtime::coerce(param.type, args[i], "parameter '" + param.name + "' of '" + name + "'");
        if (scope->frame) frameSlots[i] = std::move(args[i]);
        else scope->defineVar(param.name, param.type, args[i]);
    }

    CallDepth guard(name);
    DebugHook* hook = runtime::debugHook();
    FrameMark frame{hook};
    if (hook) hook->enterFunction(name, block ? block->file : nullptr, range.start.line, *scope);
    // After a normal return the caller's statement is the one running again, so an
    // error later in it must not be reported at the callee's last line.
    runtime::StackGuard& location = runtime::stackGuard();
    int callerLine = location.line;
    const std::string* callerFile = location.file;
    Value result{"void", ""};
    body->eval(*scope);
    switch (location.flow) {
        case runtime::StackGuard::Flow::Return:
            result = std::move(location.returned);
            location.returned = Value();
            break;
        case runtime::StackGuard::Flow::Break:
            location.flow = runtime::StackGuard::Flow::None;
            throw std::runtime_error("Runtime Error: 'break' outside of loop in function '" + name + "'");
        case runtime::StackGuard::Flow::Continue:
            location.flow = runtime::StackGuard::Flow::None;
            throw std::runtime_error("Runtime Error: 'continue' outside of loop in function '" + name + "'");
        default:
            break;
    }
    location.flow = runtime::StackGuard::Flow::None;

    location.line = callerLine;
    location.file = callerFile;
    scope.reset();
    if (returnType == "void") return {"void", ""};
    if (result.type == "void")
        throw std::runtime_error("Runtime Error: function '" + name + "' must return " + returnType + " but ended without a value");
    runtime::coerce(returnType, result, "return value of '" + name + "'");
    return result;
}

Value ReturnNode::eval(Context& ctx) {
    Value result = expr ? expr->eval(ctx) : Value{"void", ""};
    runtime::StackGuard& guard = runtime::stackGuard();
    guard.returned = std::move(result);
    guard.flow = runtime::StackGuard::Flow::Return;
    return {"void", ""};
}

Value FuncCallNode::eval(Context& ctx) {
    std::vector<Value> argValues;
    argValues.reserve(args.size());
    for (auto& arg : args) argValues.push_back(arg->eval(ctx));

    if (!resolved) {
        builtin = runtime::findBuiltin(name);
        resolved = true;
    }
    const Context* root = ctx.getRoot();
    std::shared_ptr<Node> callee;
    if (functionRoot == root && functionGeneration == root->functionGeneration) {
        callee = function.lock();
    } else {
        callee = ctx.getFunc(name);
        function = callee;
        functionRoot = root;
        functionGeneration = root->functionGeneration;
    }
    // A builtin and a FoxLang function may share a name, like get(items, i) and the
    // server's get(path, handler). The builtin wins whenever the arguments fit it.
    if (builtin && (!callee || runtime::acceptsArguments(*builtin, argValues)))
        return runtime::invoke(*builtin, argValues, ctx);
    if (!callee) {
        // A struct's name called like a function builds a value of it.
        if (auto type = ctx.getStruct(name)) return runtime::construct(*type, std::move(argValues), ctx);
        throw std::runtime_error("Runtime Error: Function '" + name + "' not found!");
    }
    // `callee` keeps the function alive even if it redefines itself meanwhile.
    return static_cast<const FuncDefNode*>(callee.get())->invoke(std::move(argValues), ctx);
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

Value* VarRef::find(Context& ctx, const std::string& name) {
    if (slot >= 0 && ctx.slots) {
        Value& value = ctx.slots[slot];
        return value.type.is(TypeName::Kind::Void) ? nullptr : &value;
    }
    if (slot == global) {
        Context* root = ctx.getRoot();
        if (cached && cachedRoot == root && cachedGeneration == root->generation) return cached;
        auto found = root->variables.find(name);
        if (found == root->variables.end()) return nullptr;
        cached = &found->second;
        cachedRoot = root;
        cachedGeneration = root->generation;
        return cached;
    }
    return ctx.findVar(name);
}

namespace {
[[noreturn]] void notFound(const std::string& name) {
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}
} // namespace

Value VarAccessNode::eval(Context& ctx) {
    Value* value = ref.find(ctx, name);
    if (!value) notFound(name);
    return *value;
}

Value VarAssignNode::eval(Context& ctx) {
    Value assigned = expr->eval(ctx);
    Value* target = ref.find(ctx, name);
    if (!target) notFound(name);
    runtime::assign(*target, std::move(assigned), name);
    return {"void", ""};
}

Value VarDeclNode::eval(Context& ctx) {
    if (duplicate) throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    if (slot >= 0 && ctx.slots) {
        Value val = expr ? expr->eval(ctx) : runtime::zeroValue(type, ctx);
        runtime::coerce(type, val, "variable '" + name + "'");
        ctx.slots[slot] = std::move(val);
        return {"void", ""};
    }
    Context& scope = global ? *ctx.getRoot() : ctx;
    if (!global && scope.variables.count(name))
        throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    Value val = expr ? expr->eval(ctx) : runtime::zeroValue(type, ctx);
    runtime::coerce(type, val, std::string(global ? "global variable '" : "variable '") + name + "'");
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

BinOpNode::BinOpNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r)
    : op(std::move(o)), left(std::move(l)), right(std::move(r)) {
    static const std::pair<const char*, Kind> kinds[] = {
        {"&&", Kind::And}, {"||", Kind::Or}, {"+", Kind::Add}, {"+=", Kind::Add}, {"-", Kind::Sub}, {"-=", Kind::Sub},
        {"*", Kind::Mul}, {"*=", Kind::Mul}, {"/", Kind::Div}, {"/=", Kind::Div}, {"%", Kind::Mod}, {"%=", Kind::Mod},
        {"==", Kind::Eq}, {"!=", Kind::Ne}, {"<", Kind::Lt}, {"<=", Kind::Le}, {">", Kind::Gt}, {">=", Kind::Ge}};
    kind = Kind::Unknown;
    for (const auto& entry : kinds)
        if (op == entry.first) kind = entry.second;
}

Value BinOpNode::eval(Context& ctx) {
    // Short-circuit before the right side runs: `x != 0 && 10 / x > 1` must not divide by zero.
    if (kind == Kind::And || kind == Kind::Or) {
        bool l = truth(left->eval(ctx), "left");
        if (l == (kind == Kind::Or)) return {"bool", l ? "true" : "false"};
        return {"bool", truth(right->eval(ctx), "right") ? "true" : "false"};
    }

    Value lval = left->eval(ctx);
    Value rval = right->eval(ctx);

    // Two ints are the common case: arithmetic and comparison without any conversion.
    if (lval.value.isInteger() && rval.value.isInteger() && lval.type == "int" && rval.type == "int") {
        long long l = lval.value.integerValue(), r = rval.value.integerValue();
        switch (kind) {
            case Kind::Add: return {"int", runtime::intResult(l + r, op)};
            case Kind::Sub: return {"int", runtime::intResult(l - r, op)};
            case Kind::Mul: return {"int", runtime::intResult(l * r, op)};
            case Kind::Div:
            case Kind::Mod:
                if (r == 0) throw std::runtime_error("Runtime Error: Division by zero");
                return {"int", runtime::intResult(kind == Kind::Div ? l / r : l % r, op)};
            case Kind::Eq: return {"bool", l == r ? "true" : "false"};
            case Kind::Ne: return {"bool", l != r ? "true" : "false"};
            case Kind::Lt: return {"bool", l < r ? "true" : "false"};
            case Kind::Le: return {"bool", l <= r ? "true" : "false"};
            case Kind::Gt: return {"bool", l > r ? "true" : "false"};
            case Kind::Ge: return {"bool", l >= r ? "true" : "false"};
            default: break;
        }
    }

    bool equality = kind == Kind::Eq || kind == Kind::Ne;
    bool concatenation = kind == Kind::Add && (lval.type == "string" || rval.type == "string");
    if ((lval.ref || rval.ref) && !equality && !concatenation)
        throw std::runtime_error("Type Error: operator '" + op + "' cannot be applied to " + containerName(lval.ref ? lval : rval));

    switch (kind) {
        case Kind::Add:
            if (concatenation) return {"string", runtime::display(lval) + runtime::display(rval)};
            if (lval.type == "float" || rval.type == "float")
                return {"float", runtime::realResult(number(lval, "left") + number(rval, "right"))};
            return {"int", runtime::intResult(integer(lval, "left") + integer(rval, "right"), op)};
        case Kind::Sub:
        case Kind::Mul:
        case Kind::Div:
        case Kind::Mod: {
            bool division = kind == Kind::Div || kind == Kind::Mod;
            if (lval.type == "float" || rval.type == "float") {
                double l = number(lval, "left"), r = number(rval, "right");
                if (r == 0.0 && division) throw std::runtime_error("Runtime Error: Division by zero");
                double result = kind == Kind::Sub ? l - r : kind == Kind::Mul ? l * r : kind == Kind::Div ? l / r : std::fmod(l, r);
                return {"float", runtime::realResult(result)};
            }
            long long l = integer(lval, "left"), r = integer(rval, "right");
            if (r == 0 && division) throw std::runtime_error("Runtime Error: Division by zero");
            long long result = kind == Kind::Sub ? l - r : kind == Kind::Mul ? l * r : kind == Kind::Div ? l / r : l % r;
            return {"int", runtime::intResult(result, op)};
        }
        case Kind::Eq: case Kind::Ne: case Kind::Lt: case Kind::Le: case Kind::Gt: case Kind::Ge: {
            auto decide = [&](auto l, auto r) {
                switch (kind) {
                    case Kind::Eq: return l == r;
                    case Kind::Ne: return l != r;
                    case Kind::Lt: return l < r;
                    case Kind::Le: return l <= r;
                    case Kind::Ge: return l >= r;
                    default: return l > r;
                }
            };
            bool result;
            if (lval.type == "string" && rval.type == "string") {
                result = decide(lval.value.str(), rval.value.str());
            } else if (lval.type == "bool" && rval.type == "bool") {
                result = decide(truth(lval, "left"), truth(rval, "right"));
            } else if (lval.ref || rval.ref) {
                // Arrays, maps and structs are equal when their contents are.
                if (!equality) throw std::runtime_error("Type Error: operator '" + op + "' cannot be applied to " + containerName(lval.ref ? lval : rval));
                result = runtime::deepEqual(lval, rval) == (kind == Kind::Eq);
            } else {
                result = decide(number(lval, "left"), number(rval, "right"));
            }
            return {"bool", result ? "true" : "false"};
        }
        default:
            throw std::runtime_error("Runtime Error: unknown operator '" + op + "'");
    }
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
    Value* target = ref.find(ctx, name);
    if (!target) notFound(name);
    const char* op = delta > 0 ? "++" : "--";
    if (target->type.is(TypeName::Kind::Float)) {
        Value old = *target;
        double value = runtime::toNumber(old, "variable '" + name + "'");
        target->value = runtime::realResult(value + delta);
        return old;
    }
    if (!target->type.is(TypeName::Kind::Int))
        throw std::runtime_error("Type Error: '" + std::string(op) + "' needs an int or float variable, '" + name + "' is " + target->type);
    long long value = intArg(*target, "variable", name);
    target->value = runtime::intResult(value + delta, op);
    return {"int", Text::integer(value)};
}

Value ArrayDeclNode::eval(Context& ctx) {
    if (duplicate) throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    Context& scope = global ? *ctx.getRoot() : ctx;
    if (slot < 0 && !global && scope.variables.count(name))
        throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
    Value value;
    if (initializer) {
        value = initializer->eval(ctx);
        runtime::coerce("array", value, "initializer of array '" + name + "'");
    } else {
        long long size = sizeNode ? intArg(sizeNode->eval(ctx), "size of array", name) : 0;
        if (size < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
        value = runtime::makeArray(std::vector<Value>(static_cast<size_t>(size), {"int", Text::integer(0)}));
    }
    if (slot >= 0 && ctx.slots) ctx.slots[slot] = std::move(value);
    else scope.defineVar(name, "array", value);
    return {"void", ""};
}

Value ArrayLiteralNode::eval(Context& ctx) {
    std::vector<Value> items;
    items.reserve(elements.size());
    for (auto& element : elements) {
        items.push_back(element->eval(ctx));
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
        Value* found = variable->ref.find(ctx, variable->name);
        if (!found) notFound(variable->name);
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
    slot = std::move(assigned);
    return {"void", ""};
}

Value MapLiteralNode::eval(Context& ctx) {
    Value map = runtime::makeMap();
    for (auto& entry : entries) {
        std::string key = keyOf(entry.first->eval(ctx));
        Value value = entry.second->eval(ctx);
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
    // A pending return, break or continue waits while finally runs, unless finally
    // itself leaves by one of them.
    auto runCleanup = [&] {
        if (!cleanup) return;
        auto pending = guard.flow;
        Value returned = std::move(guard.returned);
        guard.flow = runtime::StackGuard::Flow::None;
        cleanup->eval(ctx);
        if (guard.flow == runtime::StackGuard::Flow::None) {
            guard.flow = pending;
            guard.returned = std::move(returned);
        }
    };
    auto cleanupAfter = [&](auto&& run) {
        try {
            run();
        } catch (...) {
            guard.flow = runtime::StackGuard::Flow::None;
            runCleanup();
            throw;
        }
        runCleanup();
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
        Context scope(ctx);
        SlotReset reset{errorSlot >= 0 ? scope.slots : nullptr, errorSlot, errorSlot + 1};
        if (!errorName.empty()) {
            if (errorSlot >= 0 && scope.slots) scope.slots[errorSlot] = {"string", message};
            else scope.defineVar(errorName, "string", {"string", message});
        }
        handler->eval(scope);
    });
    return {"void", ""};
}

Value ThrowNode::eval(Context& ctx) {
    Value value = message->eval(ctx);
    throw std::runtime_error(runtime::display(value));
}

Value BlockNode::eval(Context& ctx) {
    Context inner(ctx);
    Context& scope = scoped ? inner : ctx;
    SlotReset reset{endSlot > firstSlot ? scope.slots : nullptr, firstSlot, endSlot};
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
        if (guard.flow != runtime::StackGuard::Flow::None) break;
    }
    return {"void", ""};
}

Value SwitchNode::eval(Context& ctx) {
    Value switchValue = expr->eval(ctx);
    runtime::StackGuard& guard = runtime::stackGuard();
    // break ends the switch; return and continue leave it for whatever encloses it.
    auto finished = [&] {
        if (guard.flow == runtime::StackGuard::Flow::None) return false;
        if (guard.flow == runtime::StackGuard::Flow::Break) guard.flow = runtime::StackGuard::Flow::None;
        return true;
    };
    bool matched = false;
    for (auto& caseItem : cases) {
        if (!matched) {
            Value caseValue = caseItem.first->eval(ctx);
            double l = 0, r = 0;
            bool numeric = runtime::tryNumber(switchValue, l) && runtime::tryNumber(caseValue, r) &&
                           switchValue.type != "string" && caseValue.type != "string";
            matched = numeric ? l == r : runtime::deepEqual(switchValue, caseValue);
        }
        // Without break, execution falls through into the following cases.
        if (matched) {
            caseItem.second->eval(ctx);
            if (finished()) return {"void", ""};
        }
    }
    if (defaultCase) {
        defaultCase->eval(ctx);
        finished();
    }
    return {"void", ""};
}

} // namespace foxlang
