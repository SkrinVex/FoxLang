#include "foxlang/Context.h"
#include "foxlang/AST.h"
#include "foxlang/Runtime.h"
#include <algorithm>
#include <atomic>
#include <string>
#include <stdexcept>

namespace foxlang {

Context::~Context() { releaseArrays(); }

// A loop body is not a scope in FoxLang, so re-declaring an array in one used to
// strand the previous buffer in the root context until the program ended.
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
    root->arrays[id] = std::vector<Value>(size, {"int", "0"});
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

bool Context::exists(const std::string& name) const {
    if (variables.count(name) || arrays.count(name)) return true;
    if (parent) return parent->exists(name);
    return false;
}

Value Context::getVar(const std::string& name) const {
    auto it = variables.find(name);
    if (it != variables.end()) return it->second;
    if (parent) return parent->getVar(name);
    throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
}

std::vector<Value>& Context::getArray(const std::string& name) {
    auto it = arrays.find(name);
    if (it != arrays.end()) return it->second;
    if (parent) return parent->getArray(name);
    throw std::runtime_error("Runtime Error: Array '" + name + "' not found!");
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
    auto it = variables.find(name);
    if (it != variables.end()) {
        if (it->second.type != val.type) {
            if (it->second.type == "float" && val.type == "int") {
                val.type = "float";
            } else if (it->second.type == "int" && val.type == "float") {
                val.type = "int";
                narrowToInt(val, "variable", name);
            } else {
                throw std::runtime_error("Type Error: Cannot assign value of type '" + val.type + "' to variable '" + name + "' of type '" + it->second.type + "'");
            }
        }
        if (it->second.type == "int") narrowToInt(val, "variable", name);
        it->second.value = val.value;
        return;
    }
    if (parent) {
        parent->setVar(name, val);
        return;
    }
    throw std::runtime_error("Error: Variable '" + name + "' not defined!");
}

} // namespace foxlang
