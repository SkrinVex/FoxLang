#pragma once
#include "skrumi/Context.h"

namespace skrumi::runtime { class Call; }

namespace skrumi::graphics {
struct Signature {
    std::string builtin, name, result;
    std::vector<FuncParam> params;
    std::string documentation;
    Value (*handler)(runtime::Call&);
};
const std::vector<Signature>& signatures();
} // namespace skrumi::graphics
