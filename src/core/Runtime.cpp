#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace foxlang {
namespace runtime {

// Shortest text that reads back as the same double. std::to_string keeps only six
// decimals, so every float operation used to lose precision when its result was stored.
std::string formatNumber(double val) {
    if (!std::isfinite(val)) throw std::runtime_error("Runtime Error: float result is not a finite number");
    char buffer[40];
    // 15 significant digits is the widest precision that still prints short values short,
    // so at most three attempts are needed to find the shortest exact text.
    for (int digits = 15; digits < 17; ++digits) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", digits, val);
        if (std::strtod(buffer, nullptr) == val) return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "%.17g", val);
    return buffer;
}

StackGuard& stackGuard() {
    static thread_local StackGuard state;
    return state;
}

bool tryNumber(const Value& value, double& out) {
    if (value.value.isInteger()) { out = static_cast<double>(value.value.integerValue()); return true; }
    if (value.value.isReal()) { out = value.value.realValue(); return true; }
    const std::string& text = value.value.str();
    char* end = nullptr;
    double result = std::strtod(text.c_str(), &end);
    if (text.empty() || end != text.c_str() + text.size() || !std::isfinite(result)) return false;
    out = result;
    return true;
}

bool tryInt(const Value& value, long long& out) {
    if (value.value.isInteger()) {
        long long stored = value.value.integerValue();
        if (stored < -2147483648LL || stored > 2147483647LL) return false;
        out = stored;
        return true;
    }
    // A real narrows through toInt, which truncates it, exactly as its text would.
    if (value.value.isReal()) return false;
    const std::string& text = value.value.str();
    char* end = nullptr;
    long long result = std::strtoll(text.c_str(), &end, 10);
    if (text.empty() || end != text.c_str() + text.size() ||
        result < -2147483648LL || result > 2147483647LL) return false;
    out = result;
    return true;
}

double toNumber(const Value& value, const std::string& what) {
    double result = 0;
    if (!tryNumber(value, result))
        throw std::runtime_error("Type Error: " + what + " is not a number: '" + value.value + "'");
    return result;
}

Text realResult(double result) {
    if (!std::isfinite(result)) throw std::runtime_error("Runtime Error: float result is not a finite number");
    return Text::real(result);
}

int toInt(const Value& value, const std::string& what) {
    double number = toNumber(value, what);
    if (number < -2147483648.0 || number > 2147483647.0)
        throw std::runtime_error("Runtime Error: " + what + " does not fit in int: '" + value.value +
                                 "' (int holds -2147483648..2147483647)");
    return static_cast<int>(number);
}

Text intText(const Value& value, const std::string& what) {
    return Text::integer(toInt(value, what));
}

Text intResult(long long result, const std::string& op) {
    if (result < -2147483648LL || result > 2147483647LL)
        throw std::runtime_error("Runtime Error: int overflow in '" + op + "': result " + std::to_string(result) +
                                 " is outside -2147483648..2147483647");
    return Text::integer(result);
}

void coerce(const std::string& type, Value& value, const std::string& what) {
    if (type == value.type) {
        if (type == "int") {
            long long probe = 0;
            if (!tryInt(value, probe)) value.value = intText(value, what);
        }
        return;
    }
    bool numeric = value.type == "int" || value.type == "float";
    if (type == "float" && numeric) {
        value.type = "float";
    } else if (type == "int" && numeric) {
        value.type = "int";
        value.value = intText(value, what);
    } else if (type == "string" && (numeric || value.type == "bool")) {
        value.type = "string";
    } else {
        throw std::runtime_error("Type Error: " + what + " has type '" + type + "' and cannot hold a value of type '" +
                                 value.type + "'");
    }
}

int getLogLevelThreshold() {
    std::string raw = platform::getEnvVar("FOXLANG_LOG_LEVEL");
    std::string level = !raw.empty() ? raw : "info";
    std::transform(level.begin(), level.end(), level.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    if (level == "debug") return 10;
    if (level == "info") return 20;
    if (level == "warn" || level == "warning") return 30;
    if (level == "error") return 40;
    if (level == "off" || level == "none") return 99;
    return 20;
}

static int hexVal(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static void appendUtf8(std::string& out, unsigned cp) {
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

Value jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char code[8];
                    std::snprintf(code, sizeof(code), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(ch)));
                    out += code;
                } else {
                    out += ch;
                }
        }
    }
    return {"string", out};
}

namespace {

// Walks a JSON document without building a tree. Every method leaves pos just past
// what it consumed and reports malformed input by returning false.
class JsonCursor {
public:
    explicit JsonCursor(const std::string& text) : json(text) {}
    const std::string& json;
    size_t pos = 0;

    void whitespace() {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    }
    bool at(char ch) {
        whitespace();
        return pos < json.size() && json[pos] == ch;
    }

    bool string(std::string* out) {
        if (!at('"')) return false;
        ++pos;
        while (pos < json.size()) {
            char ch = json[pos++];
            if (ch == '"') return true;
            if (ch != '\\') {
                if (out) *out += ch;
                continue;
            }
            if (pos >= json.size()) return false;
            ch = json[pos++];
            if (!out) {
                if (ch == 'u') pos += 4;
                continue;
            }
            switch (ch) {
                case 'n': *out += '\n'; break;
                case 'r': *out += '\r'; break;
                case 't': *out += '\t'; break;
                case 'b': *out += '\b'; break;
                case 'f': *out += '\f'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex(cp)) return false;
                    // A high surrogate followed by a low one encodes a single code point (emoji).
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos + 1 < json.size() && json[pos] == '\\' && json[pos + 1] == 'u') {
                        size_t mark = pos;
                        pos += 2;
                        unsigned low = 0;
                        if (hex(low) && low >= 0xDC00 && low <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        else pos = mark;
                    }
                    appendUtf8(*out, cp);
                    break;
                }
                default: *out += ch;
            }
        }
        return false;
    }

    bool value() {
        whitespace();
        if (pos >= json.size()) return false;
        char ch = json[pos];
        if (ch == '"') return string(nullptr);
        if (ch == '{' || ch == '[') {
            char close = ch == '{' ? '}' : ']';
            ++pos;
            if (at(close)) { ++pos; return true; }
            while (true) {
                if (close == '}') {
                    if (!string(nullptr) || !at(':')) return false;
                    ++pos;
                }
                if (!value()) return false;
                if (at(',')) { ++pos; continue; }
                if (at(close)) { ++pos; return true; }
                return false;
            }
        }
        size_t start = pos;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']' &&
               !std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        return pos > start;
    }

    // Moves to the value at the path; false when the path does not exist.
    bool locate(const std::string& path) {
        std::string normalized = path;
        for (auto& ch : normalized) if (ch == '[') ch = '.';
        normalized.erase(std::remove(normalized.begin(), normalized.end(), ']'), normalized.end());
        std::stringstream segments(normalized);
        std::string segment;
        while (std::getline(segments, segment, '.')) {
            if (segment.empty()) continue;
            if (at('{')) {
                ++pos;
                bool found = false;
                while (!at('}')) {
                    std::string key;
                    if (!string(&key) || !at(':')) return false;
                    ++pos;
                    if (key == segment) { found = true; break; }
                    if (!value()) return false;
                    if (at(',')) ++pos;
                    else if (!at('}')) return false;
                }
                if (!found) return false;
            } else if (at('[')) {
                if (segment.find_first_not_of("0123456789") != std::string::npos || segment.size() > 9) return false;
                size_t index = std::stoul(segment);
                ++pos;
                for (size_t i = 0; i < index; ++i) {
                    if (at(']') || !value()) return false;
                    if (!at(',')) return false;
                    ++pos;
                }
                if (at(']')) return false;
            } else {
                return false;
            }
        }
        whitespace();
        return pos < json.size();
    }

    int count() {
        whitespace();
        if (pos >= json.size() || (json[pos] != '{' && json[pos] != '[')) return -1;
        char close = json[pos] == '{' ? '}' : ']';
        ++pos;
        int items = 0;
        if (at(close)) return 0;
        while (true) {
            if (close == '}') {
                if (!string(nullptr) || !at(':')) return -1;
                ++pos;
            }
            if (!value()) return -1;
            ++items;
            if (at(',')) { ++pos; continue; }
            return at(close) ? items : -1;
        }
    }

private:
    bool hex(unsigned& cp) {
        if (pos + 4 > json.size()) return false;
        for (int i = 0; i < 4; ++i) {
            int digit = hexVal(json[pos + static_cast<size_t>(i)]);
            if (digit < 0) return false;
            cp = (cp << 4) | static_cast<unsigned>(digit);
        }
        pos += 4;
        return true;
    }
};

} // namespace

Value jsonGet(const std::string& json, const std::string& path) {
    JsonCursor cursor(json);
    if (!cursor.locate(path)) return {"string", ""};
    if (json[cursor.pos] == '"') {
        std::string decoded;
        if (!cursor.string(&decoded)) return {"string", ""};
        return {"string", decoded};
    }
    size_t start = cursor.pos;
    if (!cursor.value()) return {"string", ""};
    return {"string", json.substr(start, cursor.pos - start)};
}

int jsonCount(const std::string& json, const std::string& path) {
    JsonCursor cursor(json);
    if (!cursor.locate(path)) return -1;
    return cursor.count();
}

std::string jsonType(const std::string& json, const std::string& path) {
    JsonCursor cursor(json);
    if (!cursor.locate(path)) return "";
    switch (json[cursor.pos]) {
        case '{': return "object";
        case '[': return "array";
        case '"': return "string";
        case 't': case 'f': return "bool";
        case 'n': return "null";
        default: return "number";
    }
}

void loadDotEnv(const std::string& scriptPath) {
    std::string dir = ".";
    size_t slash = scriptPath.find_last_of("/\\");
    if (slash != std::string::npos) dir = scriptPath.substr(0, slash);

    std::ifstream env(dir + "/.env");
    if (!env.is_open()) env.open(".env");
    if (!env.is_open()) return;

    std::string line;
    while (std::getline(env, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = line.substr(7);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        size_t ks = key.find_first_not_of(" \t");
        if (ks != std::string::npos) key = key.substr(ks);

        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        if (!key.empty() && platform::getEnvVar(key).empty()) {
            platform::setEnvVar(key, value);
        }
    }
}

std::string resolveFoxFile(const std::string& requested, const std::string& currentFile, const std::string& foxHome) {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;
    fs::path req(requested);

    if (req.is_absolute()) candidates.push_back(req);
    if (!currentFile.empty()) {
        size_t found = currentFile.find_last_of("/\\");
        std::string dir = (found == std::string::npos) ? "./" : currentFile.substr(0, found + 1);
        candidates.push_back(fs::path(dir) / req);
    }
    candidates.push_back(req);

    std::string home = foxHome.empty() ? platform::getEnvVar("FOXLANG_HOME") : foxHome;
    if (!home.empty()) {
        candidates.push_back(fs::path(home) / req);
        candidates.push_back(fs::path(home) / "std" / req);
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
            return fs::weakly_canonical(candidate, ec).string();
        }
    }
    throw std::runtime_error("Module Error: File '" + requested + "' not found. Checked current file, working directory and FOXLANG_HOME.");
}

} // namespace runtime
} // namespace foxlang
