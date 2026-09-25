#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <iosfwd>
#include <stdexcept>
#include "foxlang/SourceLocation.h"

namespace foxlang {

struct Node;
class Interpreter;
namespace graphics { class Window; }
namespace platform { struct ServerState; }

struct FuncParam {
    std::string type;
    std::string name;
    SourceRange range; // where a parameter of a FoxLang function is named; empty for builtins

    FuncParam() = default;
    FuncParam(std::string t, std::string n, SourceRange r = {}) : type(std::move(t)), name(std::move(n)), range(r) {}
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

struct Object;

// The type of a value: "int", "float", "string", "bool", "void", "array", "map" or
// the name of a struct. It reads and compares like a string, but is a kind and a
// pointer to one shared copy of the name, so values are cheap to make and copy and
// the interpreter can switch on kind() instead of comparing text.
class TypeName {
public:
    enum class Kind : unsigned char { Void, Int, Float, String, Bool, Array, Map, Named };

    TypeName() : kind_(Kind::Void), name_(&names()[0]) {}
    TypeName(const char* name) { assign(name, std::char_traits<char>::length(name)); }
    TypeName(const std::string& name) { assign(name.data(), name.size()); }

    Kind kind() const { return kind_; }
    bool is(Kind kind) const { return kind_ == kind; }
    const std::string& str() const { return *name_; }
    operator const std::string&() const { return *name_; }
    bool empty() const { return name_->empty(); }
    size_t size() const { return name_->size(); }

    friend bool operator==(const TypeName& a, const TypeName& b) { return a.name_ == b.name_; }
    friend bool operator!=(const TypeName& a, const TypeName& b) { return a.name_ != b.name_; }
    friend bool operator==(const TypeName& a, const char* b) { return *a.name_ == b; }
    friend bool operator!=(const TypeName& a, const char* b) { return *a.name_ != b; }
    friend bool operator==(const TypeName& a, const std::string& b) { return *a.name_ == b; }
    friend bool operator!=(const TypeName& a, const std::string& b) { return *a.name_ != b; }
    friend bool operator==(const std::string& a, const TypeName& b) { return a == *b.name_; }
    friend bool operator!=(const std::string& a, const TypeName& b) { return a != *b.name_; }
    friend std::string operator+(const std::string& a, const TypeName& b) { return a + *b.name_; }
    friend std::string operator+(const char* a, const TypeName& b) { return a + *b.name_; }
    friend std::string operator+(const TypeName& a, const std::string& b) { return *a.name_ + b; }
    friend std::string operator+(const TypeName& a, const char* b) { return *a.name_ + b; }

private:
    Kind kind_;
    const std::string* name_; // the builtin names, or one interned copy per struct name
    static const std::string* names();
    void assign(const char* text, size_t length);
};
std::ostream& operator<<(std::ostream& out, const TypeName& type);

// A scalar keeps its text in `value`; an array, a map or a struct lives in `ref`,
// shared by every Value that names it.
struct Value {
    TypeName type;
    Text value;
    std::shared_ptr<Object> ref;

    Value() = default;
    Value(TypeName t, Text v, std::shared_ptr<Object> r = nullptr)
        : type(t), value(std::move(v)), ref(std::move(r)) {}
};

struct StructType {
    std::string name;
    std::vector<FuncParam> fields;
    std::vector<std::shared_ptr<Node>> defaults; // an initial value per field, or null
};

struct Object {
    enum class Kind { Array, Map, Struct };
    explicit Object(Kind k) : kind(k) {}
    Kind kind;
    // Array: the elements. Struct: the fields in declaration order.
    std::vector<Value> items;
    // Map: keys in insertion order, items[i] belongs to keys[i].
    std::vector<std::string> keys;
    std::shared_ptr<const StructType> structType;

    // Map access; -1 when the key is absent.
    long find(const std::string& key) const;
    Value& slot(const std::string& key); // inserts a void value for a new key
    bool erase(const std::string& key);

private:
    // Where each key sits in `keys`, built once the map is big enough for a scan to
    // cost more than hashing; it is in step whenever it has as many entries as `keys`.
    mutable std::unordered_map<std::string, size_t> index_;
};

// exit(code) unwinds the whole program; it is deliberately not a std::exception,
// so handlers that report runtime errors never swallow it.
struct ExitRequest {
    int code = 0;
};

struct Context {
    Context* parent = nullptr;
    Interpreter* interpreter = nullptr;
    std::map<std::string, Value> variables;
    std::map<std::string, std::shared_ptr<Node>> functions;
    std::map<std::string, std::shared_ptr<const StructType>> structs;
    // The numbered variables of the running function (or of the program's blocks),
    // shared by every block scope inside it; the frame owner is the scope that made them.
    Value* slots = nullptr;
    const std::vector<std::string>* slotNames = nullptr;
    bool frame = false;
    // The root counts how often its variables were cleared and its functions changed,
    // so what calls and globals cached goes stale.
    unsigned generation = 0;
    unsigned functionGeneration = 0;
    std::shared_ptr<graphics::Window> graphics;
    std::shared_ptr<platform::ServerState> server;

    Context() = default;
    // A block scope inside `outer`: same function frame, own names.
    explicit Context(Context& outer)
        : parent(&outer), interpreter(outer.interpreter), slots(outer.slots), slotNames(outer.slotNames) {}
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Value getVar(const std::string& name) const;
    // The variable itself, for assignment through it; null when it is not declared.
    Value* findVar(const std::string& name);
    Context* getRoot();
    const Context* getRoot() const;

    void defineFunc(const std::string& name, std::shared_ptr<Node> func);
    std::shared_ptr<Node> getFunc(const std::string& name) const;
    std::shared_ptr<const StructType> getStruct(const std::string& name) const;

    void defineVar(const std::string& name, const std::string& type, const Value& value);
    void setVar(const std::string& name, Value val);

    // The elements of an array value, for builtins and the debugger.
    std::vector<Value>& arrayOf(const Value& value, const std::string& what);
};

namespace runtime {
Value makeArray(std::vector<Value> items);
Value makeMap();
// A new container with the same contents, nested containers copied too (copy()).
Value deepCopy(const Value& value);
// Stores a value into an existing variable, converting it to the variable's type.
void assign(Value& target, Value val, const std::string& name);
bool deepEqual(const Value& a, const Value& b);
} // namespace runtime

} // namespace foxlang
