// Runs bytecode (Compiler.cpp). A FoxLang call is a native call of execute(), so the
// native stack is what the call depth limit measures, as it always has. Registers live
// on a stack of fixed chunks: a frame's registers never move while it runs, so the
// debugger may point at them.
#include "foxlang/Bytecode.h"
#include "foxlang/AST.h"
#include "foxlang/Debug.h"
#include "foxlang/Platform.h"
#include "Operations.h"
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(_MSC_VER)
#define FOXLANG_COLD __declspec(noinline)
#define FOXLANG_APART __declspec(noinline)
#else
#define FOXLANG_COLD __attribute__((noinline, cold))
#define FOXLANG_APART __attribute__((noinline))
#endif

namespace foxlang::vm {

using namespace bytecode;

bool treeWalker() {
    static const bool tree = platform::getEnvVar("FOXLANG_TREE") == "1";
    return tree;
}

namespace {

// ---------------------------------------------------------------- registers

struct RegisterStack {
    std::vector<std::unique_ptr<Value[]>> chunks;
    std::vector<size_t> sizes;
    size_t chunk = 0;
    Value* top = nullptr;
    Value* limit = nullptr;
};

thread_local RegisterStack registers;

// The registers of one frame, all empty when it begins and emptied again when it ends.
class Window {
public:
    explicit Window(size_t size) : size_(size), top_(registers.top), limit_(registers.limit), chunk_(registers.chunk) {
        RegisterStack& stack = registers;
        if (!stack.top || stack.top + size > stack.limit) nextChunk(size);
        base_ = stack.top;
        stack.top += size;
    }
    ~Window() {
        for (size_t i = 0; i < size_; ++i) base_[i] = Value();
        registers.top = top_;
        registers.limit = limit_;
        registers.chunk = chunk_;
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Value* base() const { return base_; }

private:
    static void nextChunk(size_t size);
    size_t size_;
    Value* base_ = nullptr;
    Value* top_;
    Value* limit_;
    size_t chunk_;
};

void Window::nextChunk(size_t size) {
    RegisterStack& stack = registers;
    size_t next = stack.top ? stack.chunk + 1 : 0;
    if (next < stack.chunks.size() && stack.sizes[next] < size) {
        stack.chunks.resize(next);
        stack.sizes.resize(next);
    }
    if (next >= stack.chunks.size()) {
        size_t capacity = std::max<size_t>(size, size_t{1} << 14);
        stack.chunks.emplace_back(new Value[capacity]);
        stack.sizes.push_back(capacity);
    }
    stack.chunk = next;
    stack.top = stack.chunks[next].get();
    stack.limit = stack.top + stack.sizes[next];
}

// ---------------------------------------------------------------- debugger

// What a debugger sees of a running frame: a scope for the function (its slots are
// the registers) and one for every block open inside it.
struct DebugFrame {
    DebugHook* hook;
    Context* function;
    std::vector<std::unique_ptr<Context>> owned;
    std::vector<Context*> chain;
    Context* current() { return chain.empty() ? function : chain.back(); }

    void enter(bool scoped) {
        Context* scope = current();
        // A program's top level is the root scope itself, not a block inside it.
        if (scoped) {
            owned.push_back(std::make_unique<Context>(*scope));
            scope = owned.back().get();
        } else {
            owned.push_back(nullptr);
        }
        chain.push_back(scope);
        hook->enterScope(*scope);
    }
    void leave() {
        hook->leaveScope();
        chain.pop_back();
        owned.pop_back();
    }
    void leaveTo(size_t depth) {
        while (chain.size() > depth) leave();
    }
};

// ---------------------------------------------------------------- helpers

[[noreturn]] FOXLANG_COLD void notFound(const std::string& name) {
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

FOXLANG_APART Value* lookUp(GlobalSite& site, Context& root) {
    auto found = root.variables.find(site.name);
    if (found == root.variables.end()) notFound(site.name);
    site.cached = &found->second;
    site.root = &root;
    site.generation = root.generation;
    return site.cached;
}

inline Value* global(GlobalSite& site, Context& root) {
    if (site.cached && site.root == &root && site.generation == root.generation) return site.cached;
    return lookUp(site, root);
}

bool fitsInt(long long value) { return value >= -2147483648LL && value <= 2147483647LL; }

int depthAt(const Proto& proto, int pc) {
    int depth = 0;
    for (const auto& body : proto.tryBodies)
        if (pc >= body.first && pc < body.second) ++depth;
    return depth;
}

const Handler* handlerFor(const Proto& proto, int pc, bool runtimeError) {
    const Handler* best = nullptr;
    for (const auto& handler : proto.handlers) {
        if (pc < handler.start || pc >= handler.end) continue;
        if (!handler.finally && !runtimeError) continue;
        if (!best || handler.level > best->level) best = &handler;
    }
    return best;
}

const char* statementName(int kind) {
    return kind == 0 ? "if" : kind == 1 ? "while" : "for";
}

bool truth(const Value& value, int kind) {
    if (value.isBool()) return value.asBool();
    return conditionTruth(value, statementName(kind)); // raises the error
}

FOXLANG_APART void setPath(Proto& proto, const SetPath& path, Value* R, Value& assigned, Context& root) {
    Value* at = path.slot >= 0 ? &R[path.slot] : global(proto.globals[static_cast<size_t>(path.global)], root);
    const std::string* declared = nullptr;
    bool create = path.op == "=";
    for (size_t i = 0; i < path.steps.size(); ++i) {
        const auto& step = path.steps[i];
        bool last = i + 1 == path.steps.size();
        at = step.field ? &runtime::member(*at, step.name, last && create, last ? &declared : nullptr)
                        : &runtime::element(*at, R[step.key], last && create);
    }
    Value value = create ? assigned : runtime::binary(path.kind, path.op, *at, assigned);
    // A struct field keeps its declared type; elements and map values take any value.
    if (declared) runtime::coerce(*declared, value, "field '" + path.steps.back().name + "'");
    *at = std::move(value);
}

Value execute(Proto& proto, Value* R, Context& root, DebugFrame* debug);

FOXLANG_COLD const Proto& compile(const FuncDefNode& function, bool debug) {
    auto& slot = debug ? function.block()->debugProto : function.block()->proto;
    slot = compileFunction(function, debug);
    return *slot;
}

inline const Proto& protoOf(const FuncDefNode& function, bool debug) {
    const auto& slot = debug ? function.block()->debugProto : function.block()->proto;
    return slot ? *slot : compile(function, debug);
}

// A call under a debugger, kept apart so that an ordinary call's native frame stays
// small: the depth of recursion a program gets depends on it.
FOXLANG_COLD Value executeDebugged(Proto& proto, Value* R, Context& root, DebugHook* hook) {
    // Lexical scope: a function sees globals, never the caller's locals.
    Context scope;
    scope.parent = &root;
    scope.interpreter = root.interpreter;
    scope.slots = R;
    scope.slotNames = proto.slotNames.get();
    scope.frame = true;
    DebugFrame frame{hook, &scope, {}, {}};
    struct Leave {
        DebugFrame& frame;
        ~Leave() {
            frame.leaveTo(0);
            frame.hook->leaveFunction();
        }
    };
    hook->enterFunction(proto.name, proto.file, proto.line, scope);
    Leave leave{frame};
    return execute(proto, R, root, &frame);
}

FOXLANG_COLD void convert(const Conversion& conversion, Value& value) {
    runtime::coerce(conversion.type, value, conversion.what);
}

[[noreturn]] FOXLANG_COLD void wrongCount(const Proto& proto, size_t count) {
    throw std::runtime_error("Runtime Error: function '" + proto.name + "' expects " + std::to_string(proto.params.size()) +
                             " arguments, got " + std::to_string(count));
}

[[noreturn]] FOXLANG_COLD void noResult(const Proto& proto) {
    throw std::runtime_error("Runtime Error: function '" + proto.name + "' must return " + proto.result.type +
                             " but ended without a value");
}

Value callFunction(const FuncDefNode& function, Value* args, size_t count, Context& root) {
    DebugHook* hook = runtime::debugHook();
    Proto& proto = const_cast<Proto&>(protoOf(function, hook != nullptr));
    if (count != proto.params.size()) wrongCount(proto, count);
    Value result;
    {
        Window window(static_cast<size_t>(proto.registers));
        Value* R = window.base();
        for (size_t i = 0; i < count; ++i) {
            R[i] = std::move(args[i]);
            const Conversion& param = proto.params[i];
            if (!runtime::storesAsIs(param.kind, R[i])) convert(param, R[i]);
        }
        CallDepth depth(proto.name);
        result = hook ? executeDebugged(proto, R, root, hook) : execute(proto, R, root, nullptr);
    }
    if (proto.result.kind == Value::Kind::Void) return Value();
    if (result.isVoid()) noResult(proto);
    if (!runtime::storesAsIs(proto.result.kind, result)) convert(proto.result, result);
    return result;
}

FOXLANG_APART void refresh(CallSite& site, Context& root) {
    if (!site.resolved) {
        site.builtin = runtime::findBuiltin(site.name);
        site.resolved = true;
    }
    site.function = static_cast<const FuncDefNode*>(root.getFunc(site.name).get());
    site.root = &root;
    site.generation = root.functionGeneration;
}

// A call of a builtin or of a struct's constructor, or of a FoxLang function that shares
// its name with a builtin.
FOXLANG_APART void callOther(const CallSite& site, Value* args, size_t count, Context& root) {
    // A builtin and a FoxLang function may share a name, like get(items, i) and the
    // server's get(path, handler). The builtin wins whenever the arguments fit it.
    if (site.builtin && (!site.function || runtime::acceptsArguments(*site.builtin, Arguments(args, count)))) {
        Value result = runtime::invoke(*site.builtin, Arguments(args, count), root);
        args[0] = std::move(result);
        return;
    }
    if (site.function) {
        Value result = callFunction(*site.function, args, count, root);
        args[0] = std::move(result);
        return;
    }
    // A struct's name called like a function builds a value of it.
    if (auto type = root.getStruct(site.name)) {
        Value result = runtime::construct(type, args, count, root);
        args[0] = std::move(result);
        return;
    }
    throw std::runtime_error("Runtime Error: Function '" + site.name + "' not found!");
}

inline void call(Proto& proto, const Instr& in, Value* R, Context& root) {
    CallSite& site = proto.calls[static_cast<size_t>(in.b)];
    if (!site.resolved || site.root != &root || site.generation != root.functionGeneration) refresh(site, root);
    Value* args = R + in.a;
    if (site.builtin || !site.function) callOther(site, args, static_cast<size_t>(in.c), root);
    else args[0] = callFunction(*site.function, args, static_cast<size_t>(in.c), root);
}

// ---------------------------------------------------------------- the loop

// An error left instruction pc: where the handler that takes it begins. Called while
// the error is being handled; raises it again when no handler of this function takes it.
FOXLANG_COLD int recover(const Proto& proto, int pc, Value* R, DebugFrame* debug, std::exception_ptr* pending) {
    runtime::StackGuard& guard = runtime::stackGuard();
    std::exception_ptr error = std::current_exception();
    std::string message;
    bool runtimeError = false;
    try {
        throw;
    } catch (const std::exception& e) {
        message = e.what();
        runtimeError = true;
    } catch (...) {
    }
    // The innermost function that sees an error names its line; the functions it
    // passes through on the way out keep that.
    if (runtimeError && guard.located != error) {
        guard.located = error;
        guard.line = proto.lines[static_cast<size_t>(pc)];
        guard.file = proto.file;
    }
    if (debug && runtimeError) debug->hook->error(message, guard.tryDepth > 0);
    const Handler* handler = handlerFor(proto, pc, runtimeError);
    int depth = depthAt(proto, pc);
    if (!handler) {
        guard.tryDepth -= depth;
        if (debug) debug->leaveTo(0);
        throw;
    }
    guard.tryDepth += depthAt(proto, handler->target) - depth;
    if (debug) debug->leaveTo(static_cast<size_t>(handler->scopes));
    for (int i = handler->clearFrom; i < handler->clearTo; ++i) R[i] = Value();
    if (handler->finally) pending[static_cast<size_t>(handler->pending)] = error;
    else if (handler->message >= 0) R[handler->message] = Value::string(message);
    return handler->target;
}

[[noreturn]] FOXLANG_COLD void raise(const std::string& message) { throw std::runtime_error(message); }

[[noreturn]] FOXLANG_COLD void alreadyDeclared(const std::string& name) {
    throw std::runtime_error("Runtime Error: Variable '" + name + "' is already declared in this scope");
}

FOXLANG_COLD Value sizedArray(const Value& size, const std::string& name) {
    long long count = intArg(size, "size of array", name);
    if (count < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
    return runtime::makeArray(std::vector<Value>(static_cast<size_t>(count), Value::integer(0)));
}

// The slow paths of instructions, apart from the loop: each would otherwise keep its
// own temporaries in the loop's native frame, which every FoxLang call pays for.
FOXLANG_APART void binarySlow(runtime::Operator op, const std::string& text, Value& out, const Value& l, const Value& r) {
    out = runtime::binary(op, text, l, r);
}

FOXLANG_APART bool compareSlow(runtime::Operator op, const std::string& text, const Value& l, const Value& r) {
    return runtime::binary(op, text, l, r).asBool();
}

FOXLANG_APART void newArray(Value& out, Value* items, int count) {
    std::vector<Value> values;
    values.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) values.push_back(std::move(items[i]));
    out = runtime::makeArray(std::move(values));
}

FOXLANG_APART void newMap(Value& out, Value* pairs, int count) {
    Value map = runtime::makeMap();
    for (int i = 0; i < count; ++i) map.ref()->slot(pairs[2 * i].str()) = std::move(pairs[2 * i + 1]);
    out = std::move(map);
}

FOXLANG_APART void index(Value& out, Value& base, const Value& key) {
    Value element = runtime::element(base, key, false);
    out = std::move(element);
}

FOXLANG_APART void field(Value& out, Value& base, FieldSite& site) {
    Value member = runtime::member(base, site.name, false);
    if (base.is(Value::Kind::Struct)) {
        const Object& object = *base.ref();
        const auto& fields = object.structType->fields;
        for (size_t i = 0; i < fields.size(); ++i)
            if (fields[i].name == site.name) site.index = i;
        site.type = object.structType.get();
    }
    out = std::move(member);
}

FOXLANG_APART void incrementSlow(Value* out, Value& target, int delta, const std::string& name) {
    Value old = runtime::postIncrement(target, delta, name);
    if (out) *out = std::move(old);
}

FOXLANG_APART void unary(Op op, Value& out, const Value& operand) {
    out = op == Op::Negate ? runtime::negate(operand) : runtime::logicalNot(operand);
}

FOXLANG_APART void defineGlobal(Context& root, const std::string& name, Value* value) {
    if (!value) {
        if (root.variables.count(name)) alreadyDeclared(name);
    } else {
        root.variables[name] = std::move(*value);
    }
}

FOXLANG_APART void setGlobal(GlobalSite& site, Context& root, Value& value) {
    runtime::assign(*global(site, root), std::move(value), site.name);
}

FOXLANG_APART void assignSlow(Value& target, Value& value, const std::string& name) {
    runtime::assign(target, std::move(value), name);
}

FOXLANG_APART void mapKey(Value& key) { key = Value::string(runtime::keyOf(key)); }

[[noreturn]] FOXLANG_APART void rethrow(std::exception_ptr& pending) {
    std::exception_ptr error = pending;
    pending = nullptr;
    std::rethrow_exception(error);
}

Value execute(Proto& proto, Value* R, Context& root, DebugFrame* debug) {
    for (const auto& constant : proto.preload) R[constant.first] = proto.constants[static_cast<size_t>(constant.second)];
    const Instr* code = proto.code.data();
    const Instr* ip = code;
    const Value* K = proto.constants.data();
    std::unique_ptr<std::exception_ptr[]> pending;
    if (proto.pendingErrors > 0) pending.reset(new std::exception_ptr[static_cast<size_t>(proto.pendingErrors)]);
    runtime::StackGuard& guard = runtime::stackGuard();

    for (;;) {
        try {
            for (;;) {
                const Instr& in = *ip++;
                switch (in.op) {
                    case Op::Move:
                        if (in.x) R[in.a] = std::move(R[in.b]);
                        else R[in.a] = R[in.b];
                        break;
                    case Op::LoadConst:
                        R[in.a] = K[in.b];
                        break;
                    case Op::Clear:
                        for (int i = in.a; i < in.b; ++i) R[i] = Value();
                        break;
                    case Op::GetGlobal:
                        R[in.a] = *global(proto.globals[static_cast<size_t>(in.b)], root);
                        break;
                    case Op::SetGlobal: {
                        GlobalSite& site = proto.globals[static_cast<size_t>(in.b)];
                        Value& target = *global(site, root);
                        Value& value = R[in.a];
                        if (target.kind() == value.kind() && !target.is(Value::Kind::Struct)) target = std::move(value);
                        else setGlobal(site, root, value);
                        break;
                    }
                    case Op::DefineGlobal:
                        defineGlobal(root, proto.globals[static_cast<size_t>(in.b)].name, in.a < 0 ? nullptr : &R[in.a]);
                        break;
                    case Op::Coerce: {
                        const Conversion& conversion = proto.conversions[static_cast<size_t>(in.b)];
                        if (!runtime::storesAsIs(conversion.kind, R[in.a])) runtime::coerce(conversion.type, R[in.a], conversion.what);
                        break;
                    }
                    case Op::Assign: {
                        Value& target = R[in.a];
                        Value& value = R[in.b];
                        if (target.kind() == value.kind() && !target.is(Value::Kind::Struct)) target = std::move(value);
                        else assignSlow(target, value, K[in.c].str());
                        break;
                    }
                    case Op::Zero:
                        R[in.a] = runtime::zeroValue(K[in.b].str(), root);
                        break;
                    case Op::NewSized:
                        R[in.a] = sizedArray(R[in.b], K[in.c].str());
                        break;
                    case Op::Fail:
                        raise(K[in.a].str());

                    case Op::Add: {
                        const Value& l = R[in.b];
                        const Value& r = R[in.c];
                        if (l.isInt() && r.isInt()) {
                            long long sum = l.asInt() + r.asInt();
                            if (fitsInt(sum)) {
                                R[in.a].setInt(sum);
                                break;
                            }
                        }
                        binarySlow(runtime::Operator::Add, proto.texts[in.y], R[in.a], l, r);
                        break;
                    }
                    case Op::Sub: {
                        const Value& l = R[in.b];
                        const Value& r = R[in.c];
                        if (l.isInt() && r.isInt()) {
                            long long difference = l.asInt() - r.asInt();
                            if (fitsInt(difference)) {
                                R[in.a].setInt(difference);
                                break;
                            }
                        }
                        binarySlow(runtime::Operator::Sub, proto.texts[in.y], R[in.a], l, r);
                        break;
                    }
                    case Op::Mul: {
                        const Value& l = R[in.b];
                        const Value& r = R[in.c];
                        if (l.isInt() && r.isInt()) {
                            long long product = l.asInt() * r.asInt();
                            if (fitsInt(l.asInt()) && fitsInt(r.asInt()) && fitsInt(product)) {
                                R[in.a].setInt(product);
                                break;
                            }
                        }
                        binarySlow(runtime::Operator::Mul, proto.texts[in.y], R[in.a], l, r);
                        break;
                    }
                    case Op::Div:
                    case Op::Mod: {
                        const Value& l = R[in.b];
                        const Value& r = R[in.c];
                        if (l.isInt() && r.isInt() && r.asInt() != 0 && fitsInt(l.asInt()) && fitsInt(r.asInt())) {
                            long long result = in.op == Op::Div ? l.asInt() / r.asInt() : l.asInt() % r.asInt();
                            if (fitsInt(result)) {
                                R[in.a].setInt(result);
                                break;
                            }
                        }
                        binarySlow(in.op == Op::Div ? runtime::Operator::Div : runtime::Operator::Mod, proto.texts[in.y],
                                   R[in.a], l, r);
                        break;
                    }
                    case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
                        const Value& l = R[in.b];
                        const Value& r = R[in.c];
                        auto op = static_cast<runtime::Operator>(static_cast<int>(runtime::Operator::Eq) +
                                                                 (static_cast<int>(in.op) - static_cast<int>(Op::Eq)));
                        if (l.isInt() && r.isInt()) {
                            long long a = l.asInt(), b = r.asInt();
                            bool result = op == runtime::Operator::Eq ? a == b : op == runtime::Operator::Ne ? a != b
                                        : op == runtime::Operator::Lt ? a < b : op == runtime::Operator::Le ? a <= b
                                        : op == runtime::Operator::Gt ? a > b : a >= b;
                            R[in.a].setBool(result);
                            break;
                        }
                        binarySlow(op, proto.texts[in.y], R[in.a], l, r);
                        break;
                    }
                    case Op::Negate:
                    case Op::Not:
                        unary(in.op, R[in.a], R[in.b]);
                        break;
                    case Op::Truth:
                        runtime::operandTruth(R[in.a], in.x ? "right" : "left", proto.texts[in.y]);
                        break;

                    case Op::Jump:
                        ip = code + in.a;
                        break;
                    case Op::JumpIfFalse:
                        if (!truth(R[in.a], in.x)) ip = code + in.b;
                        break;
                    case Op::JumpIfTrue:
                        if (truth(R[in.a], in.x)) ip = code + in.b;
                        break;
                    case Op::Compare: {
                        const Value& l = R[in.a];
                        const Value& r = R[in.b];
                        auto op = static_cast<runtime::Operator>(in.x);
                        bool result;
                        if (l.isInt() && r.isInt()) {
                            long long a = l.asInt(), b = r.asInt();
                            result = op == runtime::Operator::Lt ? a < b : op == runtime::Operator::Le ? a <= b
                                   : op == runtime::Operator::Gt ? a > b : op == runtime::Operator::Ge ? a >= b
                                   : op == runtime::Operator::Eq ? a == b : a != b;
                        } else {
                            result = compareSlow(op, proto.texts[in.y >> 1], l, r);
                        }
                        if (result == ((in.y & 1) != 0)) ip = code + in.c;
                        break;
                    }

                    case Op::Call:
                        call(proto, in, R, root);
                        break;
                    case Op::Return:
                        return std::move(R[in.a]);
                    case Op::ReturnVoid:
                        return Value();

                    case Op::NewArray:
                        newArray(R[in.a], R + in.b, in.c);
                        break;
                    case Op::NewMap:
                        newMap(R[in.a], R + in.b, in.c);
                        break;
                    case Op::MapKey:
                        if (!R[in.a].isString()) mapKey(R[in.a]);
                        break;
                    case Op::Index: {
                        // An array element by an int in range is the common case.
                        const Value& base = R[in.b];
                        const Value& key = R[in.c];
                        if (base.is(Value::Kind::Array) && key.isInt() && in.a != in.b) {
                            const auto& items = base.ref()->items;
                            long long at = key.asInt();
                            if (at >= 0 && static_cast<unsigned long long>(at) < items.size()) {
                                R[in.a] = items[static_cast<size_t>(at)];
                                break;
                            }
                        }
                        index(R[in.a], R[in.b], R[in.c]);
                        break;
                    }
                    case Op::Field: {
                        FieldSite& site = proto.fields[static_cast<size_t>(in.c)];
                        const Value& base = R[in.b];
                        if (base.is(Value::Kind::Struct) && base.ref()->structType.get() == site.type && in.a != in.b) {
                            R[in.a] = base.ref()->items[site.index];
                            break;
                        }
                        field(R[in.a], R[in.b], site);
                        break;
                    }
                    case Op::SetPath:
                        setPath(proto, proto.paths[static_cast<size_t>(in.b)], R, R[in.a], root);
                        break;
                    case Op::Increment: {
                        Value& target = R[in.b];
                        int delta = (in.c & 1) ? 1 : -1;
                        if (target.isInt() && fitsInt(target.asInt()) && fitsInt(target.asInt() + delta)) {
                            long long old = target.asInt();
                            target.setInt(old + delta);
                            if (in.a >= 0) R[in.a].setInt(old);
                            break;
                        }
                        incrementSlow(in.a >= 0 ? &R[in.a] : nullptr, target, delta, K[in.c >> 1].str());
                        break;
                    }
                    case Op::IncrementGlobal: {
                        GlobalSite& site = proto.globals[static_cast<size_t>(in.b)];
                        incrementSlow(in.a >= 0 ? &R[in.a] : nullptr, *global(site, root), in.c ? 1 : -1, site.name);
                        break;
                    }

                    case Op::Declare:
                        proto.declarations[static_cast<size_t>(in.b)]->eval(root);
                        break;
                    case Op::Throw:
                        raise(runtime::display(R[in.a]));
                    case Op::Rethrow:
                        rethrow(pending[static_cast<size_t>(in.a)]);
                    case Op::TryEnter:
                        ++guard.tryDepth;
                        break;
                    case Op::TryLeave:
                        --guard.tryDepth;
                        break;
                    case Op::Match:
                        R[in.a].setBool(runtime::switchMatches(R[in.b], R[in.c]));
                        break;

                    case Op::Statement:
                        guard.line = in.a;
                        guard.file = proto.file;
                        if (debug) debug->hook->statement(proto.file, in.a);
                        break;
                    case Op::ScopeEnter:
                        if (debug) debug->enter(in.a != 0);
                        break;
                    case Op::ScopeLeave:
                        if (debug) debug->leave();
                        break;
                }
            }
        } catch (...) {
            ip = code + recover(proto, static_cast<int>(ip - code) - 1, R, debug, pending.get());
        }
    }
}

} // namespace

Value call(const FuncDefNode& function, Value* args, size_t count, Context& caller) {
    return callFunction(function, args, count, *caller.getRoot());
}

void run(BlockNode& program, Context& root, Unit unit) {
    DebugHook* hook = unit == Unit::Program ? runtime::debugHook() : nullptr;
    std::shared_ptr<Proto> proto = compileProgram(program, unit, hook != nullptr);
    Window window(static_cast<size_t>(proto->registers));
    if (unit != Unit::Program) {
        execute(*proto, window.base(), root, nullptr);
        return;
    }
    // The blocks of the program keep their variables in numbered slots, alive while it
    // runs; the debugger and its console find them through the root scope.
    struct Frame {
        Context& root;
        Value* slots;
        const std::vector<std::string>* names;
        bool frame;
        ~Frame() {
            root.slots = slots;
            root.slotNames = names;
            root.frame = frame;
        }
    } restore{root, root.slots, root.slotNames, root.frame};
    root.slots = window.base();
    root.slotNames = proto->slotNames.get();
    root.frame = true;
    if (!hook) {
        execute(*proto, window.base(), root, nullptr);
        return;
    }
    DebugFrame frame{hook, &root, {}, {}};
    struct Leave {
        DebugFrame& frame;
        ~Leave() { frame.leaveTo(0); }
    } leave{frame};
    execute(*proto, window.base(), root, &frame);
}

} // namespace foxlang::vm
