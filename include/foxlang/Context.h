#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace foxlang {

struct Node;
class Interpreter;
namespace graphics { class Window; }

struct FuncParam {
    std::string type;
    std::string name;
};

struct Value {
    std::string type;
    std::string value;
};

struct ReturnValue {
    Value value;
};

struct BreakException {};
struct ContinueException {};

struct Context {
    Context* parent = nullptr;
    Interpreter* interpreter = nullptr;
    std::map<std::string, Value> variables;
    std::map<std::string, std::shared_ptr<Node>> functions;
    std::map<std::string, std::vector<Value>> arrays;
    std::shared_ptr<graphics::Window> graphics;

    bool exists(const std::string& name) const;
    Value getVar(const std::string& name) const;
    std::vector<Value>& getArray(const std::string& name);
    Context* getRoot();
    const Context* getRoot() const;

    void defineFunc(const std::string& name, std::shared_ptr<Node> func);
    std::shared_ptr<Node> getFunc(const std::string& name) const;

    void defineVar(const std::string& name, const std::string& type, const Value& value);
    void setVar(const std::string& name, Value val);
};

} // namespace foxlang

// Compatibility aliases
using foxlang::FuncParam;
using foxlang::Value;
using foxlang::ReturnValue;
using foxlang::BreakException;
using foxlang::ContinueException;
using foxlang::Context;
