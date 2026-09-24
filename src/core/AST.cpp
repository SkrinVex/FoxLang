#include "foxlang/AST.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace foxlang {

namespace {

// Arrays leave a function by copy: the scope that owns the original is gone by the
// time the caller sees the value, so the copy is handed to the caller to own.
Value detachArray(const Value& value, Context& ctx) {
    if (value.type != "array") return value;
    auto& arrays = ctx.getRoot()->arrays;
    auto source = arrays.find(value.value.str());
    if (source == arrays.end()) throw std::runtime_error("Runtime Error: returned array no longer exists");
    static unsigned long long counter = 0;
    std::string id = "__ret_" + std::to_string(counter++);
    arrays[id] = source->second;
    return {"array", id};
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

    // Lexical scope: a function sees globals, never the caller's locals.
    Context scope;
    scope.parent = caller.getRoot();
    scope.interpreter = caller.interpreter;
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        if (param.type == "array") {
            caller.arrayOf(args[i], "parameter '" + param.name + "' of '" + name + "'");
        } else {
            runtime::coerce(param.type, args[i], "parameter '" + param.name + "' of '" + name + "'");
        }
        scope.defineVar(param.name, param.type, args[i]);
    }

    CallDepth guard(name);
    Value result{"void", ""};
    try {
        body->eval(scope);
    } catch (ReturnValue& ret) {
        result = std::move(ret.value);
    } catch (const BreakException&) {
        throw std::runtime_error("Runtime Error: 'break' outside of loop in function '" + name + "'");
    } catch (const ContinueException&) {
        throw std::runtime_error("Runtime Error: 'continue' outside of loop in function '" + name + "'");
    }

    if (result.type == "array" && returnType != "array") caller.getRoot()->arrays.erase(result.value.str());
    if (returnType == "void") return {"void", ""};
    if (result.type == "void")
        throw std::runtime_error("Runtime Error: function '" + name + "' must return " + returnType + " but ended without a value");
    if (returnType == "array") {
        if (result.type != "array")
            throw std::runtime_error("Type Error: function '" + name + "' must return array, got '" + result.type + "'");
        // The detached copy becomes a temporary of the scope that made the call.
        caller.ownedArrays.push_back(result.value.str());
        return result;
    }
    runtime::coerce(returnType, result, "return value of '" + name + "'");
    return result;
}

Value ReturnNode::eval(Context& ctx) {
    Value result = expr ? expr->eval(ctx) : Value{"void", ""};
    throw ReturnValue{detachArray(result, ctx)};
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
    if (!function) throw std::runtime_error("Runtime Error: Function '" + name + "' not found!");
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
    Value val = expr->eval(ctx);
    runtime::coerce(type, val, std::string(global ? "global variable '" : "variable '") + name + "'");
    scope.defineVar(name, type, val);
    return {"void", ""};
}

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
    if ((lval.type == "array" || rval.type == "array") && !equality)
        throw std::runtime_error("Type Error: operator '" + op + "' cannot be applied to an array");

    if (op == "+" || op == "+=") {
        if (lval.type == "string" || rval.type == "string") return {"string", lval.value + rval.value};
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
        } else if (lval.type == "array" || rval.type == "array") {
            // Arrays compare by identity: the same array, not equal contents.
            result = decide(lval.type + ":" + lval.value.str(), rval.type + ":" + rval.value.str());
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
    if (initializer) {
        Value source = initializer->eval(ctx);
        std::vector<Value> items = ctx.takeArray(source, "initializer of array '" + name + "'");
        scope.declareArray(name, 0);
        scope.getRoot()->arrays[scope.getVar(name).value.str()] = std::move(items);
        return {"void", ""};
    }
    long long size = sizeNode ? intArg(sizeNode->eval(ctx), "size of array", name) : 0;
    if (size < 0) throw std::runtime_error("Runtime Error: Array size cannot be negative");
    scope.declareArray(name, static_cast<size_t>(size));
    return {"void", ""};
}

Value ArrayLiteralNode::eval(Context& ctx) {
    std::vector<Value> items;
    items.reserve(elements.size());
    for (auto& element : elements) items.push_back(element->eval(ctx));
    return {"array", ctx.newArray(std::move(items))};
}

namespace {
size_t elementIndex(Context& ctx, const std::string& name, Node& indexNode, std::vector<Value>*& items) {
    items = &ctx.arrayOf(ctx.getVar(name), "'" + name + "'");
    long long index = intArg(indexNode.eval(ctx), "index of array", name);
    if (index < 0 || static_cast<unsigned long long>(index) >= items->size())
        throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(index) +
                                 " (size of '" + name + "' is " + std::to_string(items->size()) + ")");
    return static_cast<size_t>(index);
}
} // namespace

Value ArraySetNode::eval(Context& ctx) {
    std::vector<Value>* items = nullptr;
    size_t at = elementIndex(ctx, name, *index, items);
    Value assigned = value->eval(ctx);
    // Evaluating the value may have resized or replaced the array, so look it up again.
    items = &ctx.arrayOf(ctx.getVar(name), "'" + name + "'");
    if (at >= items->size()) throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(at));
    if (op != "=") {
        struct Literal : Node {
            Value held;
            explicit Literal(Value v) : held(std::move(v)) {}
            Value eval(Context&) override { return held; }
        };
        BinOpNode combine(op, std::make_unique<Literal>((*items)[at]), std::make_unique<Literal>(assigned));
        assigned = combine.eval(ctx);
    }
    (*items)[at] = std::move(assigned);
    return {"void", ""};
}

Value ArrayGetNode::eval(Context& ctx) {
    std::vector<Value>* items = nullptr;
    size_t at = elementIndex(ctx, name, *index, items);
    return (*items)[at];
}

Value BlockNode::eval(Context& ctx) {
    Context inner;
    if (scoped) {
        inner.parent = &ctx;
        inner.interpreter = ctx.interpreter;
    }
    Context& scope = scoped ? inner : ctx;
    runtime::StackGuard& guard = runtime::stackGuard();
    for (auto& stmt : stmts) {
        if (!stmt) continue;
        // Remembering the line costs one store. Catching here to rethrow with the
        // line attached cost an exception per nested block, and an error leaving a
        // deep recursion had to pass through every one of them.
        if (stmt->range.start.line > 0) guard.line = stmt->range.start.line;
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
                matched = numeric ? l == r : (switchValue.type == caseValue.type && switchValue.value == caseValue.value);
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
