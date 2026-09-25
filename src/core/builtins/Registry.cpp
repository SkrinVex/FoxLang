#include "Builtin.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace foxlang {

std::string BuiltinSpec::signature() const {
    std::string out = name + "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) out += ", ";
        bool optional = i >= required && !(variadic && i + 1 == params.size());
        if (optional) out += "[";
        out += params[i].type + " " + params[i].name;
        if (variadic && i + 1 == params.size()) out += "...";
        if (optional) out += "]";
    }
    return out + ") -> " + result;
}

namespace runtime {
namespace {

struct Registry {
    std::vector<Builtin> builtins;
    std::vector<BuiltinSpec> specs;
    std::unordered_map<std::string, size_t> index;

    Registry() {
        addCoreBuiltins(builtins);
        addTextBuiltins(builtins);
        addCollectionBuiltins(builtins);
        addSystemBuiltins(builtins);
        addFileBuiltins(builtins);
        addNetworkBuiltins(builtins);
        addGraphicsBuiltins(builtins);
        addSoundBuiltins(builtins);
        for (size_t i = 0; i < builtins.size(); ++i) {
            const auto& spec = builtins[i].spec;
            if (!index.emplace(spec.name, i).second)
                throw std::logic_error("builtin '" + spec.name + "' is registered twice");
            specs.push_back(spec);
        }
    }
};

const Registry& registry() {
    static const Registry instance;
    return instance;
}

bool typeFits(const std::string& expected, const Value& value) {
    if (expected == "any") return value.type != "void";
    if (expected == "int" || expected == "float" || expected == "number")
        return value.type == "int" || value.type == "float";
    return value.type == expected;
}

std::string countText(const BuiltinSpec& spec) {
    if (spec.variadic) return "at least " + std::to_string(spec.required);
    if (spec.required == spec.params.size()) return std::to_string(spec.required);
    return std::to_string(spec.required) + ".." + std::to_string(spec.params.size());
}

const FuncParam& parameterFor(const BuiltinSpec& spec, size_t index) {
    return spec.params[std::min(index, spec.params.size() - 1)];
}

} // namespace

std::string Call::what(size_t index) const {
    return "argument '" + parameterFor(spec, index).name + "' of " + spec.name + "()";
}

long long Call::integer(size_t index) const {
    long long result = 0;
    if (tryInt(args[index], result)) return result;
    return toInt(args[index], what(index));
}

double Call::number(size_t index) const {
    double result = 0;
    if (tryNumber(args[index], result)) return result;
    return toNumber(args[index], what(index));
}

size_t Call::amount(size_t index, size_t limit) const {
    long long value = integer(index);
    if (value < 0) throw std::runtime_error("Runtime Error: " + what(index) + " cannot be negative: " + std::to_string(value));
    if (static_cast<unsigned long long>(value) > limit)
        throw std::runtime_error("Runtime Error: " + what(index) + " is too large: " + std::to_string(value));
    return static_cast<size_t>(value);
}

const Builtin* findBuiltin(const std::string& name) {
    const auto& reg = registry();
    auto found = reg.index.find(name);
    return found == reg.index.end() ? nullptr : &reg.builtins[found->second];
}

const BuiltinSpec& specOf(const Builtin& builtin) { return builtin.spec; }

bool acceptsArguments(const Builtin& builtin, const std::vector<Value>& args) {
    const auto& spec = builtin.spec;
    if (!spec.acceptsCount(args.size())) return false;
    for (size_t i = 0; i < args.size(); ++i)
        if (!typeFits(parameterFor(spec, i).type, args[i])) return false;
    return true;
}

Value invoke(const Builtin& builtin, const std::vector<Value>& args, Context& ctx) {
    const auto& spec = builtin.spec;
    if (!spec.acceptsCount(args.size()))
        throw std::runtime_error("Runtime Error: " + spec.name + "() expects " + countText(spec) +
                                 " arguments, got " + std::to_string(args.size()));
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& param = parameterFor(spec, i);
        if (!typeFits(param.type, args[i]))
            throw std::runtime_error("Type Error: argument '" + param.name + "' of " + spec.name + "() must be " +
                                     (param.type == "number" ? std::string("int or float") : param.type) +
                                     ", got '" + args[i].type + "'");
    }
    Call call(spec, args, ctx);
    return builtin.handler(call);
}

bool isBuiltin(const std::string& name) { return findBuiltin(name) != nullptr; }

Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& ctx) {
    const Builtin* builtin = findBuiltin(name);
    if (!builtin) throw std::runtime_error("Runtime Error: Unknown builtin function '" + name + "'");
    return invoke(*builtin, args, ctx);
}

} // namespace runtime

const std::vector<BuiltinSpec>& builtinCatalog() { return runtime::registry().specs; }

const BuiltinSpec* findBuiltinSpec(const std::string& name) {
    const auto* builtin = runtime::findBuiltin(name);
    return builtin ? &builtin->spec : nullptr;
}

} // namespace foxlang
