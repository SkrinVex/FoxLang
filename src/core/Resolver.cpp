// Gives every local variable a numbered slot before the code first runs, so reading
// and writing it is an index into the function's frame instead of a search by name
// through nested scopes. Names that are not local are globals; code that was never
// resolved (the debugger's console) still looks names up by name.
#include "foxlang/Resolver.h"
#include "foxlang/AST.h"
#include <unordered_map>

namespace foxlang {
namespace {

class Resolver {
public:
    explicit Resolver(FrameLayout& layout) : layout_(layout) {}

    void push() { scopes_.emplace_back(); }
    void pop() { scopes_.pop_back(); }

    int declare(const std::string& name, bool* duplicate = nullptr) {
        auto& scope = scopes_.back();
        if (duplicate && scope.count(name)) *duplicate = true;
        int slot = static_cast<int>(layout_.names.size());
        layout_.names.push_back(name);
        scope[name] = slot;
        return slot;
    }

    int lookup(const std::string& name) const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            auto found = scope->find(name);
            if (found != scope->end()) return found->second;
        }
        return VarRef::global;
    }

    int size() const { return static_cast<int>(layout_.names.size()); }
    bool topLevel() const { return scopes_.empty(); }

    void visit(Node* node);

private:
    FrameLayout& layout_;
    std::vector<std::unordered_map<std::string, int>> scopes_;

    void block(BlockNode& block) {
        if (block.scoped) push();
        block.resolved = block.scoped;
        block.firstSlot = size();
        for (auto& stmt : block.stmts) visit(stmt.get());
        block.endSlot = size();
        if (block.scoped) pop();
    }
};

void Resolver::visit(Node* node) {
    if (!node) return;
    if (auto* n = dynamic_cast<BlockNode*>(node)) {
        block(*n);
    } else if (auto* n = dynamic_cast<VarAccessNode*>(node)) {
        n->ref.slot = lookup(n->name);
    } else if (auto* n = dynamic_cast<VarAssignNode*>(node)) {
        visit(n->expr.get());
        n->ref.slot = lookup(n->name);
    } else if (auto* n = dynamic_cast<PostIncNode*>(node)) {
        n->ref.slot = lookup(n->name);
    } else if (auto* n = dynamic_cast<VarDeclNode*>(node)) {
        // The initializer sees the names before this one: `int x = x + 1;` reads an outer x.
        visit(n->expr.get());
        if (!n->global && !topLevel()) n->slot = declare(n->name, &n->duplicate);
    } else if (auto* n = dynamic_cast<ArrayDeclNode*>(node)) {
        visit(n->sizeNode.get());
        visit(n->initializer.get());
        if (!n->global && !topLevel()) n->slot = declare(n->name, &n->duplicate);
    } else if (auto* n = dynamic_cast<ForNode*>(node)) {
        push();
        n->firstSlot = size();
        visit(n->init.get());
        visit(n->condition.get());
        visit(n->step.get());
        visit(n->body.get());
        n->endSlot = size();
        pop();
    } else if (auto* n = dynamic_cast<WhileNode*>(node)) {
        visit(n->condition.get());
        visit(n->body.get());
    } else if (auto* n = dynamic_cast<IfNode*>(node)) {
        visit(n->condition.get());
        visit(n->thenB.get());
        visit(n->elseB.get());
    } else if (auto* n = dynamic_cast<SwitchNode*>(node)) {
        visit(n->expr.get());
        for (auto& item : n->cases) {
            visit(item.first.get());
            visit(item.second.get());
        }
        visit(n->defaultCase.get());
    } else if (auto* n = dynamic_cast<TryNode*>(node)) {
        visit(n->body.get());
        if (n->handler) {
            push();
            if (!n->errorName.empty()) n->errorSlot = declare(n->errorName);
            visit(n->handler.get());
            pop();
        }
        visit(n->cleanup.get());
    } else if (auto* n = dynamic_cast<FuncCallNode*>(node)) {
        for (auto& arg : n->args) visit(arg.get());
    } else if (auto* n = dynamic_cast<ReturnNode*>(node)) {
        visit(n->expr.get());
    } else if (auto* n = dynamic_cast<BinOpNode*>(node)) {
        visit(n->left.get());
        visit(n->right.get());
    } else if (auto* n = dynamic_cast<UnaryOpNode*>(node)) {
        visit(n->operand.get());
    } else if (auto* n = dynamic_cast<ArrayLiteralNode*>(node)) {
        for (auto& element : n->elements) visit(element.get());
    } else if (auto* n = dynamic_cast<IndexNode*>(node)) {
        visit(n->base.get());
        visit(n->index.get());
    } else if (auto* n = dynamic_cast<FieldNode*>(node)) {
        visit(n->base.get());
    } else if (auto* n = dynamic_cast<SetNode*>(node)) {
        visit(n->value.get());
        visit(n->target.get());
    } else if (auto* n = dynamic_cast<MapLiteralNode*>(node)) {
        for (auto& entry : n->entries) {
            visit(entry.first.get());
            visit(entry.second.get());
        }
    } else if (auto* n = dynamic_cast<ThrowNode*>(node)) {
        visit(n->message.get());
    }
    // A function defined inside is resolved when it is first called; struct defaults,
    // literals, using and include have no names of their own to resolve.
}

} // namespace

void resolveFunction(FuncDefNode& function) {
    auto* body = dynamic_cast<BlockNode*>(function.body.get());
    if (!body || body->layout) return;
    auto layout = std::make_shared<FrameLayout>();
    Resolver resolver(*layout);
    resolver.push();
    for (const auto& param : function.params) resolver.declare(param.name);
    resolver.visit(body);
    resolver.pop();
    body->layout = layout;
}

void resolveProgram(BlockNode& program) {
    if (program.layout) return;
    auto layout = std::make_shared<FrameLayout>();
    Resolver resolver(*layout);
    // The top level declares globals; only blocks inside it get slots.
    resolver.visit(&program);
    program.layout = layout;
}

} // namespace foxlang
