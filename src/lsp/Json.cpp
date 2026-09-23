#include "Json.h"
#include <cctype>
#include <iomanip>
#include <stdexcept>

namespace foxlang {
namespace lsp {

void JsonValue::skipWhitespace(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
        i++;
    }
}

std::string JsonValue::escapeString(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::ostringstream ss;
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    out += ss.str();
                } else {
                    out += c;
                }
                break;
        }
    }
    out += "\"";
    return out;
}

std::string JsonValue::serialize() const {
    switch (type) {
        case JsonType::Null: return "null";
        case JsonType::Boolean: return boolValue ? "true" : "false";
        case JsonType::Number: {
            if (numberValue == static_cast<int64_t>(numberValue)) {
                return std::to_string(static_cast<int64_t>(numberValue));
            }
            std::ostringstream ss;
            ss << numberValue;
            return ss.str();
        }
        case JsonType::String: return escapeString(stringValue);
        case JsonType::Array: {
            std::string out = "[";
            for (size_t i = 0; i < arrayValue.size(); i++) {
                if (i > 0) out += ",";
                out += arrayValue[i].serialize();
            }
            out += "]";
            return out;
        }
        case JsonType::Object: {
            std::string out = "{";
            bool first = true;
            for (const auto& pair : objectValue) {
                if (!first) out += ",";
                first = false;
                out += escapeString(pair.first) + ":" + pair.second.serialize();
            }
            out += "}";
            return out;
        }
    }
    return "null";
}

JsonValue JsonValue::parse(const std::string& text) {
    size_t i = 0;
    skipWhitespace(text, i);
    if (i >= text.size()) return JsonValue();
    return parseValue(text, i);
}

JsonValue JsonValue::parseValue(const std::string& s, size_t& i) {
    skipWhitespace(s, i);
    if (i >= s.size()) return JsonValue();

    char c = s[i];
    if (c == 'n' && s.compare(i, 4, "null") == 0) {
        i += 4;
        return JsonValue();
    }
    if (c == 't' && s.compare(i, 4, "true") == 0) {
        i += 4;
        return JsonValue(true);
    }
    if (c == 'f' && s.compare(i, 5, "false") == 0) {
        i += 5;
        return JsonValue(false);
    }
    if (c == '"') {
        return parseString(s, i);
    }
    if (c == '[') {
        return parseArray(s, i);
    }
    if (c == '{') {
        return parseObject(s, i);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
        return parseNumber(s, i);
    }

    throw std::runtime_error(std::string("JSON parse error: unexpected character '") + c + "'");
}

JsonValue JsonValue::parseString(const std::string& s, size_t& i) {
    i++; // Skip opening quote
    std::string out;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') {
            return JsonValue(out);
        }
        if (c == '\\' && i < s.size()) {
            char esc = s[i++];
            switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 <= s.size()) {
                        std::string hexStr = s.substr(i, 4);
                        i += 4;
                        try {
                            unsigned int cp = static_cast<unsigned int>(std::stoul(hexStr, nullptr, 16));
                            if (cp < 0x80) {
                                out += static_cast<char>(cp);
                            } else if (cp < 0x800) {
                                out += static_cast<char>(0xC0 | (cp >> 6));
                                out += static_cast<char>(0x80 | (cp & 0x3F));
                            } else {
                                out += static_cast<char>(0xE0 | (cp >> 12));
                                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                out += static_cast<char>(0x80 | (cp & 0x3F));
                            }
                        } catch (...) {}
                    }
                    break;
                }
                default: out += esc; break;
            }
        } else {
            out += c;
        }
    }
    return JsonValue(out);
}

JsonValue JsonValue::parseNumber(const std::string& s, size_t& i) {
    size_t start = i;
    if (s[i] == '-') i++;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
    if (i < s.size() && s[i] == '.') {
        i++;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
    }
    double val = std::stod(s.substr(start, i - start));
    return JsonValue(val);
}

JsonValue JsonValue::parseArray(const std::string& s, size_t& i) {
    i++; // Skip '['
    std::vector<JsonValue> arr;
    skipWhitespace(s, i);
    if (i < s.size() && s[i] == ']') {
        i++;
        return JsonValue(arr);
    }

    while (i < s.size()) {
        arr.push_back(parseValue(s, i));
        skipWhitespace(s, i);
        if (i < s.size() && s[i] == ',') {
            i++;
            continue;
        }
        if (i < s.size() && s[i] == ']') {
            i++;
            break;
        }
    }
    return JsonValue(arr);
}

JsonValue JsonValue::parseObject(const std::string& s, size_t& i) {
    i++; // Skip '{'
    std::map<std::string, JsonValue> obj;
    skipWhitespace(s, i);
    if (i < s.size() && s[i] == '}') {
        i++;
        return JsonValue(obj);
    }

    while (i < s.size()) {
        skipWhitespace(s, i);
        if (i >= s.size() || s[i] != '"') break;
        JsonValue keyVal = parseString(s, i);
        skipWhitespace(s, i);
        if (i < s.size() && s[i] == ':') i++;
        JsonValue val = parseValue(s, i);
        obj[keyVal.asString()] = std::move(val);
        skipWhitespace(s, i);
        if (i < s.size() && s[i] == ',') {
            i++;
            continue;
        }
        if (i < s.size() && s[i] == '}') {
            i++;
            break;
        }
    }
    return JsonValue(obj);
}

} // namespace lsp
} // namespace foxlang
