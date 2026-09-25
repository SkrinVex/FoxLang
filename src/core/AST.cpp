#include "foxlang/AST.h"
#include "foxlang/Bytecode.h"
#include <cerrno>
#include <cstdlib>

namespace foxlang {

void CallDepth::exceeded(const std::string& name, int depth) {
    throw std::runtime_error("Runtime Error: call depth limit reached in '" + name + "' after " + std::to_string(depth) +
                             " nested calls (recursion without a base case?)");
}

FuncDefNode::FuncDefNode(std::string rt, std::string n, std::vector<FuncParam> p, std::shared_ptr<Node> b, SourceRange nr)
    : returnType(std::move(rt)), name(std::move(n)), params(std::move(p)), body(std::move(b)), nameRange(nr) {
    block_ = dynamic_cast<BlockNode*>(body.get());
}

void FuncDefNode::declare(Context& root) {
    root.getRoot()->defineFunc(name, std::make_shared<FuncDefNode>(returnType, name, params, body, nameRange));
}

Value FuncDefNode::invoke(std::vector<Value> args, Context& caller) const {
    return vm::call(*this, args.data(), args.size(), caller);
}

NumberNode::NumberNode(std::string v) : val(std::move(v)) {
    isFloat = val.find('.') != std::string::npos;
    if (isFloat) {
        literal = Value::real(std::strtod(val.c_str(), nullptr));
    } else {
        errno = 0;
        long long parsed = std::strtoll(val.c_str(), nullptr, 10);
        // Digits that do not fit even 64 bits are an error wherever the literal is used.
        tooBig = errno == ERANGE;
        literal = Value::integer(parsed);
    }
}

void StructDefNode::declare(Context& root) {
    root.getRoot()->structs[type->name] = type;
}

} // namespace foxlang
