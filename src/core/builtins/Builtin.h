#pragma once
#include "foxlang/Builtins.h"
#include "foxlang/Runtime.h"
#include <forward_list>
#include <string>
#include <vector>

namespace foxlang::runtime {

// Arguments of one builtin call. The registry has already checked their count and
// types against the catalog, so accessors only convert and range-check.
class Call {
public:
    Call(const BuiltinSpec& spec, Arguments args, Context& ctx)
        : spec(spec), args(args), ctx(ctx) {}

    const BuiltinSpec& spec;
    const Arguments args;
    Context& ctx;

    size_t count() const { return args.size(); }
    bool has(size_t index) const { return index < args.size(); }
    const Value& at(size_t index) const { return args[index]; }
    std::string what(size_t index) const;

    long long integer(size_t index) const;
    double number(size_t index) const;
    // A string argument as it is; any other value as its text.
    const std::string& text(size_t index) const {
        const Value& value = args[index];
        if (value.isString()) return value.str();
        return scratch_.emplace_front(value.text());
    }
    bool flag(size_t index) const { return args[index].isBool() && args[index].asBool(); }
    std::vector<Value>& array(size_t index) const {
        if (args[index].is(Value::Kind::Array)) return args[index].ref()->items;
        return ctx.arrayOf(args[index], what(index)); // reports the wrong type
    }
    // A non-negative int that must not exceed limit, for sizes and counts.
    size_t amount(size_t index, size_t limit) const;

private:
    mutable std::forward_list<std::string> scratch_; // texts of non-string arguments, alive for the call
};

using Handler = Value (*)(Call&);
// The common case of a builtin without the checks every call goes through: false when
// the arguments are not that case (and nothing was changed), and the call takes the
// full way. The arguments may be the caller's variables themselves: they are read,
// never moved from. `result` is none of them.
using FastPath = bool (*)(Value* const* args, size_t count, Value& result);

struct Builtin {
    Builtin(BuiltinSpec s, Handler h) : spec(std::move(s)), handler(h) {}
    BuiltinSpec spec;
    Handler handler = nullptr;
    FastPath fast = nullptr;
    // What each parameter accepts, worked out from its type name when registered.
    struct Accepts {
        enum class Rule : unsigned char { Any, Number, Kind, Named } rule = Rule::Any;
        Value::Kind kind = Value::Kind::Void;
    };
    std::vector<Accepts> accepts;
};

inline Value nothing() { return Value(); }
inline Value boolean(bool value) { return Value::boolean(value); }
inline Value integer(long long value) { return intResult(value, "result"); }
inline Value real(double value) { return realResult(value); }
inline Value text(std::string value) { return Value::string(std::move(value)); }

// Each area contributes its part of the catalog.
void addCoreBuiltins(std::vector<Builtin>& out);
void addTextBuiltins(std::vector<Builtin>& out);
void addCollectionBuiltins(std::vector<Builtin>& out);
void addSystemBuiltins(std::vector<Builtin>& out);
void addFileBuiltins(std::vector<Builtin>& out);
void addNetworkBuiltins(std::vector<Builtin>& out);
void addGraphicsBuiltins(std::vector<Builtin>& out);
void addSoundBuiltins(std::vector<Builtin>& out);

} // namespace foxlang::runtime
