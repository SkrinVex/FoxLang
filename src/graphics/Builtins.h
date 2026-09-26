#pragma once
#include "foxlang/Context.h"

namespace foxlang::runtime { class Call; }

namespace foxlang::graphics {
struct Signature {
    std::string builtin, name, result;
    std::vector<FuncParam> params;
    std::string documentation;
    Value (*handler)(runtime::Call&);
};
const std::vector<Signature>& signatures();
} // namespace foxlang::graphics
