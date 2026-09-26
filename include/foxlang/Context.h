#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <iosfwd>
#include <stdexcept>
#include <utility>
#include "foxlang/SourceLocation.h"

namespace foxlang {

struct Node;
class Interpreter;
namespace bytecode { struct Proto; }
namespace runtime { struct Builtin; }
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
    // Function: a FoxLang function, a builtin or a lambda as a value. Box: the cell of a
    // local variable that a lambda captured; code reads through it, a program never
    // sees one.
    enum class Kind : unsigned char { Void, Int, Float, Bool, String, Array, Map, Struct, Function, Box };

    Value() noexcept { data_.bits = 0; }
    Value(const Value& other) noexcept : kind_(other.kind_), data_(other.data_) { retain(); }
    Value(Value&& other) noexcept : kind_(other.kind_), data_(other.data_) { other.kind_ = Kind::Void; }
    Value& operator=(const Value& other) noexcept {
        other.retain(); // first: assigning a value to itself must not free it
        release();
        kind_ = other.kind_;
        data_ = other.data_;
        return *this;
    }
    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            release();
            kind_ = other.kind_;
            data_ = other.data_;
            other.kind_ = Kind::Void;
        }
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

    // Overwrite with a scalar in place, for the VM's arithmetic.
    void setInt(long long value) noexcept {
        release();
        kind_ = Kind::Int;
        data_.integer = value;
    }
    void setReal(double value) noexcept {
        release();
        kind_ = Kind::Float;
        data_.real = value;
    }
    void setBool(bool value) noexcept {
        release();
        kind_ = Kind::Bool;
        data_.boolean = value;
    }

    Kind kind() const { return kind_; }
    bool is(Kind kind) const { return kind_ == kind; }
    bool isVoid() const { return kind_ == Kind::Void; }
    bool isInt() const { return kind_ == Kind::Int; }
    bool isFloat() const { return kind_ == Kind::Float; }
    bool isNumber() const { return kind_ == Kind::Int || kind_ == Kind::Float; }
    bool isBool() const { return kind_ == Kind::Bool; }
    bool isString() const { return kind_ == Kind::String; }
    bool isContainer() const { return kind_ >= Kind::Array; }
    bool isFunction() const { return kind_ == Kind::Function; }

    // The payload of a value known to be of that kind.
    long long asInt() const { return data_.integer; }
    double asFloat() const { return data_.real; }
    bool asBool() const { return data_.boolean; }
    const std::string& str() const { return data_.string->text; }
    // An array, a map, a struct, a function or a box; null for everything else.
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

// The arguments of a call: consecutive values that the caller keeps alive, such as a
// vector or the registers of the bytecode VM. Builtins read them without copying.
class Arguments {
public:
    Arguments(const Value* data, size_t count) : data_(data), count_(count) {}
    Arguments(const std::vector<Value>& values) : data_(values.data()), count_(values.size()) {}
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    const Value& operator[](size_t index) const { return data_[index]; }
    const Value* begin() const { return data_; }
    const Value* end() const { return data_ + count_; }

private:
    const Value* data_;
    size_t count_;
};

struct StructType {
    std::string name;
    std::vector<FuncParam> fields;
    std::vector<std::shared_ptr<Node>> defaults; // an initial value per field, or null
    bool isEnum = false; // an enum: fields name and value; its values are fixed objects
    // Methods by name: FoxLang functions (FuncDefNode) whose first parameter is `this`.
    std::unordered_map<std::string, std::shared_ptr<const Node>> methods;
    // What each field's type holds, worked out when the first value is built, and the
    // defaults' compiled code.
    mutable std::vector<Value::Kind> kinds;
    mutable std::vector<std::shared_ptr<bytecode::Proto>> defaultCode;
};

// What a function value calls: a FoxLang function, a builtin, or a lambda's code, whose
// captured variables are the boxes in Object::items.
struct Callee {
    std::string name;                     // for messages and print(): "add", "lambda"
    std::shared_ptr<const Node> function; // a FoxLang function (a FuncDefNode)
    const runtime::Builtin* builtin = nullptr;
    std::shared_ptr<bytecode::Proto> lambda;
};

struct Object {
    enum class Kind { Array, Map, Struct, Function, Box };
    explicit Object(Kind k) : kind(k) {}
    ~Object(); // leaves the list of live containers
    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;
    Kind kind;
    unsigned refs = 0; // the values naming this container
    // Every live container is in one table, for the cycle collector (collectCycles).
    // The fields are packed so that a container stays in the same allocation size.
    int gcRefs = 0;
    unsigned gcRound = 0; // the collection that last set gcRefs
    std::uint32_t gcIndex = 0;
    bool frozen = false; // an enum's values and its map of them cannot be changed
    bool moduleAlias = false; // using math as m: m.name(...) reaches builtins too
    // array<int>, map<string, int>: the type every element (map value) has, interned;
    // null for a container that takes any values.
    const std::string* elementType = nullptr;
    // Array: the elements. Struct: the fields in declaration order.
    std::vector<Value> items;
    // Map: keys in insertion order, items[i] belongs to keys[i].
    std::vector<std::string> keys;
    std::shared_ptr<const StructType> structType;
    std::shared_ptr<const Callee> callee; // for a function

    // Map access; -1 when the key is absent.
    long find(const std::string& key) const;
    Value& slot(const std::string& key); // inserts a void value for a new key
    bool erase(const std::string& key);
    // Drops everything the container holds: how the cycle collector breaks a ring.
    void clearContents();

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
    // Globals declared with a type T?: an assignment converts to it and may store null.
    std::unordered_map<std::string, std::string> nullableGlobals;
    std::set<std::string> constants; // globals declared const
    // While the program runs, the variables of its top level are its registers; code
    // that names them (a function, a module, the debugger) finds them here.
    std::unordered_map<std::string, Value*> programGlobals;
    std::map<std::string, std::shared_ptr<Node>> functions;
    // Functions replaced by a definition with another body: their code may still be
    // running, so it is kept until the functions are cleared.
    std::vector<std::shared_ptr<Node>> retired;
    std::map<std::string, std::shared_ptr<const StructType>> structs;
    // The numbered variables of the running function (or of the program's blocks),
    // shared by every block scope inside it; the frame owner is the scope that made them.
    Value* slots = nullptr;
    const std::vector<std::string>* slotNames = nullptr;
    bool frame = false;
    // Under the debugger, the frame owner marks the slots whose declaration has run, so
    // a variable holding null is told apart from one not declared yet.
    std::vector<char> declared;
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
// Frees containers that only hold each other (a[0] = a, two structs naming each
// other) and that nothing else reaches. Runs by itself as containers are created;
// returns how many containers it freed.
size_t collectCycles();
Value makeMap();
// A new container with the same contents, nested containers copied too (copy()).
Value deepCopy(const Value& value);
// Stores a value into an existing variable, converting it to the variable's type.
void assign(Value& target, Value val, const std::string& name);
bool deepEqual(const Value& a, const Value& b);
} // namespace runtime

} // namespace foxlang
