#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"
#include "foxlang/AST.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <atomic>
#include <unordered_set>

namespace foxlang {
namespace runtime {

std::string formatNumber(double val) {
    std::string s = std::to_string(val);
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

int getLogLevelThreshold() {
    std::string legacy = platform::getEnvVar("FOXLANG_LOG");
    if (!legacy.empty()) {
        std::string v = legacy;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
        if (v == "0" || v == "false" || v == "off" || v == "no") return 99;
    }
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
    for (char ch : text) {
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += ch;
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else if (ch == '\b') {
            out += "\\b";
        } else if (ch == '\f') {
            out += "\\f";
        } else {
            out += ch;
        }
    }
    return {"string", out};
}

// Token-based robust JSON parser for json_get
Value jsonGet(const std::string& json, const std::string& path) {
    auto skipWhitespace = [&](size_t& p) {
        while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) p++;
    };

    auto parseStringAt = [&](size_t& p) -> std::string {
        skipWhitespace(p);
        if (p >= json.size() || json[p] != '"') return "";
        p++; // skip opening '"'
        std::string out;
        while (p < json.size()) {
            char ch = json[p];
            if (ch == '"') {
                p++;
                break;
            }
            if (ch != '\\') {
                out += ch;
                p++;
                continue;
            }
            p++; // skip backslash
            if (p >= json.size()) break;
            ch = json[p];
            if (ch == 'n') out += '\n';
            else if (ch == 'r') out += '\r';
            else if (ch == 't') out += '\t';
            else if (ch == 'b') out += '\b';
            else if (ch == 'f') out += '\f';
            else if (ch == '"' || ch == '\\' || ch == '/') out += ch;
            else if (ch == 'u' && p + 4 < json.size()) {
                unsigned cp = 0;
                bool ok = true;
                for (int n = 1; n <= 4; n++) {
                    int h = hexVal(json[p + n]);
                    if (h < 0) { ok = false; break; }
                    cp = (cp << 4) | static_cast<unsigned>(h);
                }
                if (ok) {
                    p += 4;
                    // Check for UTF-16 surrogate pair
                    if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 < json.size() && json[p + 1] == '\\' && json[p + 2] == 'u') {
                        unsigned low = 0;
                        bool lok = true;
                        for (int n = 3; n <= 6; n++) {
                            int h = hexVal(json[p + n]);
                            if (h < 0) { lok = false; break; }
                            low = (low << 4) | static_cast<unsigned>(h);
                        }
                        if (lok && low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            p += 6;
                        }
                    }
                    appendUtf8(out, cp);
                } else {
                    out += 'u';
                }
            } else {
                out += ch;
            }
            p++;
        }
        return out;
    };

    auto skipString = [&](size_t& p) {
        if (p >= json.size() || json[p] != '"') return;
        p++;
        while (p < json.size()) {
            if (json[p] == '\\') {
                p += 2;
            } else if (json[p] == '"') {
                p++;
                break;
            } else {
                p++;
            }
        }
    };

    auto skipValue = [&](size_t& p) {
        skipWhitespace(p);
        if (p >= json.size()) return;
        if (json[p] == '"') {
            skipString(p);
        } else if (json[p] == '{' || json[p] == '[') {
            char open = json[p];
            char close = (open == '{') ? '}' : ']';
            int depth = 0;
            while (p < json.size()) {
                if (json[p] == '"') {
                    skipString(p);
                } else {
                    if (json[p] == open) depth++;
                    else if (json[p] == close) {
                        depth--;
                        if (depth == 0) { p++; break; }
                    }
                    p++;
                }
            }
        } else {
            while (p < json.size() && json[p] != ',' && json[p] != '}' && json[p] != ']' && !std::isspace(static_cast<unsigned char>(json[p]))) {
                p++;
            }
        }
    };

    auto extractRawValue = [&](size_t& p) -> std::string {
        skipWhitespace(p);
        if (p >= json.size()) return "";
        if (json[p] == '"') {
            return parseStringAt(p);
        }
        size_t start = p;
        skipValue(p);
        return json.substr(start, p - start);
    };

    std::vector<std::string> pathSegments;
    std::stringstream ps(path);
    std::string seg;
    while (std::getline(ps, seg, '.')) {
        if (!seg.empty()) pathSegments.push_back(seg);
    }
    if (pathSegments.empty()) return {"string", ""};

    size_t pos = 0;
    skipWhitespace(pos);

    for (size_t segIdx = 0; segIdx < pathSegments.size(); segIdx++) {
        skipWhitespace(pos);
        if (pos >= json.size() || json[pos] != '{') return {"string", ""};
        pos++; // skip '{'
        const std::string& targetKey = pathSegments[segIdx];
        bool found = false;

        while (pos < json.size()) {
            skipWhitespace(pos);
            if (pos >= json.size() || json[pos] == '}') {
                pos++;
                break;
            }
            if (json[pos] != '"') {
                pos++;
                continue;
            }
            std::string key = parseStringAt(pos);
            skipWhitespace(pos);
            if (pos < json.size() && json[pos] == ':') pos++; // skip ':'
            skipWhitespace(pos);

            if (key == targetKey) {
                found = true;
                if (segIdx + 1 == pathSegments.size()) {
                    // Final segment: extract value
                    std::string res = extractRawValue(pos);
                    return {"string", res};
                }
                // Intermediate segment: continue navigation inside this object
                break;
            } else {
                skipValue(pos);
                skipWhitespace(pos);
                if (pos < json.size() && json[pos] == ',') pos++;
            }
        }
        if (!found) return {"string", ""};
    }

    return {"string", ""};
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

bool isBuiltin(const std::string& name) {
    static const std::unordered_set<std::string> builtins = {
        "print", "input", "getch", "kbhit", "wait", "round", "random",
        "abs", "min", "max", "clamp", "time_ms", "fox", "size",
        "term_clear", "term_home", "term_write", "term_goto",
        "term_hide_cursor", "term_show_cursor", "term_color", "term_reset",
        "tcp_connect", "tcp_send", "tcp_recv", "tcp_close", "dns_lookup",
        "read_file", "write_file", "append_file",
        "http_get", "httpget", "httppost", "httpput", "httpdelete",
        "log_debug", "log_info", "log_warn", "log_error",
        "env_get", "env_required", "env_default", "json_get", "json_escape",
        "str_contains", "str_replace", "str_split", "str_to_int",
        "route_get", "route_post", "request_body", "request_method", "request_path",
        "send_response", "server_start", "server_stop", "get"
    };
    return builtins.count(name) > 0;
}

Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& ctx) {
    if (name == "print") {
        if (args.empty()) {
            std::cout << std::endl;
        } else {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].value;
            }
            std::cout << std::endl;
        }
        return {"void", ""};
    }

    if (name == "input") {
        if (args.size() == 1) {
            std::cout << args[0].value << std::flush;
        }
        std::string input;
        std::getline(std::cin, input);
        return {"string", input};
    }

    if (name == "getch" && args.empty()) {
        return {"string", platform::getch()};
    }

    if (name == "kbhit" && args.empty()) {
        return {"bool", platform::kbhit() ? "true" : "false"};
    }

    if (name == "wait" && args.size() == 1) {
        int milliseconds = std::stoi(args[0].value);
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return {"void", ""};
    }

    if (name == "round" && args.size() == 1) {
        double val = std::stod(args[0].value);
        return {"int", std::to_string(static_cast<int>(std::round(val)))};
    }

    if (name == "random" && args.size() == 2) {
        int min = std::stoi(args[0].value);
        int max = std::stoi(args[1].value);
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(min, max);
        return {"int", std::to_string(dis(gen))};
    }

    if (name == "abs" && args.size() == 1) {
        const Value& v = args[0];
        double n = std::stod(v.value);
        if (v.type == "int") return {"int", std::to_string(std::abs(static_cast<int>(n)))};
        return {"float", formatNumber(std::fabs(n))};
    }

    if ((name == "min" || name == "max") && args.size() == 2) {
        const Value& a = args[0];
        const Value& b = args[1];
        double av = std::stod(a.value), bv = std::stod(b.value);
        double out = (name == "min") ? std::min(av, bv) : std::max(av, bv);
        if (a.type == "int" && b.type == "int") return {"int", std::to_string(static_cast<int>(out))};
        return {"float", formatNumber(out)};
    }

    if (name == "clamp" && args.size() == 3) {
        double v = std::stod(args[0].value);
        double lo = std::stod(args[1].value);
        double hi = std::stod(args[2].value);
        if (lo > hi) std::swap(lo, hi);
        return {"float", formatNumber(std::max(lo, std::min(v, hi)))};
    }

    if (name == "time_ms" && args.empty()) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return {"string", std::to_string(now)};
    }

    // Terminal primitives
    if (name == "term_clear" && args.empty()) { std::cout << "\033[2J\033[H" << std::flush; return {"void", ""}; }
    if (name == "term_home" && args.empty()) { std::cout << "\033[H" << std::flush; return {"void", ""}; }
    if (name == "term_write" && args.size() == 1) { std::cout << args[0].value << std::flush; return {"void", ""}; }
    if (name == "term_goto" && args.size() == 2) {
        int row = std::stoi(args[0].value), col = std::stoi(args[1].value);
        std::cout << "\033[" << row << ";" << col << "H" << std::flush;
        return {"void", ""};
    }
    if (name == "term_hide_cursor" && args.empty()) { std::cout << "\033[?25l" << std::flush; return {"void", ""}; }
    if (name == "term_show_cursor" && args.empty()) { std::cout << "\033[?25h" << std::flush; return {"void", ""}; }
    if (name == "term_color" && args.size() == 1) { std::cout << "\033[" << args[0].value << "m" << std::flush; return {"void", ""}; }
    if (name == "term_reset" && args.empty()) { std::cout << "\033[0m" << std::flush; return {"void", ""}; }

    // POSIX TCP primitives
    if (name == "tcp_connect" && args.size() == 2) {
        int port = std::stoi(args[1].value);
        int fd = platform::tcpConnect(args[0].value, port);
        return {"int", std::to_string(fd)};
    }
    if (name == "tcp_send" && args.size() == 2) {
        int fd = std::stoi(args[0].value);
        int sent = platform::tcpSend(fd, args[1].value);
        return {"int", std::to_string(sent)};
    }
    if (name == "tcp_recv" && args.size() == 2) {
        int fd = std::stoi(args[0].value);
        int maxBytes = std::stoi(args[1].value);
        return {"string", platform::tcpRecv(fd, maxBytes)};
    }
    if (name == "tcp_close" && args.size() == 1) {
        int fd = std::stoi(args[0].value);
        return {"bool", platform::tcpClose(fd) ? "true" : "false"};
    }
    if (name == "dns_lookup" && args.size() == 1) {
        return {"string", platform::dnsLookup(args[0].value)};
    }

    if (name == "fox" && args.empty()) {
        std::cout << "FoxLang" << std::endl;
        return {"void", ""};
    }

    if (name == "read_file" && args.size() == 1) {
        if (args[0].type != "string") throw std::runtime_error("read_file() requires string filename");
        std::ifstream file(args[0].value);
        if (!file.is_open()) return {"string", ""};
        std::string content, line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        return {"string", content};
    }

    if (name == "write_file" && args.size() == 2) {
        if (args[0].type != "string" || args[1].type != "string") {
            throw std::runtime_error("write_file() requires string filename and content");
        }
        std::ofstream file(args[0].value);
        if (file.is_open()) {
            file << args[1].value;
            return {"bool", "true"};
        }
        return {"bool", "false"};
    }

    if (name == "append_file" && args.size() == 2) {
        if (args[0].type != "string" || args[1].type != "string") {
            throw std::runtime_error("append_file() requires string filename and content");
        }
        std::ofstream file(args[0].value, std::ios_base::app);
        if (file.is_open()) {
            file << args[1].value;
            return {"bool", "true"};
        }
        return {"bool", "false"};
    }

    if (name == "size" && args.size() == 1) {
        const Value& val = args[0];
        if (val.type == "string") {
            return {"int", std::to_string(val.value.length())};
        }
        if (val.type == "array") {
            return {"int", std::to_string(ctx.getRoot()->arrays[val.value].size())};
        }
        throw std::runtime_error("size() requires array or string");
    }

    // HTTP client primitives
    if (name == "http_get" && args.size() == 1) {
        if (args[0].type != "string") throw std::runtime_error("http_get() requires string URL");
        std::string cmd = "curl -s " + platform::shellQuote(args[0].value);
        std::string result;
        platform::executeCommandCapture(cmd, result);
        return {"string", result};
    }

    if (name == "httpget" && args.size() == 1) {
        if (args[0].type != "string") throw std::runtime_error("httpget() requires string URL");
        std::string cmd = "curl -sS --fail-with-body --connect-timeout 10 --max-time 35 " +
                          platform::shellQuote(args[0].value) + " 2>&1";
        std::string result;
        int status = platform::executeCommandCapture(cmd, result);
        if (status != 0) {
            std::cerr << "[HTTP ERROR] GET " << args[0].value << " (curl exit code " << status << ")\n"
                      << result << std::endl;
            return {"string", ""};
        }
        return {"string", result};
    }

    if (name == "httppost" && args.size() >= 2) {
        std::string contentType = args.size() > 2 ? args[2].value : "application/json";
        std::string cmd = "curl -sS --fail-with-body --connect-timeout 10 --max-time 35 -X POST -H " +
                          platform::shellQuote("Content-Type: " + contentType) + " --data-binary " +
                          platform::shellQuote(args[1].value) + " " + platform::shellQuote(args[0].value) + " 2>&1";
        std::string result;
        int status = platform::executeCommandCapture(cmd, result);
        if (status != 0) {
            std::cerr << "[HTTP ERROR] POST " << args[0].value << " (curl exit code " << status << ")\n"
                      << result << std::endl;
            return {"string", ""};
        }
        return {"string", result};
    }

    if (name == "httpput" && args.size() >= 2) {
        std::string contentType = args.size() > 2 ? args[2].value : "application/json";
        std::string cmd = "curl -s -X PUT -H " + platform::shellQuote("Content-Type: " + contentType) +
                          " -d " + platform::shellQuote(args[1].value) + " " + platform::shellQuote(args[0].value);
        std::string result;
        platform::executeCommandCapture(cmd, result);
        return {"string", result};
    }

    if (name == "httpdelete" && args.size() == 1) {
        std::string cmd = "curl -s -X DELETE " + platform::shellQuote(args[0].value);
        std::string result;
        platform::executeCommandCapture(cmd, result);
        return {"string", result};
    }

    // Logging primitives
    if (name == "log_debug" && args.size() == 1) {
        if (10 >= getLogLevelThreshold()) std::cerr << "[DEBUG] " << args[0].value << std::endl;
        return {"void", ""};
    }
    if (name == "log_info" && args.size() == 1) {
        if (20 >= getLogLevelThreshold()) std::cerr << "[INFO] " << args[0].value << std::endl;
        return {"void", ""};
    }
    if (name == "log_warn" && args.size() == 1) {
        if (30 >= getLogLevelThreshold()) std::cerr << "[WARN] " << args[0].value << std::endl;
        return {"void", ""};
    }
    if (name == "log_error" && args.size() == 1) {
        if (40 >= getLogLevelThreshold()) std::cerr << "[ERROR] " << args[0].value << std::endl;
        return {"void", ""};
    }

    // Environment primitives
    if (name == "env_get" && args.size() == 1) {
        return {"string", platform::getEnvVar(args[0].value)};
    }
    if (name == "env_required" && args.size() == 1) {
        std::string val = platform::getEnvVar(args[0].value);
        if (val.empty()) {
            throw std::runtime_error("Environment Error: required secret '" + args[0].value + "' is not set");
        }
        return {"string", val};
    }
    if (name == "env_default" && args.size() == 2) {
        std::string val = platform::getEnvVar(args[0].value);
        if (val.empty()) {
            return {"string", args[1].value};
        }
        return {"string", val};
    }

    // JSON primitives
    if (name == "json_get" && args.size() == 2) {
        return jsonGet(args[0].value, args[1].value);
    }
    if (name == "json_escape" && args.size() == 1) {
        return jsonEscape(args[0].value);
    }

    // String primitives
    if (name == "str_contains" && args.size() == 2) {
        bool found = args[0].value.find(args[1].value) != std::string::npos;
        return {"bool", found ? "true" : "false"};
    }
    if (name == "str_replace" && args.size() == 3) {
        std::string s = args[0].value;
        const std::string& oldVal = args[1].value;
        const std::string& newVal = args[2].value;
        if (!oldVal.empty()) {
            size_t pos = 0;
            while ((pos = s.find(oldVal, pos)) != std::string::npos) {
                s.replace(pos, oldVal.length(), newVal);
                pos += newVal.length();
            }
        }
        return {"string", s};
    }
    if (name == "str_split" && args.size() == 2) {
        std::vector<Value> result;
        std::string s = args[0].value;
        std::string delim = args[1].value;
        if (delim.empty()) {
            for (char c : s) {
                result.push_back({"string", std::string(1, c)});
            }
        } else {
            size_t pos = 0;
            while ((pos = s.find(delim)) != std::string::npos) {
                result.push_back({"string", s.substr(0, pos)});
                s.erase(0, pos + delim.length());
            }
            result.push_back({"string", s});
        }
        static std::atomic<int> splitCounter{0};
        std::string arrayId = "__split_" + std::to_string(splitCounter++);
        ctx.getRoot()->arrays[arrayId] = result;
        return {"array", arrayId};
    }
    if (name == "str_to_int" && args.size() == 1) {
        try {
            int result = std::stoi(args[0].value);
            return {"int", std::to_string(result)};
        } catch (...) {
            return {"int", "0"};
        }
    }

    // HTTP server primitives
    if (name == "route_get" && args.size() == 2) {
        ctx.getRoot()->variables["__route_GET_" + args[0].value] = {"string", args[1].value};
        return {"void", ""};
    }
    if (name == "route_post" && args.size() == 2) {
        ctx.getRoot()->variables["__route_POST_" + args[0].value] = {"string", args[1].value};
        return {"void", ""};
    }
    if (name == "request_body" && args.empty()) {
        auto root = ctx.getRoot();
        return root->variables.count("__http_body") ? root->variables["__http_body"] : Value{"string", ""};
    }
    if (name == "request_method" && args.empty()) {
        auto root = ctx.getRoot();
        return root->variables.count("__http_method") ? root->variables["__http_method"] : Value{"string", ""};
    }
    if (name == "request_path" && args.empty()) {
        auto root = ctx.getRoot();
        return root->variables.count("__http_path") ? root->variables["__http_path"] : Value{"string", ""};
    }
    if (name == "send_response" && (args.size() == 1 || args.size() == 2)) {
        auto root = ctx.getRoot();
        root->variables["__http_status"] = {"int", args.size() == 2 ? args[0].value : "200"};
        root->variables["__http_response"] = {"string", args.back().value};
        return {"void", ""};
    }
    if (name == "server_start" && args.size() == 1) {
        int port = std::stoi(args[0].value);
        platform::runHttpServer(port, *ctx.getRoot(), nullptr);
        return {"void", ""};
    }
    if (name == "get" && args.size() == 2) {
        if (args[0].type != "array") throw std::runtime_error("Runtime Error: 'get' requires array as first argument");
        int idx = std::stoi(args[1].value);
        auto& arr = ctx.getRoot()->arrays[args[0].value];
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
            throw std::runtime_error("Runtime Error: Array index out of bounds: " + std::to_string(idx));
        }
        return arr[idx];
    }

    if (name == "server_stop" && args.empty()) {
        ctx.getRoot()->variables["__server_stop_requested"] = {"bool", "true"};
        return {"void", ""};
    }

    throw std::runtime_error("Runtime Error: Unknown builtin function '" + name + "' or invalid arguments count " + std::to_string(args.size()));
}

} // namespace runtime
} // namespace foxlang
