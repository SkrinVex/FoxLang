#pragma once
#include "foxlang/Builtins.h"
#include "foxlang/Runtime.h"
#include <string>
#include <vector>

namespace foxlang::runtime {

// Arguments of one builtin call. The registry has already checked their count and
// types against the catalog, so accessors only convert and range-check.
class Call {
public:
    Call(const BuiltinSpec& spec, const std::vector<Value>& args, Context& ctx)
        : spec(spec), args(args), ctx(ctx) {}

    const BuiltinSpec& spec;
    const std::vector<Value>& args;
    Context& ctx;

    size_t count() const { return args.size(); }
    bool has(size_t index) const { return index < args.size(); }
    const Value& at(size_t index) const { return args[index]; }
    std::string what(size_t index) const;

    long long integer(size_t index) const;
    double number(size_t index) const;
    const std::string& text(size_t index) const { return args[index].value.str(); }
    bool flag(size_t index) const { return args[index].value == "true"; }
    std::vector<Value>& array(size_t index) const { return ctx.arrayOf(args[index], what(index)); }
    // A non-negative int that must not exceed limit, for sizes and counts.
    size_t amount(size_t index, size_t limit) const;
};

using Handler = Value (*)(Call&);

struct Builtin {
    BuiltinSpec spec;
    Handler handler = nullptr;
};

inline Value nothing() { return {"void", ""}; }
inline Value boolean(bool value) { return {"bool", value ? "true" : "false"}; }
inline Value integer(long long value) { return {"int", intResult(value, "result")}; }
inline Value real(double value) { return {"float", realResult(value)}; }
inline Value text(std::string value) { return {"string", std::move(value)}; }

// Each area contributes its part of the catalog.
void addCoreBuiltins(std::vector<Builtin>& out);
void addTextBuiltins(std::vector<Builtin>& out);
void addCollectionBuiltins(std::vector<Builtin>& out);
void addSystemBuiltins(std::vector<Builtin>& out);
void addNetworkBuiltins(std::vector<Builtin>& out);
void addGraphicsBuiltins(std::vector<Builtin>& out);

} // namespace foxlang::runtime
