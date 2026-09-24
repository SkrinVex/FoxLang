#pragma once
#include "foxlang/Context.h"

namespace foxlang::graphics {
struct Signature {
    std::string builtin, name, result;
    std::vector<FuncParam> params;
    std::string documentation;
};
const std::vector<Signature>& signatures();
Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& context);
} // namespace foxlang::graphics
