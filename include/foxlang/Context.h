#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <iosfwd>
#include <stdexcept>
#include <utility>
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

struct Object;

// The text of a string value. It never changes once made, so every copy of the value
// shares it; the count is not atomic because values live on the interpreter thread.
struct StringData {
    unsigned refs = 1;
    std::string text;
    explicit StringData(std::string t) : text(std::move(t)) {}
};

// A value is its kind and one machine word: an int, a float or a bool are stored in
// it, a string and a container (array, map, struct) are pointers to shared data that
// the value counts references to. Copying a value never copies text or elements.
class Value {
public:
    enum class Kind : unsigned char { Void, Int, Float, Bool, String, Array, Map, Struct };

    Value() noexcept { data_.bits = 0; }
    Value(const Value& other) noexcept : kind_(other.kind_), data_(other.data_) { retain(); }
    Value(Value&& other) noexcept : kind_(other.kind_), data_(other.data_) { other.kind_ = Kind::Void; }
    Value& operator=(const Value& other) noexcept {
        Value copy(other);
        swap(copy);
        return *this;
    }
    Value& operator=(Value&& other) noexcept {
        Value moved(std::move(other));
        swap(moved);
        return *this;
    }
    ~Value() { release(); }
    void swap(Value& other) noexcept {
        std::swap(kind_, other.kind_);
        std::swap(data_, other.data_);
    }

    static Value integer(long long value) noexcept {
        Value v;
        v.kind_ = Kind::Int;
        v.data_.integer = value;
        return v;
    }
    static Value real(double value) noexcept {
        Value v;
        v.kind_ = Kind::Float;
        v.data_.real = value;
        return v;
    }
    static Value boolean(bool value) noexcept {
        Value v;
        v.kind_ = Kind::Bool;
        v.data_.boolean = value;
        return v;
    }
    static Value string(std::string text) {
        Value v;
        v.data_.string = new StringData(std::move(text));
        v.kind_ = Kind::String;
        return v;
    }
    // A new, empty array, map or struct.
    static Value container(Kind kind);

    Kind kind() const { return kind_; }
    bool is(Kind kind) const { return kind_ == kind; }
    bool isVoid() const { return kind_ == Kind::Void; }
    bool isInt() const { return kind_ == Kind::Int; }
    bool isFloat() const { return kind_ == Kind::Float; }
    bool isNumber() const { return kind_ == Kind::Int || kind_ == Kind::Float; }
    bool isBool() const { return kind_ == Kind::Bool; }
    bool isString() const { return kind_ == Kind::String; }
    bool isContainer() const { return kind_ >= Kind::Array; }

    // The payload of a value known to be of that kind.
    long long asInt() const { return data_.integer; }
    double asFloat() const { return data_.real; }
    bool asBool() const { return data_.boolean; }
    const std::string& str() const { return data_.string->text; }
    // An array, a map or a struct; null for everything else.
    Object* ref() const { return isContainer() ? data_.object : nullptr; }

    // A scalar as text: 42, 2.5, true, the string itself; empty for void and containers.
    std::string text() const;
    // "void", "int", "float", "bool", "string", "array", "map" or the struct's name.
    const std::string& typeName() const;
    static const std::string& nameOf(Kind kind);

private:
    Kind kind_ = Kind::Void;
    union Data {
        long long integer;
        double real;
        bool boolean;
        StringData* string;
        Object* object;
        unsigned long long bits;
    } data_;

    void retain() const noexcept {
        if (kind_ >= Kind::String) retainShared();
    }
    void release() noexcept {
        if (kind_ >= Kind::String) releaseShared();
    }
    void retainShared() const noexcept;
    void releaseShared() noexcept;
    friend struct Object;
};

std::ostream& operator<<(std::ostream& out, const Value& value);

struct StructType {
    std::string name;
    std::vector<FuncParam> fields;
    std::vector<std::shared_ptr<Node>> defaults; // an initial value per field, or null
};

struct Object {
    enum class Kind { Array, Map, Struct };
    explicit Object(Kind k) : kind(k) {}
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Kind kind;
    unsigned refs = 0; // the values naming this container
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
    // Only big maps pay for it: every array, struct and small map keeps a null pointer.
    mutable std::unique_ptr<std::unordered_map<std::string, size_t>> index_;
};

inline void Value::retainShared() const noexcept {
    if (kind_ == Kind::String) ++data_.string->refs;
    else ++data_.object->refs;
}

inline void Value::releaseShared() noexcept {
    if (kind_ == Kind::String) {
        if (--data_.string->refs == 0) delete data_.string;
    } else if (--data_.object->refs == 0) {
        delete data_.object;
    }
    kind_ = Kind::Void;
}

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
    Context* getRoot() {
        Context* root = this;
        while (root->parent) root = root->parent;
        return root;
    }
    const Context* getRoot() const {
        const Context* root = this;
        while (root->parent) root = root->parent;
        return root;
    }

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
