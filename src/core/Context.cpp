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

namespace {

// The live containers. Reference counts free most of them the moment they are no
// longer used; only rings of containers that name each other stay, and the collector
// looks for those once enough containers were created since last time. A table rather
// than a linked list, so the collector reads it straight through; a container that goes
// away only empties its own entry, and the table is packed now and then.
struct Containers {
    std::vector<Object*> all; // with holes where containers went away
    size_t live = 0;
    size_t created = 0;       // since the last collection
    size_t threshold = 100000; // eight times the live ones: the work per container stays small
    unsigned round = 0;
    bool collecting = false;
    std::vector<char> holds; // during a collection: which containers hold containers

    // Closes the holes; only the containers after the first hole move.
    void pack() {
        if (all.size() == live) return;
        size_t kept = 0;
        while (kept < all.size() && all[kept]) ++kept;
        for (size_t i = kept; i < all.size(); ++i) {
            Object* o = all[i];
            if (!o) continue;
            o->gcIndex = static_cast<std::uint32_t>(kept);
            all[kept++] = o;
        }
        all.resize(kept);
    }
};

Containers& containers() {
    static Containers* list = new Containers; // outlives every container, even static ones
    return *list;
}

// Memory for containers in blocks, and the cells given back, for the next ones. Like
// the list of live containers it serves the interpreter's thread; the blocks stay.
struct ObjectCells {
    union Cell {
        Cell* next;
        alignas(Object) unsigned char bytes[sizeof(Object)];
    };
    static constexpr size_t perBlock = 256;
    Cell* free = nullptr;
    Cell* fresh = nullptr; // the unused rest of the newest block
    Cell* end = nullptr;
};

ObjectCells& objectCells() {
    static ObjectCells* cells = new ObjectCells; // outlives every container, even static ones
    return *cells;
}

} // namespace

void* Object::operator new(std::size_t size) {
    if (size != sizeof(Object)) return ::operator new(size);
    ObjectCells& cells = objectCells();
    if (ObjectCells::Cell* cell = cells.free) {
        cells.free = cell->next;
        return cell;
    }
    if (cells.fresh == cells.end) {
        cells.fresh = static_cast<ObjectCells::Cell*>(::operator new(sizeof(ObjectCells::Cell) * ObjectCells::perBlock));
        cells.end = cells.fresh + ObjectCells::perBlock;
    }
    return cells.fresh++;
}

void Object::operator delete(void* memory, std::size_t size) noexcept {
    if (!memory) return;
    if (size != sizeof(Object)) {
        ::operator delete(memory);
        return;
    }
    ObjectCells& cells = objectCells();
    auto* cell = static_cast<ObjectCells::Cell*>(memory);
    cell->next = cells.free;
    cells.free = cell;
}

Object::~Object() {
    Containers& list = containers();
    list.all[gcIndex] = nullptr;
    --list.live;
}

void Object::clearContents() {
    items.clear();
    keys.clear();
    index_.reset();
    callee.reset();
}

#if defined(__GNUC__) || defined(__clang__)
#define FOXLANG_PREFETCH(address) __builtin_prefetch(address)
#else
#define FOXLANG_PREFETCH(address) ((void)0)
#endif

size_t runtime::collectCycles() {
    Containers& list = containers();
    if (list.collecting) return 0;
    list.collecting = true;
    list.created = 0;
    list.pack();
    unsigned round = ++list.round;
    const std::vector<Object*>& all = list.all;
    size_t count = all.size();
    // Trial deletion: take away the references containers hold to each other. What
    // still has references left is named from outside (a variable, a register, the
    // C++ code) and keeps alive everything it reaches; the rest are rings.
    // Most containers were made long ago and are far out of the cache, so each is
    // asked for ahead of time, and one without containers inside is not read twice.
    std::vector<char>& holds = list.holds;
    holds.assign(count, 0);
    auto start = [round](Object* o) {
        if (o->gcRound != round) {
            o->gcRound = round;
            o->gcRefs = static_cast<int>(o->refs);
        }
    };
    constexpr size_t ahead = 16;
    for (size_t i = 0; i < count; ++i) {
        if (i + ahead < count) FOXLANG_PREFETCH(all[i + ahead]);
        if (i + ahead / 2 < count) FOXLANG_PREFETCH(all[i + ahead / 2]->items.data());
        Object* o = all[i];
        start(o);
        for (const Value& item : o->items) {
            if (Object* target = item.ref()) {
                holds[i] = 1;
                start(target);
                --target->gcRefs;
            }
        }
    }
    std::vector<Object*> reached;
    size_t alive = 0;
    auto reach = [&](Object* target) {
        if (target->gcRefs < 0) return;
        target->gcRefs = -1;
        ++alive;
        if (holds[target->gcIndex]) reached.push_back(target);
    };
    for (size_t i = 0; i < count; ++i) {
        if (i + ahead < count) FOXLANG_PREFETCH(all[i + ahead]);
        if (all[i]->gcRefs > 0) reach(all[i]);
        while (!reached.empty()) {
            Object* next = reached.back();
            reached.pop_back();
            const std::vector<Value>& items = next->items;
            for (size_t j = 0; j < items.size(); ++j) {
                if (j + ahead < items.size())
                    if (Object* later = items[j + ahead].ref()) FOXLANG_PREFETCH(later);
                if (Object* target = items[j].ref()) reach(target);
            }
        }
    }
    list.threshold = std::max<size_t>(100000, 8 * list.live);
    if (alive == count) { // the usual case: no rings, nothing more to look at
        list.collecting = false;
        return 0;
    }
    // A container passed over before something reached it was marked since; what is
    // still unmarked now is only reachable from rings.
    std::vector<Object*> rings;
    for (Object* o : all) {
        if (o->gcRefs >= 0) {
            ++o->refs; // held while the rings are taken apart
            rings.push_back(o);
        }
    }
    for (Object* o : rings) o->clearContents();
    for (Object* o : rings)
        if (--o->refs == 0) delete o;
    list.pack();
    list.threshold = std::max<size_t>(100000, 8 * list.live);
    list.collecting = false;
    return rings.size();
}

Value Value::container(Kind kind) {
    Containers& list = containers();
    if (++list.created >= list.threshold) runtime::collectCycles();
    Object::Kind objectKind = kind == Kind::Array    ? Object::Kind::Array
                            : kind == Kind::Map      ? Object::Kind::Map
                            : kind == Kind::Function ? Object::Kind::Function
                            : kind == Kind::Box      ? Object::Kind::Box
                                                     : Object::Kind::Struct;
    Value v;
    Object* object = new Object(objectKind);
    if (list.all.size() >= 2 * list.live + 4096) list.pack();
    object->gcIndex = static_cast<std::uint32_t>(list.all.size());
    list.all.push_back(object);
    ++list.live;
    v.data_.object = object;
    v.data_.object->refs = 1;
    v.kind_ = kind;
    return v;
}

std::string Value::text() const {
    switch (kind_) {
        case Kind::Int: return std::to_string(data_.integer);
        case Kind::Float: return runtime::formatNumber(data_.real);
        case Kind::Bool: return data_.boolean ? "true" : "false";
        case Kind::String: return data_.string->text;
        default: return "";
    }
}

const std::string& Value::nameOf(Kind kind) {
    static const std::string names[] = {"null", "int", "float", "bool", "string", "array", "map", "struct", "func", "box"};
    return names[static_cast<int>(kind)];
}

const std::string& Value::typeName() const {
    if (kind_ == Kind::Struct && data_.object->structType) return data_.object->structType->name;
    if (kind_ == Kind::Box) return data_.object->items[0].typeName();
    return nameOf(kind_);
}

std::ostream& operator<<(std::ostream& out, const Value& value) { return out << runtime::display(value); }

long Object::find(const std::string& key) const {
    constexpr size_t scanned = 8;
    if (keys.size() <= scanned) {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == key) return static_cast<long>(i);
        return -1;
    }
    if (!index_) index_ = std::make_unique<std::unordered_map<std::string, size_t>>();
    if (index_->size() != keys.size()) {
        index_->clear();
        index_->reserve(keys.size());
        for (size_t i = 0; i < keys.size(); ++i) index_->emplace(keys[i], i);
    }
    auto found = index_->find(key);
    return found == index_->end() ? -1 : static_cast<long>(found->second);
}

Value& Object::slot(const std::string& key) {
    long at = find(key);
    if (at >= 0) return items[static_cast<size_t>(at)];
    if (index_ && !index_->empty()) index_->emplace(key, keys.size());
    keys.push_back(key);
    items.emplace_back();
    return items.back();
}

bool Object::erase(const std::string& key) {
    long at = find(key);
    if (at < 0) return false;
    keys.erase(keys.begin() + at);
    items.erase(items.begin() + at);
    if (index_) index_->clear(); // the keys after it moved; rebuilt on the next lookup
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
            for (size_t i = names.size(); i-- > 0;) {
                if (names[i] != name) continue;
                Value& slot = scope->slots[i];
                // A variable a lambda captured lives in a box.
                Value* held = slot.is(Value::Kind::Box) ? &slot.ref()->items[0] : &slot;
                // An empty slot is not declared yet, unless it is a program variable holding null.
                if (!slot.isVoid()) return held;
                auto program = scope->programGlobals.find(name);
                if (program != scope->programGlobals.end() && program->second == held) return held;
            }
        }
    }
    return nullptr;
}

std::vector<Value>& Context::arrayOf(const Value& value, const std::string& what) {
    if (!value.is(Value::Kind::Array))
        throw std::runtime_error("Type Error: " + what + " must be an array, got '" + value.typeName() + "'");
    return value.ref()->items;
}

namespace runtime {

Value makeArray(std::vector<Value> items) {
    Value array = Value::container(Value::Kind::Array);
    array.ref()->items = std::move(items);
    return array;
}

Value makeMap() { return Value::container(Value::Kind::Map); }

namespace {

// A container that holds itself (a[0] = a) is copied once, not forever.
Value copyOf(const Value& value, std::map<const Object*, Value>& copies) {
    const Object* original = value.ref();
    if (!original || value.isFunction() || original->frozen) return value; // a function or an enum value is not copied
    auto done = copies.find(original);
    if (done != copies.end()) return done->second;
    Value result = Value::container(value.kind());
    Object* copy = result.ref();
    copies[original] = result;
    copy->keys = original->keys;
    copy->elementType = original->elementType;
    copy->structType = original->structType;
    copy->items.reserve(original->items.size());
    for (const auto& item : original->items) copy->items.push_back(copyOf(item, copies));
    return result;
}

// Two containers that are being compared already, higher up, count as equal there.
bool equalTo(const Value& a, const Value& b, std::vector<std::pair<const Object*, const Object*>>& open) {
    if (a.kind() != b.kind()) return false;
    switch (a.kind()) {
        case Value::Kind::Void: return true;
        case Value::Kind::Int: return a.asInt() == b.asInt();
        case Value::Kind::Float: return a.asFloat() == b.asFloat();
        case Value::Kind::Bool: return a.asBool() == b.asBool();
        case Value::Kind::String: return a.str() == b.str();
        default: break;
    }
    const Object* left = a.ref();
    const Object* right = b.ref();
    if (left == right) return true;
    if (a.isFunction()) {
        // The same function, or the same lambda code with the same captured variables.
        const Callee& x = *left->callee;
        const Callee& y = *right->callee;
        if (x.function || x.builtin) return x.function == y.function && x.builtin == y.builtin;
        if (x.lambda != y.lambda || left->items.size() != right->items.size()) return false;
        for (size_t i = 0; i < left->items.size(); ++i)
            if (left->items[i].ref() != right->items[i].ref()) return false;
        return true;
    }
    if (a.typeName() != b.typeName()) return false;
    std::pair<const Object*, const Object*> pair{left, right};
    if (std::find(open.begin(), open.end(), pair) != open.end()) return true;
    if (left->keys != right->keys || left->items.size() != right->items.size()) return false;
    open.push_back(pair);
    bool equal = true;
    for (size_t i = 0; equal && i < left->items.size(); ++i) equal = equalTo(left->items[i], right->items[i], open);
    open.pop_back();
    return equal;
}

} // namespace

Value deepCopy(const Value& value) {
    std::map<const Object*, Value> copies;
    return copyOf(value, copies);
}

bool deepEqual(const Value& a, const Value& b) {
    std::vector<std::pair<const Object*, const Object*>> open;
    return equalTo(a, b, open);
}

} // namespace runtime

void Context::defineFunc(const std::string& name, std::shared_ptr<Node> func) {
    auto& slot = functions[name];
    auto* replaced = dynamic_cast<FuncDefNode*>(slot.get());
    auto* replacement = dynamic_cast<FuncDefNode*>(func.get());
    if (replaced && (!replacement || replaced->body != replacement->body)) retired.push_back(std::move(slot));
    slot = std::move(func);
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

void Context::defineVar(const std::string& name, const std::string& /*type*/, const Value& value) {
    variables[name] = value;
}

namespace runtime {
void assign(Value& target, Value val, const std::string& name) {
    // The usual case, a value of the variable's own type, needs no conversion.
    bool sameType = target.kind() == val.kind() &&
                    (!val.is(Value::Kind::Struct) || target.typeName() == val.typeName());
    if (!sameType) coerce(target.typeName(), val, "variable '" + name + "'");
    // Arrays, maps and structs are shared: the variable names the same container.
    target = std::move(val);
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
