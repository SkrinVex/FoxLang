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
    // Array buffers live in the root context, so ids stay valid when an array is
    // passed to a function. This scope owns the ones declared in it and frees them.
    std::vector<std::string> ownedArrays;
    std::shared_ptr<graphics::Window> graphics;

    Context() = default;
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    bool exists(const std::string& name) const;
    Value getVar(const std::string& name) const;
    std::vector<Value>& getArray(const std::string& name);
    Context* getRoot();
    const Context* getRoot() const;

    void defineFunc(const std::string& name, std::shared_ptr<Node> func);
    std::shared_ptr<Node> getFunc(const std::string& name) const;

    void defineVar(const std::string& name, const std::string& type, const Value& value);
    void setVar(const std::string& name, Value val);

    std::string declareArray(const std::string& name, size_t size);
    void releaseArrays();
};

} // namespace foxlang

// Compatibility aliases
using foxlang::FuncParam;
using foxlang::Value;
using foxlang::ReturnValue;
using foxlang::BreakException;
using foxlang::ContinueException;
using foxlang::Context;
