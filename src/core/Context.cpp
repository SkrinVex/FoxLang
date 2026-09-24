#include "foxlang/Context.h"
#include "foxlang/AST.h"
#include "foxlang/Runtime.h"
#include <algorithm>
#include <atomic>
#include <ostream>
#include <string>
#include <stdexcept>

namespace foxlang {

Text Text::integer(long long value) {
    Text result;
    result.kind_ = Kind::Integer;
    result.integer_ = value;
    result.ready_ = false;
    return result;
}

Text Text::real(double value) {
    Text result;
    result.kind_ = Kind::Real;
    result.real_ = value;
    result.ready_ = false;
    return result;
}

void Text::materialize() const {
    text_ = kind_ == Kind::Integer ? std::to_string(integer_) : runtime::formatNumber(real_);
    ready_ = true;
}

std::ostream& operator<<(std::ostream& out, const Text& text) { return out << text.str(); }

Context::~Context() { releaseArrays(); }

// Re-declaring an array in the same scope frees the previous buffer at once
// instead of stranding it until the scope ends.
std::string Context::declareArray(const std::string& name, size_t size) {
    static std::atomic<unsigned long long> counter{0};
    Context* root = getRoot();
    auto existing = variables.find(name);
    if (existing != variables.end() && existing->second.type == "array") {
        auto owned = std::find(ownedArrays.begin(), ownedArrays.end(), existing->second.value);
        if (owned != ownedArrays.end()) {
            root->arrays.erase(*owned);
            ownedArrays.erase(owned);
        }
    }
    std::string id = "__arr_" + std::to_string(counter++);
    root->arrays[id] = std::vector<Value>(size, {"int", Text::integer(0)});
    ownedArrays.push_back(id);
    defineVar(name, "array", {"array", id});
    return id;
}

void Context::releaseArrays() {
    if (ownedArrays.empty()) return;
    Context* root = getRoot();
    for (const auto& id : ownedArrays) root->arrays.erase(id);
    ownedArrays.clear();
}

Value Context::getVar(const std::string& name) const {
    for (const Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it != scope->variables.end()) return it->second;
    }
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

std::string Context::newArray(std::vector<Value> items) {
    static std::atomic<unsigned long long> counter{0};
    std::string id = "__tmp_" + std::to_string(counter++);
    getRoot()->arrays[id] = std::move(items);
    ownedArrays.push_back(id);
    return id;
}

std::vector<Value>& Context::arrayOf(const Value& value, const std::string& what) {
    if (value.type != "array")
        throw std::runtime_error("Type Error: " + what + " must be an array, got '" + value.type + "'");
    auto& arrays = getRoot()->arrays;
    auto found = arrays.find(value.value.str());
    if (found == arrays.end())
        throw std::runtime_error("Runtime Error: " + what + " refers to an array that no longer exists");
    return found->second;
}

std::vector<Value> Context::takeArray(const Value& value, const std::string& what) {
    std::vector<Value>& items = arrayOf(value, what);
    const std::string& id = value.value.str();
    bool temporary = id.rfind("__tmp_", 0) == 0 || id.rfind("__ret_", 0) == 0;
    auto owned = std::find(ownedArrays.begin(), ownedArrays.end(), id);
    if (!temporary || owned == ownedArrays.end()) return items;
    std::vector<Value> moved = std::move(items);
    getRoot()->arrays.erase(id);
    ownedArrays.erase(owned);
    return moved;
}

Context* Context::getRoot() {
    Context* curr = this;
    while (curr->parent) curr = curr->parent;
    return curr;
}

const Context* Context::getRoot() const {
    const Context* curr = this;
    while (curr->parent) curr = curr->parent;
    return curr;
}

void Context::defineFunc(const std::string& name, std::shared_ptr<Node> func) {
    functions[name] = std::move(func);
}

std::shared_ptr<Node> Context::getFunc(const std::string& name) const {
    auto it = functions.find(name);
    if (it != functions.end()) return it->second;
    if (parent) return parent->getFunc(name);
    return nullptr;
}

void Context::defineVar(const std::string& name, const std::string& type, const Value& value) {
    variables[name] = {type, value.value};
}

void Context::setVar(const std::string& name, Value val) {
    for (Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it == scope->variables.end()) continue;
        Value& target = it->second;
        if (target.type == "array") {
            // Assignment copies the elements into the variable's own buffer, so the
            // variable never points at a temporary that its creator frees later.
            std::vector<Value> items = takeArray(val, "value assigned to '" + name + "'");
            arrayOf(target, "array '" + name + "'") = std::move(items);
            return;
        }
        runtime::coerce(target.type, val, "variable '" + name + "'");
        target.value = val.value;
        return;
    }
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

} // namespace foxlang
