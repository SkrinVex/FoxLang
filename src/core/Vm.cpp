// Runs bytecode (Compiler.cpp). A FoxLang call is a native call of execute(), so the
// native stack is what the call depth limit measures, as it always has. Registers live
// on a stack of fixed chunks: a frame's registers never move while it runs, so the
// debugger may point at them.
#include "foxlang/Bytecode.h"
#include "foxlang/AST.h"
#include "foxlang/Debug.h"
#include "foxlang/Platform.h"
#include "Operations.h"
#include <algorithm>
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
    // Bounded, so the allocation below cannot be asked for the whole address space.
    if (size > (size_t{1} << 28)) throw std::length_error("register window too large");
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

// A variable of the program's top level, now declared: found by name from now on.
FOXLANG_COLD void declareProgramGlobal(const GlobalSite& site, Value& slot, Context& root) {
    root.programGlobals[site.name] = slot.is(Value::Kind::Box) ? &slot.ref()->items[0] : &slot;
    if (site.declaresConstant) root.constants.insert(site.name);
    if (!site.nullable.empty()) root.nullableGlobals[site.name] = site.nullable;
}

FOXLANG_COLD void markDeclared(DebugFrame& frame, int from, int to, bool declared) {
    auto& marks = frame.function->declared;
    for (int i = from; i < to && i >= 0 && static_cast<size_t>(i) < marks.size(); ++i) marks[static_cast<size_t>(i)] = declared;
}

[[noreturn]] FOXLANG_COLD void notFound(const std::string& name) {
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

// A global by name: a variable of the root, or one of the running program's top level.
Value* globalVariable(Context& root, const std::string& name) {
    auto found = root.variables.find(name);
    if (found != root.variables.end()) return &found->second;
    if (root.programGlobals.empty()) return nullptr;
    auto program = root.programGlobals.find(name);
    return program == root.programGlobals.end() ? nullptr : program->second;
}

FOXLANG_APART Value* lookUp(GlobalSite& site, Context& root, Context& scope) {
    if (site.byName) {
        Value* found = scope.findVar(site.name);
        if (!found) notFound(site.name);
        // The debugger's console reaches globals by name; a constant stays one there too.
        Context& owner = *scope.getRoot();
        auto global = owner.variables.find(site.name);
        site.constant = global != owner.variables.end() && &global->second == found && owner.constants.count(site.name) > 0;
        return found;
    }
    Value* variable = globalVariable(root, site.name);
    if (!variable) notFound(site.name);
    site.constant = !root.constants.empty() && root.constants.count(site.name) > 0;
    site.cached = variable;
    site.root = &root;
    site.generation = root.generation;
    return site.cached;
}

// The name of a function, or of a builtin, read as a value: func f = add;
Value functionValue(const std::string& name, Context& root) {
    auto function = root.getFunc(name);
    const runtime::Builtin* builtin = function ? nullptr : runtime::findBuiltin(name);
    if (!function && !builtin) return Value();
    auto callee = std::make_shared<Callee>();
    callee->name = name;
    callee->function = function;
    callee->builtin = builtin;
    Value value = Value::container(Value::Kind::Function);
    value.ref()->callee = std::move(callee);
    return value;
}

// A name read as a value: a variable, or else a function of that name.
FOXLANG_APART Value* lookUpValue(GlobalSite& site, Context& root, Context& scope) {
    Value* variable = site.byName ? scope.findVar(site.name) : nullptr;
    if (!site.byName) {
        if (Value* found = globalVariable(root, site.name)) {
            site.constant = !root.constants.empty() && root.constants.count(site.name) > 0;
            site.cached = found;
            site.root = &root;
            site.generation = root.generation;
            return site.cached;
        }
    }
    if (variable) return variable;
    site.function = functionValue(site.name, root);
    if (site.function.isVoid()) notFound(site.name);
    return &site.function;
}

inline Value* readGlobal(GlobalSite& site, Context& root, Context& scope) {
    if (site.cached && site.root == &root && site.generation == root.generation) return site.cached;
    return lookUpValue(site, root, scope);
}

inline Value* global(GlobalSite& site, Context& root, Context& scope) {
    if (site.cached && site.root == &root && site.generation == root.generation) return site.cached;
    return lookUp(site, root, scope);
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

FOXLANG_APART void setPath(Proto& proto, const SetPath& path, Value* R, Value& assigned, Context& root, Context& scope,
                           Object* closure) {
    Value* at;
    if (path.slot >= 0) at = path.boxed ? &R[path.slot].ref()->items[0] : &R[path.slot];
    else if (path.capture >= 0) at = &closure->items[static_cast<size_t>(path.capture)].ref()->items[0];
    else at = global(proto.globals[static_cast<size_t>(path.global)], root, scope);
    const std::string* declared = nullptr;
    const Object* owner = nullptr; // the container of the element written
    bool create = path.op == "=";
    for (size_t i = 0; i < path.steps.size(); ++i) {
        const auto& step = path.steps[i];
        bool last = i + 1 == path.steps.size();
        if (at->ref() && at->ref()->frozen)
            throw std::runtime_error("Runtime Error: the values of an enum cannot be changed");
        owner = at->ref();
        at = step.field ? &runtime::member(*at, step.name, last && create, last ? &declared : nullptr)
                        : &runtime::element(*at, R[step.key], last && create);
    }
    Value value = create ? assigned : runtime::binary(path.kind, path.op, *at, assigned);
    // A struct field keeps its declared type; elements and map values take any value.
    if (declared) runtime::coerce(*declared, value, "field '" + path.steps.back().name + "'");
    else if (owner && owner->elementType) runtime::storeElement(*owner, value); // array<int>, map<string,int>
    *at = std::move(value);
}

Value execute(Proto& proto, Value* R, Context& root, Context& scope, DebugFrame* debug, Object* closure = nullptr);

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
FOXLANG_COLD Value executeDebugged(Proto& proto, Value* R, Context& root, DebugHook* hook, Object* closure = nullptr) {
    // Lexical scope: a function sees globals, never the caller's locals.
    Context scope;
    scope.parent = &root;
    scope.interpreter = root.interpreter;
    scope.slots = R;
    scope.slotNames = proto.slotNames.get();
    scope.frame = true;
    scope.declared.assign(static_cast<size_t>(proto.slots), 0);
    for (size_t i = 0; i < proto.params.size() && i < scope.declared.size(); ++i) scope.declared[i] = 1;
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
    return execute(proto, R, root, root, &frame, closure);
}

FOXLANG_COLD void convert(const Conversion& conversion, Value& value) {
    runtime::coerce(conversion.type, value, conversion.what);
}

[[noreturn]] FOXLANG_COLD void wrongCount(const Proto& proto, size_t count) {
    size_t hidden = proto.method ? 1 : 0; // a method's `this`
    throw std::runtime_error("Runtime Error: " + std::string(proto.method ? "method '" : "function '") + proto.name +
                             "' expects " + std::to_string(proto.params.size() - hidden) + " arguments, got " +
                             std::to_string(count - hidden));
}

[[noreturn]] FOXLANG_COLD void noResult(const Proto& proto) {
    throw std::runtime_error("Runtime Error: function '" + proto.name + "' must return " + proto.result.type +
                             " but ended without a value");
}

FOXLANG_APART void refresh(CallSite& site, Context& root);

// Arguments that need no conversion go straight on to the builtin a forwarding body
// calls: no frame, and an error names the line of the call made to the forwarder.
const runtime::Builtin* forwardsTo(Proto& proto, const Value* args, size_t count, Context& root) {
    if (proto.forward < 0) return nullptr;
    CallSite& site = proto.calls[static_cast<size_t>(proto.forward)];
    if (!site.resolved || site.root != &root || site.generation != root.functionGeneration) refresh(site, root);
    if (!site.builtin || site.function) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        const Conversion& param = proto.params[i];
        if (!param.type.empty() && !runtime::storesAsIs(param.kind, args[i])) return nullptr;
    }
    return site.builtin;
}

Value callFunction(const FuncDefNode& function, Value* args, size_t count, Context& root) {
    DebugHook* hook = runtime::debugHook();
    Proto& proto = const_cast<Proto&>(protoOf(function, hook != nullptr));
    if (count != proto.params.size()) wrongCount(proto, count);
    Value result;
    if (const runtime::Builtin* builtin = forwardsTo(proto, args, count, root)) {
        result = runtime::invoke(*builtin, Arguments(args, count), root);
    } else {
        Window window(static_cast<size_t>(proto.registers));
        Value* R = window.base();
        for (size_t i = 0; i < count; ++i) {
            R[i] = std::move(args[i]);
            const Conversion& param = proto.params[i];
            if (!param.type.empty() && !runtime::storesAsIs(param.kind, R[i])) convert(param, R[i]);
        }
        CallDepth depth(proto.name);
        result = hook ? executeDebugged(proto, R, root, hook) : execute(proto, R, root, root, nullptr);
    }
    if (proto.result.kind == Value::Kind::Void) return Value();
    if (result.isVoid()) {
        if (isNullable(proto.result.type)) return result; // a T? function may return null
        noResult(proto);
    }
    if (!runtime::storesAsIs(proto.result.kind, result)) convert(proto.result, result);
    return result;
}

Value callLambda(Object& function, Value* args, size_t count, Context& root) {
    Proto& proto = *function.callee->lambda;
    if (count != proto.params.size()) wrongCount(proto, count);
    DebugHook* hook = runtime::debugHook();
    Window window(static_cast<size_t>(proto.registers));
    Value* R = window.base();
    for (size_t i = 0; i < count; ++i) {
        R[i] = std::move(args[i]);
        const Conversion& param = proto.params[i];
        if (!param.type.empty() && !runtime::storesAsIs(param.kind, R[i])) convert(param, R[i]);
    }
    CallDepth depth(proto.name);
    return hook ? executeDebugged(proto, R, root, hook, &function) : execute(proto, R, root, root, nullptr, &function);
}

[[noreturn]] FOXLANG_COLD void notFunction(const std::string& name, const Value& value) {
    throw std::runtime_error("Type Error: '" + name + "' is not a function, it holds a value of type '" + value.typeName() + "'");
}

// Calls a function value with its arguments (moved from).
Value callFunctionValue(const Value& function, Value* args, size_t count, Context& root, const std::string& name) {
    if (!function.isFunction()) notFunction(name, function);
    Object& object = *function.ref();
    const Callee& callee = *object.callee;
    if (callee.lambda) return callLambda(object, args, count, root);
    if (callee.function) return callFunction(*static_cast<const FuncDefNode*>(callee.function.get()), args, count, root);
    return runtime::invoke(*callee.builtin, Arguments(args, count), root);
}

FOXLANG_APART void callValueInPlace(Value* R, int base, int count, Context& root, const std::string& name) {
    Value function = std::move(R[base]); // kept alive while it runs
    Value result = callFunctionValue(function, R + base + 1, static_cast<size_t>(count), root, name);
    R[base] = std::move(result);
}

// value.name(args): R[base] is the value, the arguments follow it.
FOXLANG_APART void callMethod(Value* R, int base, int count, Context& root, const std::string& name) {
    Value self = R[base]; // kept alive while the method runs
    if (self.is(Value::Kind::Struct)) {
        const StructType& type = *self.ref()->structType;
        auto method = type.methods.find(name);
        if (method != type.methods.end()) {
            Value result = callFunction(*static_cast<const FuncDefNode*>(method->second.get()), R + base,
                                        static_cast<size_t>(count) + 1, root);
            R[base] = std::move(result);
            return;
        }
        for (size_t i = 0; i < type.fields.size(); ++i) {
            if (type.fields[i].name != name) continue;
            Value function = self.ref()->items[i];
            Value result = callFunctionValue(function, R + base + 1, static_cast<size_t>(count), root, name);
            R[base] = std::move(result);
            return;
        }
        throw std::runtime_error("Runtime Error: struct '" + type.name + "' has no method '" + name + "'");
    }
    if (self.is(Value::Kind::Map)) {
        long at = self.ref()->find(name);
        if (at < 0 && self.ref()->moduleAlias) {
            // m.sqrt(2): a builtin through a module's alias, as the module's own code calls it.
            if (const runtime::Builtin* builtin = runtime::findBuiltin(name)) {
                Value result = runtime::invoke(*builtin, Arguments(R + base + 1, static_cast<size_t>(count)), root);
                R[base] = std::move(result);
                return;
            }
            throw std::runtime_error("Runtime Error: the module has no function '" + name + "'");
        }
        if (at < 0) throw std::runtime_error("Runtime Error: map has no key '" + name + "' to call");
        Value function = self.ref()->items[static_cast<size_t>(at)];
        Value result = callFunctionValue(function, R + base + 1, static_cast<size_t>(count), root, name);
        R[base] = std::move(result);
        return;
    }
    throw std::runtime_error("Type Error: '." + name + "()' needs a struct or a map, got '" + self.typeName() + "'");
}

FOXLANG_APART void makeClosure(Proto& proto, int index, Value& out, Value* R, Object* current) {
    const Proto& lambda = *proto.lambdas[static_cast<size_t>(index)];
    Value function = Value::container(Value::Kind::Function);
    Object& object = *function.ref();
    object.callee = proto.callees[static_cast<size_t>(index)];
    object.items.reserve(lambda.captures.size());
    for (const auto& capture : lambda.captures)
        object.items.push_back(capture.fromCapture ? current->items[static_cast<size_t>(capture.index)] : R[capture.index]);
    out = std::move(function);
}

FOXLANG_APART void box(Value& slot) {
    Value cell = Value::container(Value::Kind::Box);
    cell.ref()->items.push_back(std::move(slot));
    slot = std::move(cell);
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
    // A global variable that holds a function.
    auto variable = root.variables.find(site.name);
    if (variable != root.variables.end() && variable->second.isFunction()) {
        Value function = variable->second;
        Value result = callFunctionValue(function, args, count, root, site.name);
        args[0] = std::move(result);
        return;
    }
    // A struct's name called like a function builds a value of it.
    if (auto type = root.getStruct(site.name)) {
        Value result = runtime::construct(type, args, count, root);
        args[0] = std::move(result);
        return;
    }
    if (variable != root.variables.end()) notFunction(site.name, variable->second);
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

// An error left instruction pc: where the handler that takes it begins, or -1 when no
// handler of this function takes it and the caller must raise it again. It does not
// raise it itself: under MSVC a catch block runs on top of the frames the error left,
// so raising from inside it at every level would stack up until the stack overflows.
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
    std::size_t messageHash = std::hash<std::string>()(message);
    bool alreadyPlaced = guard.placed && guard.placedMessage == messageHash;
    if (runtimeError && !alreadyPlaced && proto.lines[static_cast<size_t>(pc)] > 0) {
        guard.placed = true;
        guard.placedMessage = messageHash;
        guard.line = proto.lines[static_cast<size_t>(pc)];
        guard.file = proto.file;
    }
    if (debug && runtimeError) debug->hook->error(message, guard.tryDepth > 0);
    const Handler* handler = handlerFor(proto, pc, runtimeError);
    int depth = depthAt(proto, pc);
    if (!handler) {
        guard.tryDepth -= depth;
        if (debug) debug->leaveTo(0);
        return -1;
    }
    if (!handler->finally) guard.placed = false; // caught: the next error is a new one
    guard.tryDepth += depthAt(proto, handler->target) - depth;
    if (debug) debug->leaveTo(static_cast<size_t>(handler->scopes));
    for (int i = handler->clearFrom; i < handler->clearTo; ++i) R[i] = Value();
    if (debug) markDeclared(*debug, handler->clearFrom, handler->clearTo, false);
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

// A declaration outside any function: a global, or in code compiled for a scope, a
// variable of that scope.
FOXLANG_APART void defineGlobal(const GlobalSite& site, Context& root, Context& scope, Value* value) {
    Context& owner = site.byName ? scope : root;
    if (!value) {
        if (owner.variables.count(site.name) || owner.programGlobals.count(site.name)) alreadyDeclared(site.name);
    } else {
        // `global int x = ...` for a variable of the program's top level sets that one,
        // as it always set the global of that name.
        auto program = owner.programGlobals.find(site.name);
        if (program != owner.programGlobals.end()) {
            *program->second = std::move(*value);
            return;
        }
        owner.variables[site.name] = std::move(*value);
        if (site.declaresConstant) owner.constants.insert(site.name);
        if (!site.nullable.empty()) owner.nullableGlobals[site.name] = site.nullable;
        else if (!owner.nullableGlobals.empty()) owner.nullableGlobals.erase(site.name);
    }
}

[[noreturn]] FOXLANG_COLD void constantChanged(const std::string& name) {
    throw std::runtime_error("Runtime Error: '" + name + "' is a constant and cannot be changed");
}

FOXLANG_APART void setGlobal(GlobalSite& site, Context& root, Context& scope, Value& value) {
    Value& target = *global(site, root, scope);
    Context& owner = site.byName ? scope : root;
    auto nullable = owner.nullableGlobals.find(site.name);
    if (nullable != owner.nullableGlobals.end() && globalVariable(owner, site.name) == &target) {
        runtime::coerce(nullable->second, value, "variable '" + site.name + "'");
        target = std::move(value);
        return;
    }
    runtime::assign(target, std::move(value), site.name);
}

FOXLANG_APART void assignSlow(Value& target, Value& value, const std::string& name) {
    runtime::assign(target, std::move(value), name);
}

FOXLANG_APART void concat(Value& out, const Value* parts, int count) {
    std::string text;
    for (int i = 0; i < count; ++i) runtime::appendDisplay(text, parts[i]);
    out = Value::string(std::move(text));
}

// One round of a for-in loop: false when there is nothing left.
FOXLANG_APART bool forIn(Value* R, const Instr& in) {
    const Value& container = R[in.a];
    Value& position = R[in.a + 1];
    Value& count = R[in.a + 2];
    size_t at = static_cast<size_t>(position.asInt());
    bool pair = in.x == 2;
    Value* first = R + in.b;
    switch (container.kind()) {
        case Value::Kind::Array: {
            const auto& items = container.ref()->items;
            if (at >= items.size()) return false;
            if (pair) {
                first[0].setInt(static_cast<long long>(at));
                first[1] = items[at];
            } else {
                first[0] = items[at];
            }
            break;
        }
        case Value::Kind::Map: {
            const Object& map = *container.ref();
            if (at >= map.keys.size()) return false;
            first[0] = Value::string(map.keys[at]);
            if (pair) first[1] = map.items[at];
            break;
        }
        case Value::Kind::String: {
            const std::string& text = container.str();
            if (at >= text.size()) return false;
            // One character: the bytes of one UTF-8 sequence.
            unsigned char lead = static_cast<unsigned char>(text[at]);
            size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
            length = std::min(length, text.size() - at);
            Value character = Value::string(text.substr(at, length));
            if (pair) {
                first[0].setInt(count.asInt());
                first[1] = std::move(character);
            } else {
                first[0] = std::move(character);
            }
            position.setInt(static_cast<long long>(at + length));
            count.setInt(count.asInt() + 1);
            return true;
        }
        default:
            throw std::runtime_error("Type Error: a for-in loop needs an array, a map or a string, got '" +
                                     container.typeName() + "'");
    }
    position.setInt(static_cast<long long>(at + 1));
    return true;
}

FOXLANG_APART void mapKey(Value& key) { key = Value::string(runtime::keyOf(key)); }

[[noreturn]] FOXLANG_APART void rethrow(std::exception_ptr& pending) {
    std::exception_ptr error = pending;
    pending = nullptr;
    std::rethrow_exception(error);
}

// GCC and Clang can jump to a label's address; the loop then dispatches each next
// instruction from where the last one ended. Elsewhere it is an ordinary switch.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wpedantic" // labels as values are the extension used here
#define FOXLANG_THREADED 1
#define OP(name) case Op::name: L_##name
#define NEXT                                                     \
    do {                                                         \
        in = ip++;                                               \
        goto *jump[static_cast<int>(in->op)];                    \
    } while (0)
#else
#define FOXLANG_THREADED 0
#define OP(name) case Op::name
#define NEXT break
#endif

Value execute(Proto& proto, Value* R, Context& root, Context& scope, DebugFrame* debug, Object* closure) {
    for (const auto& constant : proto.preload) R[constant.first] = proto.constants[static_cast<size_t>(constant.second)];
    const Instr* code = proto.code.data();
    const Instr* ip = code;
    const Value* K = proto.constants.data();
    std::unique_ptr<std::exception_ptr[]> pending;
    if (proto.pendingErrors > 0) pending.reset(new std::exception_ptr[static_cast<size_t>(proto.pendingErrors)]);
    runtime::StackGuard& guard = runtime::stackGuard();
    std::exception_ptr escaping;
    const Instr* in = nullptr;
#if FOXLANG_THREADED
    // Each instruction jumps straight to the code of the next one's operation, instead of
    // going back to one switch: fewer branches, and each one easier to predict.
    static void* const jump[] = {
        &&L_Move,
        &&L_LoadConst,
        &&L_Clear,
        &&L_GetGlobal,
        &&L_SetGlobal,
        &&L_DefineGlobal,
        &&L_Coerce,
        &&L_Assign,
        &&L_Zero,
        &&L_NewSized,
        &&L_Fail,
        &&L_Add,
        &&L_Sub,
        &&L_Mul,
        &&L_Div,
        &&L_Mod,
        &&L_Eq,
        &&L_Ne,
        &&L_Lt,
        &&L_Le,
        &&L_Gt,
        &&L_Ge,
        &&L_Negate,
        &&L_Not,
        &&L_Truth,
        &&L_Jump,
        &&L_JumpIfFalse,
        &&L_JumpIfTrue,
        &&L_JumpIfNull,
        &&L_JumpIfNotNull,
        &&L_Compare,
        &&L_ForIn,
        &&L_Call,
        &&L_Return,
        &&L_ReturnVoid,
        &&L_NewArray,
        &&L_NewMap,
        &&L_MapKey,
        &&L_Concat,
        &&L_Index,
        &&L_Field,
        &&L_SetPath,
        &&L_Increment,
        &&L_IncrementGlobal,
        &&L_Box,
        &&L_Unbox,
        &&L_BoxStore,
        &&L_BoxAssign,
        &&L_GetCapture,
        &&L_SetCapture,
        &&L_IncrementRef,
        &&L_Closure,
        &&L_CallValue,
        &&L_Method,
        &&L_Declare,
        &&L_Throw,
        &&L_Rethrow,
        &&L_TryEnter,
        &&L_TryLeave,
        &&L_Match,
        &&L_Statement,
        &&L_ScopeEnter,
        &&L_ScopeLeave,
        &&L_Declared,
    };
#endif

    for (;;) {
        try {
            for (;;) {
                in = ip++;
                switch (in->op) {
                    OP(Move):
                        if (in->x) R[in->a] = std::move(R[in->b]);
                        else R[in->a] = R[in->b];
                        NEXT;
                    OP(LoadConst):
                        R[in->a] = K[in->b];
                        NEXT;
                    OP(Clear):
                        for (int i = in->a; i < in->b; ++i) R[i] = Value();
                        if (debug) markDeclared(*debug, in->a, in->b, false);
                        NEXT;
                    OP(GetGlobal):
                        R[in->a] = *readGlobal(proto.globals[static_cast<size_t>(in->b)], root, scope);
                        NEXT;
                    OP(SetGlobal): {
                        GlobalSite& site = proto.globals[static_cast<size_t>(in->b)];
                        Value& target = *global(site, root, scope);
                        if (site.constant) constantChanged(site.name);
                        Value& value = R[in->a];
                        if (target.kind() == value.kind() && !target.is(Value::Kind::Struct)) target = std::move(value);
                        else setGlobal(site, root, scope, value);
                        NEXT;
                    }
                    OP(DefineGlobal):
                        defineGlobal(proto.globals[static_cast<size_t>(in->b)], root, scope, in->a < 0 ? nullptr : &R[in->a]);
                        NEXT;
                    OP(Coerce): {
                        const Conversion& conversion = proto.conversions[static_cast<size_t>(in->b)];
                        if (!runtime::storesAsIs(conversion.kind, R[in->a])) runtime::coerce(conversion.type, R[in->a], conversion.what);
                        NEXT;
                    }
                    OP(Assign): {
                        Value& target = R[in->a];
                        Value& value = R[in->b];
                        if (target.kind() == value.kind() && !target.is(Value::Kind::Struct)) target = std::move(value);
                        else assignSlow(target, value, K[in->c].str());
                        NEXT;
                    }
                    OP(Zero):
                        R[in->a] = runtime::zeroValue(K[in->b].str(), root);
                        NEXT;
                    OP(NewSized):
                        R[in->a] = sizedArray(R[in->b], K[in->c].str());
                        NEXT;
                    OP(Fail):
                        raise(K[in->a].str());

                    OP(Add): {
                        const Value& l = R[in->b];
                        const Value& r = R[in->c];
                        if (l.isInt() && r.isInt()) {
                            long long sum = l.asInt() + r.asInt();
                            if (fitsInt(sum)) {
                                R[in->a].setInt(sum);
                                NEXT;
                            }
                        }
                        binarySlow(runtime::Operator::Add, proto.texts[in->y], R[in->a], l, r);
                        NEXT;
                    }
                    OP(Sub): {
                        const Value& l = R[in->b];
                        const Value& r = R[in->c];
                        if (l.isInt() && r.isInt()) {
                            long long difference = l.asInt() - r.asInt();
                            if (fitsInt(difference)) {
                                R[in->a].setInt(difference);
                                NEXT;
                            }
                        }
                        binarySlow(runtime::Operator::Sub, proto.texts[in->y], R[in->a], l, r);
                        NEXT;
                    }
                    OP(Mul): {
                        const Value& l = R[in->b];
                        const Value& r = R[in->c];
                        if (l.isInt() && r.isInt()) {
                            long long product = l.asInt() * r.asInt();
                            if (fitsInt(l.asInt()) && fitsInt(r.asInt()) && fitsInt(product)) {
                                R[in->a].setInt(product);
                                NEXT;
                            }
                        }
                        binarySlow(runtime::Operator::Mul, proto.texts[in->y], R[in->a], l, r);
                        NEXT;
                    }
                    OP(Div):
                    OP(Mod): {
                        const Value& l = R[in->b];
                        const Value& r = R[in->c];
                        if (l.isInt() && r.isInt() && r.asInt() != 0 && fitsInt(l.asInt()) && fitsInt(r.asInt())) {
                            long long result = in->op == Op::Div ? l.asInt() / r.asInt() : l.asInt() % r.asInt();
                            if (fitsInt(result)) {
                                R[in->a].setInt(result);
                                NEXT;
                            }
                        }
                        binarySlow(in->op == Op::Div ? runtime::Operator::Div : runtime::Operator::Mod, proto.texts[in->y],
                                   R[in->a], l, r);
                        NEXT;
                    }
                    OP(Eq): OP(Ne): OP(Lt): OP(Le): OP(Gt): OP(Ge): {
                        const Value& l = R[in->b];
                        const Value& r = R[in->c];
                        auto op = static_cast<runtime::Operator>(static_cast<int>(runtime::Operator::Eq) +
                                                                 (static_cast<int>(in->op) - static_cast<int>(Op::Eq)));
                        if (l.isInt() && r.isInt()) {
                            long long a = l.asInt(), b = r.asInt();
                            bool result = op == runtime::Operator::Eq ? a == b : op == runtime::Operator::Ne ? a != b
                                        : op == runtime::Operator::Lt ? a < b : op == runtime::Operator::Le ? a <= b
                                        : op == runtime::Operator::Gt ? a > b : a >= b;
                            R[in->a].setBool(result);
                            NEXT;
                        }
                        binarySlow(op, proto.texts[in->y], R[in->a], l, r);
                        NEXT;
                    }
                    OP(Negate):
                    OP(Not):
                        unary(in->op, R[in->a], R[in->b]);
                        NEXT;
                    OP(Truth):
                        runtime::operandTruth(R[in->a], in->x ? "right" : "left", proto.texts[in->y]);
                        NEXT;

                    OP(Jump):
                        ip = code + in->a;
                        NEXT;
                    OP(JumpIfFalse):
                        if (!truth(R[in->a], in->x)) ip = code + in->b;
                        NEXT;
                    OP(JumpIfTrue):
                        if (truth(R[in->a], in->x)) ip = code + in->b;
                        NEXT;
                    OP(JumpIfNull):
                        if (R[in->a].isVoid()) ip = code + in->b;
                        NEXT;
                    OP(JumpIfNotNull):
                        if (!R[in->a].isVoid()) ip = code + in->b;
                        NEXT;
                    OP(Compare): {
                        const Value& l = R[in->a];
                        const Value& r = R[in->b];
                        auto op = static_cast<runtime::Operator>(in->x);
                        bool result;
                        if (l.isInt() && r.isInt()) {
                            long long a = l.asInt(), b = r.asInt();
                            switch (op) {
                                case runtime::Operator::Lt: result = a < b; break;
                                case runtime::Operator::Le: result = a <= b; break;
                                case runtime::Operator::Gt: result = a > b; break;
                                case runtime::Operator::Ge: result = a >= b; break;
                                case runtime::Operator::Eq: result = a == b; break;
                                default: result = a != b; break;
                            }
                        } else {
                            result = compareSlow(op, proto.texts[in->y >> 1], l, r);
                        }
                        if (result == ((in->y & 1) != 0)) ip = code + in->c;
                        NEXT;
                    }

                    OP(ForIn):
                        if (!forIn(R, *in)) ip = code + in->c;
                        NEXT;
                    OP(Call):
                        call(proto, *in, R, root);
                        NEXT;
                    OP(Return):
                        return std::move(R[in->a]);
                    OP(ReturnVoid):
                        return Value();

                    OP(NewArray):
                        newArray(R[in->a], R + in->b, in->c);
                        NEXT;
                    OP(NewMap):
                        newMap(R[in->a], R + in->b, in->c);
                        NEXT;
                    OP(Concat):
                        concat(R[in->a], R + in->b, in->c);
                        NEXT;
                    OP(MapKey):
                        if (!R[in->a].isString()) mapKey(R[in->a]);
                        NEXT;
                    OP(Index): {
                        // An array element by an int in range is the common case.
                        const Value& base = R[in->b];
                        const Value& key = R[in->c];
                        if (base.is(Value::Kind::Array) && key.isInt() && in->a != in->b) {
                            const auto& items = base.ref()->items;
                            long long at = key.asInt();
                            if (at >= 0 && static_cast<unsigned long long>(at) < items.size()) {
                                R[in->a] = items[static_cast<size_t>(at)];
                                NEXT;
                            }
                        }
                        index(R[in->a], R[in->b], R[in->c]);
                        NEXT;
                    }
                    OP(Field): {
                        FieldSite& site = proto.fields[static_cast<size_t>(in->c)];
                        const Value& base = R[in->b];
                        if (base.is(Value::Kind::Struct) && base.ref()->structType.get() == site.type && in->a != in->b) {
                            R[in->a] = base.ref()->items[site.index];
                            NEXT;
                        }
                        field(R[in->a], R[in->b], site);
                        NEXT;
                    }
                    OP(SetPath):
                        setPath(proto, proto.paths[static_cast<size_t>(in->b)], R, R[in->a], root, scope, closure);
                        NEXT;
                    OP(Increment): {
                        Value& target = R[in->b];
                        int delta = (in->c & 1) ? 1 : -1;
                        if (target.isInt() && fitsInt(target.asInt()) && fitsInt(target.asInt() + delta)) {
                            long long old = target.asInt();
                            target.setInt(old + delta);
                            if (in->a >= 0) R[in->a].setInt(old);
                            NEXT;
                        }
                        incrementSlow(in->a >= 0 ? &R[in->a] : nullptr, target, delta, K[in->c >> 1].str());
                        NEXT;
                    }
                    OP(IncrementGlobal): {
                        GlobalSite& site = proto.globals[static_cast<size_t>(in->b)];
                        Value& target = *global(site, root, scope);
                        if (site.constant) constantChanged(site.name);
                        int delta = in->c ? 1 : -1;
                        if (target.isInt() && fitsInt(target.asInt() + delta)) {
                            long long old = target.asInt();
                            target.setInt(old + delta);
                            if (in->a >= 0) R[in->a].setInt(old);
                            NEXT;
                        }
                        incrementSlow(in->a >= 0 ? &R[in->a] : nullptr, target, delta, site.name);
                        NEXT;
                    }

                    OP(Box):
                        box(R[in->a]);
                        NEXT;
                    OP(Unbox):
                        R[in->a] = R[in->b].ref()->items[0];
                        NEXT;
                    OP(BoxStore):
                        R[in->a].ref()->items[0] = std::move(R[in->b]);
                        NEXT;
                    OP(BoxAssign): {
                        Value& target = R[in->a].ref()->items[0];
                        Value& value = R[in->b];
                        if (target.kind() == value.kind() && !target.is(Value::Kind::Struct)) target = std::move(value);
                        else assignSlow(target, value, K[in->c].str());
                        NEXT;
                    }
                    OP(GetCapture):
                        R[in->a] = closure->items[static_cast<size_t>(in->b)].ref()->items[0];
                        NEXT;
                    OP(SetCapture): {
                        Value& target = closure->items[static_cast<size_t>(in->a)].ref()->items[0];
                        Value& value = R[in->b];
                        if (in->x || (target.kind() == value.kind() && !target.is(Value::Kind::Struct))) target = std::move(value);
                        else assignSlow(target, value, K[in->c].str());
                        NEXT;
                    }
                    OP(IncrementRef): {
                        Value& target = in->x ? closure->items[static_cast<size_t>(in->b)].ref()->items[0] : R[in->b].ref()->items[0];
                        incrementSlow(in->a >= 0 ? &R[in->a] : nullptr, target, (in->c & 1) ? 1 : -1, K[in->c >> 1].str());
                        NEXT;
                    }
                    OP(Closure):
                        makeClosure(proto, in->b, R[in->a], R, closure);
                        NEXT;
                    OP(CallValue):
                        callValueInPlace(R, in->a, in->c, root, K[in->b].str());
                        NEXT;
                    OP(Method):
                        callMethod(R, in->a, in->c, root, K[in->b].str());
                        NEXT;
                    OP(Declare): {
                        Declaration* declaration = proto.declarations[static_cast<size_t>(in->b)];
                        if (!in->c || !declaration->exists(root)) declaration->declare(root);
                        NEXT;
                    }
                    OP(Throw):
                        raise(runtime::display(R[in->a]));
                    OP(Rethrow):
                        rethrow(pending[static_cast<size_t>(in->a)]);
                    OP(TryEnter):
                        ++guard.tryDepth;
                        NEXT;
                    OP(TryLeave):
                        --guard.tryDepth;
                        NEXT;
                    OP(Match):
                        R[in->a].setBool(runtime::switchMatches(R[in->b], R[in->c]));
                        NEXT;

                    OP(Statement):
                        guard.line = in->a;
                        guard.file = proto.file;
                        if (debug) debug->hook->statement(proto.file, in->a);
                        NEXT;
                    OP(ScopeEnter):
                        if (debug) debug->enter(in->a != 0);
                        NEXT;
                    OP(ScopeLeave):
                        if (debug) debug->leave();
                        NEXT;
                    OP(Declared):
                        if (in->b) declareProgramGlobal(proto.globals[static_cast<size_t>(in->b - 1)], R[in->a], root);
                        if (debug) markDeclared(*debug, in->a, in->a + 1, true);
                        NEXT;
                }
            }
        } catch (...) {
            int target = recover(proto, static_cast<int>(ip - code) - 1, R, debug, pending.get());
            if (target >= 0) {
                ip = code + target;
                continue;
            }
            escaping = std::current_exception();
        }
        // Raised again only now, outside the catch block, once this frame's handling is over.
        std::rethrow_exception(escaping);
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
        execute(*proto, window.base(), root, root, nullptr);
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
            // The top level's variables outlive its registers as ordinary globals, for
            // code that runs later: another runSource, a callback, Interpreter::getGlobal.
            for (const auto& variable : root.programGlobals) root.variables.emplace(variable.first, *variable.second);
            if (!root.programGlobals.empty()) {
                root.programGlobals.clear();
                ++root.generation; // lookups cached the registers
            }
            root.slots = slots;
            root.slotNames = names;
            root.frame = frame;
        }
    } restore{root, root.slots, root.slotNames, root.frame};
    root.slots = window.base();
    root.slotNames = proto->slotNames.get();
    root.frame = true;
    if (!hook) {
        execute(*proto, window.base(), root, root, nullptr);
        return;
    }
    std::vector<char> outerDeclared(static_cast<size_t>(proto->slots), 0);
    root.declared.swap(outerDeclared);
    DebugFrame frame{hook, &root, {}, {}};
    struct Leave {
        DebugFrame& frame;
        std::vector<char>& outer;
        ~Leave() {
            frame.leaveTo(0);
            frame.function->declared.swap(outer);
        }
    } leave{frame, outerDeclared};
    execute(*proto, window.base(), root, root, &frame);
}

Value run(Proto& proto, Context& scope) {
    Window window(static_cast<size_t>(proto.registers));
    return execute(proto, window.base(), *scope.getRoot(), scope, nullptr);
}

Value evaluate(Node& expression, Context& scope) {
    return run(*compileExpression(expression), scope);
}

void execute(BlockNode& statements, Context& scope, const std::string& outside) {
    run(*compileStatements(statements, outside), scope);
}

} // namespace foxlang::vm

namespace foxlang::runtime {

Value callValue(const Value& function, Arguments args, Context& ctx) {
    std::vector<Value> values(args.begin(), args.end());
    return vm::callFunctionValue(function, values.data(), values.size(), *ctx.getRoot(), "function");
}

Value callValue(const Value& function, Value* args, size_t count, Context& ctx) {
    // A call from outside any FoxLang code (an HTTP handler) starts with no error placed.
    if (stackGuard().depth == 0) stackGuard().placed = false;
    return vm::callFunctionValue(function, args, count, *ctx.getRoot(), "function");
}

} // namespace foxlang::runtime
