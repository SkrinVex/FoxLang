#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>

namespace foxlang {
namespace lsp {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    JsonType type = JsonType::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;

    JsonValue() : type(JsonType::Null) {}
    JsonValue(std::nullptr_t) : type(JsonType::Null) {}
    JsonValue(bool b) : type(JsonType::Boolean), boolValue(b) {}
    JsonValue(int n) : type(JsonType::Number), numberValue(n) {}
    JsonValue(int64_t n) : type(JsonType::Number), numberValue(static_cast<double>(n)) {}
    JsonValue(size_t n) : type(JsonType::Number), numberValue(static_cast<double>(n)) {}
    JsonValue(double d) : type(JsonType::Number), numberValue(d) {}
    JsonValue(const char* s) : type(JsonType::String), stringValue(s ? s : "") {}
    JsonValue(std::string s) : type(JsonType::String), stringValue(std::move(s)) {}
    JsonValue(std::vector<JsonValue> arr) : type(JsonType::Array), arrayValue(std::move(arr)) {}
    JsonValue(std::map<std::string, JsonValue> obj) : type(JsonType::Object), objectValue(std::move(obj)) {}

    bool isNull() const { return type == JsonType::Null; }
    bool isBool() const { return type == JsonType::Boolean; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isString() const { return type == JsonType::String; }
    bool isArray() const { return type == JsonType::Array; }
    bool isObject() const { return type == JsonType::Object; }

    static JsonValue object() { return JsonValue(std::map<std::string, JsonValue>{}); }
    static JsonValue array() { return JsonValue(std::vector<JsonValue>{}); }

    void set(const std::string& key, JsonValue val) {
        if (!isObject()) {
            type = JsonType::Object;
            objectValue.clear();
        }
        objectValue[key] = std::move(val);
    }

    std::string asString(const std::string& def = "") const {
        return isString() ? stringValue : def;
    }
    int asInt(int def = 0) const {
        return isNumber() ? static_cast<int>(numberValue) : def;
    }
    bool asBool(bool def = false) const {
        return isBool() ? boolValue : def;
    }
    double asDouble(double def = 0.0) const {
        return isNumber() ? numberValue : def;
    }
    const std::vector<JsonValue>& asArray() const {
        return arrayValue;
    }
    const std::map<std::string, JsonValue>& asObject() const {
        return objectValue;
    }

    bool has(const std::string& key) const {
        return isObject() && objectValue.find(key) != objectValue.end();
    }

    const JsonValue& get(const std::string& key) const {
        static const JsonValue nullVal;
        if (!isObject()) return nullVal;
        auto it = objectValue.find(key);
        return it != objectValue.end() ? it->second : nullVal;
    }

    JsonValue& operator[](const std::string& key) {
        if (!isObject()) {
            type = JsonType::Object;
            objectValue.clear();
        }
        return objectValue[key];
    }

    const JsonValue& operator[](const std::string& key) const {
        return get(key);
    }

    const JsonValue& operator[](size_t idx) const {
        static const JsonValue nullVal;
        if (!isArray() || idx >= arrayValue.size()) return nullVal;
        return arrayValue[idx];
    }

    std::string serialize() const;
    static JsonValue parse(const std::string& text);

private:
    static JsonValue parseValue(const std::string& s, size_t& i);
    static JsonValue parseString(const std::string& s, size_t& i);
    static JsonValue parseNumber(const std::string& s, size_t& i);
    static JsonValue parseArray(const std::string& s, size_t& i);
    static JsonValue parseObject(const std::string& s, size_t& i);
    static void skipWhitespace(const std::string& s, size_t& i);
    static std::string escapeString(const std::string& s);
};

} // namespace lsp
} // namespace foxlang
