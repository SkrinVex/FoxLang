#include "foxlang/Context.h"
#include "foxlang/AST.h"
#include "foxlang/Runtime.h"
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <map>
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
std::ostream& operator<<(std::ostream& out, const TypeName& type) { return out << type.str(); }

const std::string* TypeName::names() {
    static const std::string builtin[] = {"void", "int", "float", "string", "bool", "array", "map"};
    return builtin;
}

void TypeName::assign(const char* text, size_t length) {
    static const Kind kinds[] = {Kind::Void, Kind::Int, Kind::Float, Kind::String, Kind::Bool, Kind::Array, Kind::Map};
    const std::string* builtin = names();
    for (int i = 0; i < 7; ++i) {
        if (builtin[i].size() == length && builtin[i].compare(0, length, text, length) == 0) {
            kind_ = kinds[i];
            name_ = &builtin[i];
            return;
        }
    }
    // Struct names are kept once each; a set never moves the strings it holds.
    static std::unordered_set<std::string> interned;
    static std::mutex guard;
    std::lock_guard<std::mutex> lock(guard);
    kind_ = Kind::Named;
    name_ = &*interned.emplace(text, length).first;
}

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
    if (Value* found = const_cast<Context*>(this)->findVar(name)) return *found;
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

// By name: the scopes' own variables, the numbered slots of the frame they belong to
// (the latest declaration of the name that is alive), then the globals.
Value* Context::findVar(const std::string& name) {
    bool frameSearched = false;
    for (Context* scope = this; scope; scope = scope->parent) {
        auto it = scope->variables.find(name);
        if (it != scope->variables.end()) return &it->second;
        if (scope->frame && !frameSearched && scope->slots && scope->slotNames) {
            frameSearched = true;
            const auto& names = *scope->slotNames;
            for (size_t i = names.size(); i-- > 0;)
                if (names[i] == name && !scope->slots[i].type.is(TypeName::Kind::Void)) return &scope->slots[i];
        }
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

namespace {

// A container that holds itself (a[0] = a) is copied once, not forever.
Value copyOf(const Value& value, std::map<const Object*, std::shared_ptr<Object>>& copies) {
    if (!value.ref) return value;
    auto done = copies.find(value.ref.get());
    if (done != copies.end()) return {value.type, value.value, done->second};
    auto copy = std::make_shared<Object>(value.ref->kind);
    copies[value.ref.get()] = copy;
    copy->keys = value.ref->keys;
    copy->structType = value.ref->structType;
    copy->items.reserve(value.ref->items.size());
    for (const auto& item : value.ref->items) copy->items.push_back(copyOf(item, copies));
    return {value.type, value.value, std::move(copy)};
}

// Two containers that are being compared already, higher up, count as equal there.
bool equalTo(const Value& a, const Value& b, std::vector<std::pair<const Object*, const Object*>>& open) {
    if (a.type != b.type) return false;
    if (!a.ref || !b.ref) return !a.ref && !b.ref && a.value == b.value;
    if (a.ref == b.ref) return true;
    std::pair<const Object*, const Object*> pair{a.ref.get(), b.ref.get()};
    if (std::find(open.begin(), open.end(), pair) != open.end()) return true;
    if (a.ref->keys != b.ref->keys || a.ref->items.size() != b.ref->items.size()) return false;
    open.push_back(pair);
    bool equal = true;
    for (size_t i = 0; equal && i < a.ref->items.size(); ++i) equal = equalTo(a.ref->items[i], b.ref->items[i], open);
    open.pop_back();
    return equal;
}

} // namespace

Value deepCopy(const Value& value) {
    std::map<const Object*, std::shared_ptr<Object>> copies;
    return copyOf(value, copies);
}

bool deepEqual(const Value& a, const Value& b) {
    std::vector<std::pair<const Object*, const Object*>> open;
    return equalTo(a, b, open);
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
    ++functionGeneration;
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

namespace runtime {
void assign(Value& target, Value val, const std::string& name) {
    // The usual case, a value of the variable's own type, needs no conversion.
    bool sameType = target.type == val.type && (!val.type.is(TypeName::Kind::Int) || val.value.isInteger());
    if (!sameType) coerce(target.type, val, "variable '" + name + "'");
    // Arrays, maps and structs are shared: the variable names the same container.
    target.value = std::move(val.value);
    target.ref = std::move(val.ref);
}
} // namespace runtime

void Context::setVar(const std::string& name, Value val) {
    if (Value* target = findVar(name)) {
        runtime::assign(*target, std::move(val), name);
        return;
    }
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

} // namespace foxlang
