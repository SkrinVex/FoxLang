#pragma once
#include <string>
#include <vector>
#include "foxlang/Context.h"

namespace foxlang {

// The one description of every builtin function. The runtime checks calls against
// it, and the semantic analyzer, the language server and the editor grammars are
// generated from it, so a builtin cannot exist in one place and be missing in another.
struct BuiltinSpec {
    std::string name;
    std::string result;             // int, float, string, bool, void, array, number or any
    std::vector<FuncParam> params;  // types: the result types above, number = int or float
    size_t required = 0;            // parameters after this many are optional
    bool variadic = false;          // the last parameter repeats
    std::string module;             // std module with the friendly wrapper, empty for core functions
    std::string documentation;      // Markdown, Russian, shown by the language server

    size_t maxArguments() const { return variadic ? static_cast<size_t>(-1) : params.size(); }
    bool acceptsCount(size_t count) const { return count >= required && count <= maxArguments(); }
    // name(type a, type b = optional) -> result, as editors display it.
    std::string signature() const;
};

const std::vector<BuiltinSpec>& builtinCatalog();
const BuiltinSpec* findBuiltinSpec(const std::string& name);

} // namespace foxlang
