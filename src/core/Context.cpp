#include "foxlang/Context.h"
#include "foxlang/AST.h"
#include "foxlang/Runtime.h"
#include <algorithm>
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

long Object::find(const std::string& key) const {
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == key) return static_cast<long>(i);
    return -1;
}

Value& Object::slot(const std::string& key) {
    long at = find(key);
    if (at >= 0) return items[static_cast<size_t>(at)];
    keys.push_back(key);
    items.push_back({"void", ""});
    return items.back();
}

bool Object::erase(const std::string& key) {
    long at = find(key);
    if (at < 0) return false;
    keys.erase(keys.begin() + at);
    items.erase(items.begin() + at);
    return true;
}

Value Context::getVar(const std::string& name) const {
    for (const Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it != scope->variables.end()) return it->second;
    }
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

Value* Context::findVar(const std::string& name) {
    for (Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it != scope->variables.end()) return &it->second;
    }
    return nullptr;
}

std::vector<Value>& Context::arrayOf(const Value& value, const std::string& what) {
    if (value.type != "array" || !value.ref)
        throw std::runtime_error("Type Error: " + what + " must be an array, got '" + value.type + "'");
    return value.ref->items;
}

namespace runtime {

Value makeArray(std::vector<Value> items) {
    auto object = std::make_shared<Object>(Object::Kind::Array);
    object->items = std::move(items);
    return {"array", "", std::move(object)};
}

Value makeMap() { return {"map", "", std::make_shared<Object>(Object::Kind::Map)}; }

Value deepCopy(const Value& value) {
    if (!value.ref) return value;
    auto copy = std::make_shared<Object>(value.ref->kind);
    copy->keys = value.ref->keys;
    copy->structType = value.ref->structType;
    copy->items.reserve(value.ref->items.size());
    for (const auto& item : value.ref->items) copy->items.push_back(deepCopy(item));
    return {value.type, value.value, std::move(copy)};
}

void own(Value& value) {
    // Nothing else refers to a temporary, so it can be stored without a copy.
    if (value.ref && value.ref.use_count() > 1) value = deepCopy(value);
}

bool deepEqual(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    if (!a.ref || !b.ref) return !a.ref && !b.ref && a.value == b.value;
    if (a.ref == b.ref) return true;
    if (a.ref->keys != b.ref->keys || a.ref->items.size() != b.ref->items.size()) return false;
    for (size_t i = 0; i < a.ref->items.size(); ++i)
        if (!deepEqual(a.ref->items[i], b.ref->items[i])) return false;
    return true;
}

} // namespace runtime

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

std::shared_ptr<const StructType> Context::getStruct(const std::string& name) const {
    const Context* root = getRoot();
    auto it = root->structs.find(name);
    return it == root->structs.end() ? nullptr : it->second;
}

void Context::defineVar(const std::string& name, const std::string& type, const Value& value) {
    variables[name] = {type, value.value, value.ref};
}

void Context::setVar(const std::string& name, Value val) {
    for (Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it == scope->variables.end()) continue;
        Value& target = it->second;
        // The usual case, a value of the variable's own type, needs no conversion.
        bool sameType = target.type == val.type && (val.type != "int" || val.value.isInteger());
        if (!sameType) runtime::coerce(target.type, val, "variable '" + name + "'");
        // A container assigned to a variable becomes its own copy.
        runtime::own(val);
        target.value = std::move(val.value);
        target.ref = std::move(val.ref);
        return;
    }
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

} // namespace foxlang
