#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iosfwd>
#include <stdexcept>

namespace foxlang {

struct Node;
class Interpreter;
namespace graphics { class Window; }

struct FuncParam {
    std::string type;
    std::string name;
};

// The runtime exposes every value as text, because that is what the language
// stores: variables, arrays, builtins and the embedding API all read Value::value.
// A number additionally carries its binary form, so a chain of arithmetic neither
// formats nor parses text until something actually reads the text. Materialization
// happens on the interpreter thread, like everything else in the runtime.
class Text {
public:
    Text() = default;
    Text(std::string characters) : text_(std::move(characters)) {}
    Text(const char* characters) : text_(characters) {}

    static Text integer(long long value);
    static Text real(double value);

    const std::string& str() const {
        if (!ready_) materialize();
        return text_;
    }
    operator const std::string&() const { return str(); }
    const char* c_str() const { return str().c_str(); }
    bool empty() const { return ready_ ? text_.empty() : false; }
    std::size_t size() const { return str().size(); }
    std::size_t length() const { return str().length(); }
    std::size_t find(const std::string& needle) const { return str().find(needle); }

    bool isInteger() const { return kind_ == Kind::Integer; }
    bool isReal() const { return kind_ == Kind::Real; }
    long long integerValue() const { return integer_; }
    double realValue() const { return real_; }

private:
    enum class Kind { Characters, Integer, Real };
    mutable std::string text_;
    mutable bool ready_ = true;
    Kind kind_ = Kind::Characters;
    long long integer_ = 0;
    double real_ = 0;
    void materialize() const;
};

// std::string's own operators are templates, so they never see the conversion above.
inline bool operator==(const Text& a, const Text& b) { return a.str() == b.str(); }
inline bool operator==(const Text& a, const char* b) { return a.str() == b; }
inline bool operator==(const char* a, const Text& b) { return a == b.str(); }
inline bool operator==(const Text& a, const std::string& b) { return a.str() == b; }
inline bool operator==(const std::string& a, const Text& b) { return a == b.str(); }
inline bool operator!=(const Text& a, const Text& b) { return !(a == b); }
inline bool operator!=(const Text& a, const char* b) { return !(a == b); }
inline bool operator!=(const char* a, const Text& b) { return !(a == b); }
inline bool operator!=(const Text& a, const std::string& b) { return !(a == b); }
inline bool operator!=(const std::string& a, const Text& b) { return !(a == b); }
inline bool operator<(const Text& a, const Text& b) { return a.str() < b.str(); }
inline bool operator<=(const Text& a, const Text& b) { return a.str() <= b.str(); }
inline bool operator>(const Text& a, const Text& b) { return a.str() > b.str(); }
inline bool operator>=(const Text& a, const Text& b) { return a.str() >= b.str(); }
inline std::string operator+(const Text& a, const Text& b) { return a.str() + b.str(); }
inline std::string operator+(const Text& a, const char* b) { return a.str() + b; }
inline std::string operator+(const char* a, const Text& b) { return a + b.str(); }
inline std::string operator+(const Text& a, const std::string& b) { return a.str() + b; }
inline std::string operator+(const std::string& a, const Text& b) { return a + b.str(); }
std::ostream& operator<<(std::ostream& out, const Text& text);

struct Value {
    std::string type;
    Text value;
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
