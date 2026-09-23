#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef _WIN32
    #include <cstdio>
    #define popen _popen
    #define pclose _pclose
#endif

struct Node;

struct FuncParam {
    std::string type;
    std::string name;
};

struct Value {
    std::string type;
    std::string value;
};

struct Context {
    Context* parent = nullptr;
    std::map<std::string, Value> variables;
    std::map<std::string, std::shared_ptr<Node>> functions;
    std::map<std::string, std::vector<Value>> arrays;

    bool exists(const std::string& name) {
        if (variables.count(name) || arrays.count(name)) return true;
        if (parent) return parent->exists(name);
        return false;
    }

    Value getVar(const std::string& name) {
        if (variables.count(name)) return variables[name];
        if (parent) return parent->getVar(name);
        throw std::runtime_error("Runtime Error: Variable '" + name + "' not found!");
    }

    std::vector<Value>& getArray(const std::string& name) {
        if (arrays.count(name)) return arrays[name];
        if (parent) return parent->getArray(name);
        throw std::runtime_error("Runtime Error: Array '" + name + "' not found!");
    }

    Context* getRoot() {
        Context* curr = this;
        while (curr->parent) curr = curr->parent;
        return curr;
    }

    void defineFunc(const std::string& name, std::shared_ptr<Node> func) {
        functions[name] = func;
    }

    std::shared_ptr<Node> getFunc(const std::string& name) {
        if (functions.count(name)) return functions[name];
        if (parent) return parent->getFunc(name);
        return nullptr;
    }

    void defineVar(const std::string& name, const std::string& type, const Value& value) {
        variables[name] = {type, value.value};
    }

    void setVar(const std::string& name, Value val) {
        if (variables.count(name)) {
            if (variables[name].type != val.type) {
                if (variables[name].type == "float" && val.type == "int") {
                    val.type = "float";
                } else if (variables[name].type == "int" && val.type == "float") {
                    val.type = "int";
                    val.value = std::to_string((int)std::stod(val.value));
                } else {
                    throw std::runtime_error("Type Error: Cannot assign value of type '" + val.type + "' to variable '" + name + "' of type '" + variables[name].type + "'");
                }
            }
            variables[name].value = val.value;
            return;
        }
        if (parent) { parent->setVar(name, val); return; }
        throw std::runtime_error("Error: Variable '" + name + "' not defined!");
    }
};

struct ReturnValue {
    Value value;
};

struct BreakException {};
struct ContinueException {};

static std::string formatNumber(double val) {
    std::string s = std::to_string(val);
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

struct Node {
    virtual ~Node() = default;
    virtual Value eval(Context& ctx) = 0;
};

struct FuncDefNode : Node {
    std::string returnType;
    std::string name;
    std::vector<FuncParam> params;
    std::shared_ptr<Node> body;

    FuncDefNode(std::string rt, std::string n, std::vector<FuncParam> p, std::shared_ptr<Node> b)
        : returnType(rt), name(n), params(p), body(b) {}

    Value eval(Context& ctx) override { return {"void", ""}; }
};

struct ReturnNode : Node {
    std::unique_ptr<Node> expr;
    ReturnNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Value result = expr ? expr->eval(ctx) : Value{"void", ""};
        throw ReturnValue{result};
    }
};

struct FuncCallNode : Node {
    std::string name;
    std::vector<std::unique_ptr<Node>> args;

    FuncCallNode(std::string n, std::vector<std::unique_ptr<Node>> a)
        : name(n), args(std::move(a)) {}

    Value eval(Context& ctx) override {
        // Встроенные функции
        if (name == "print") {
            if (args.empty()) {
                std::cout << std::endl;
            } else {
                for (size_t i = 0; i < args.size(); i++) {
                    if (i > 0) std::cout << " ";
                    std::cout << args[i]->eval(ctx).value;
                }
                std::cout << std::endl;
            }
            return {"void", ""};
        }
        if (name == "input") {
            if (args.size() == 1) {
                // Вывести приглашение
                std::cout << args[0]->eval(ctx).value;
            }
            std::string input;
            std::getline(std::cin, input);
            return {"string", input};
        }
        if (name == "getch" && args.size() == 0) {
#ifdef _WIN32
            return {"string", std::string(1, _getch())};
#else
            struct termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            char ch = getchar();
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return {"string", std::string(1, ch)};
#endif
        }
        if (name == "kbhit" && args.size() == 0) {
#ifdef _WIN32
            return {"bool", _kbhit() ? "true" : "false"};
#else
            struct termios oldt, newt;
            if (tcgetattr(STDIN_FILENO, &oldt) != 0) return {"bool", "false"};
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            timeval tv{0, 0};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            int ready = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return {"bool", ready > 0 ? "true" : "false"};
#endif
        }
        if (name == "wait" && args.size() == 1) {
            int milliseconds = std::stoi(args[0]->eval(ctx).value);
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            return {"void", ""};
        }
        if (name == "round" && args.size() == 1) {
            double val = std::stod(args[0]->eval(ctx).value);
            return {"int", std::to_string((int)std::round(val))};
        }
        if (name == "random" && args.size() == 2) {
            int min = std::stoi(args[0]->eval(ctx).value);
            int max = std::stoi(args[1]->eval(ctx).value);
            static std::random_device rd; static std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(min, max);
            return {"int", std::to_string(dis(gen))};
        }

        if (name == "abs" && args.size() == 1) {
            Value v = args[0]->eval(ctx);
            double n = std::stod(v.value);
            if (v.type == "int") return {"int", std::to_string(std::abs((int)n))};
            return {"float", formatNumber(std::fabs(n))};
        }
        if ((name == "min" || name == "max") && args.size() == 2) {
            Value a = args[0]->eval(ctx);
            Value b = args[1]->eval(ctx);
            double av = std::stod(a.value), bv = std::stod(b.value);
            double out = name == "min" ? std::min(av, bv) : std::max(av, bv);
            if (a.type == "int" && b.type == "int") return {"int", std::to_string((int)out)};
            return {"float", formatNumber(out)};
        }
        if (name == "clamp" && args.size() == 3) {
            double v = std::stod(args[0]->eval(ctx).value);
            double lo = std::stod(args[1]->eval(ctx).value);
            double hi = std::stod(args[2]->eval(ctx).value);
            if (lo > hi) std::swap(lo, hi);
            return {"float", formatNumber(std::max(lo, std::min(v, hi)))};
        }
        if (name == "time_ms" && args.size() == 0) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return {"string", std::to_string(now)};
        }

        // Low-level terminal runtime. Prefer std/terminal.fox in application code.
        if (name == "term_clear" && args.size() == 0) { std::cout << "\033[2J\033[H" << std::flush; return {"void", ""}; }
        if (name == "term_home" && args.size() == 0) { std::cout << "\033[H" << std::flush; return {"void", ""}; }
        if (name == "term_write" && args.size() == 1) { std::cout << args[0]->eval(ctx).value << std::flush; return {"void", ""}; }
        if (name == "term_goto" && args.size() == 2) {
            int row = std::stoi(args[0]->eval(ctx).value), col = std::stoi(args[1]->eval(ctx).value);
            std::cout << "\033[" << row << ";" << col << "H" << std::flush; return {"void", ""};
        }
        if (name == "term_hide_cursor" && args.size() == 0) { std::cout << "\033[?25l" << std::flush; return {"void", ""}; }
        if (name == "term_show_cursor" && args.size() == 0) { std::cout << "\033[?25h" << std::flush; return {"void", ""}; }
        if (name == "term_color" && args.size() == 1) { std::cout << "\033[" << args[0]->eval(ctx).value << "m" << std::flush; return {"void", ""}; }
        if (name == "term_reset" && args.size() == 0) { std::cout << "\033[0m" << std::flush; return {"void", ""}; }

#ifndef _WIN32
        // POSIX TCP client primitives. Sockets are represented as integer handles.
        if (name == "tcp_connect" && args.size() == 2) {
            std::string host = args[0]->eval(ctx).value;
            std::string port = args[1]->eval(ctx).value;
            addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
            addrinfo* res = nullptr;
            if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) return {"int", "-1"};
            int fd = -1;
            for (addrinfo* p = res; p; p = p->ai_next) {
                fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (fd < 0) continue;
                if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
                close(fd); fd = -1;
            }
            freeaddrinfo(res);
            return {"int", std::to_string(fd)};
        }
        if (name == "tcp_send" && args.size() == 2) {
            int fd = std::stoi(args[0]->eval(ctx).value);
            std::string data = args[1]->eval(ctx).value;
            ssize_t sent = send(fd, data.data(), data.size(), 0);
            return {"int", std::to_string(sent < 0 ? -1 : sent)};
        }
        if (name == "tcp_recv" && args.size() == 2) {
            int fd = std::stoi(args[0]->eval(ctx).value);
            int maxBytes = std::max(1, std::stoi(args[1]->eval(ctx).value));
            std::string out(maxBytes, '\0');
            ssize_t n = recv(fd, out.data(), out.size(), 0);
            if (n <= 0) return {"string", ""};
            out.resize((size_t)n);
            return {"string", out};
        }
        if (name == "tcp_close" && args.size() == 1) {
            int fd = std::stoi(args[0]->eval(ctx).value);
            return {"bool", close(fd) == 0 ? "true" : "false"};
        }
        if (name == "dns_lookup" && args.size() == 1) {
            std::string host = args[0]->eval(ctx).value;
            addrinfo hints{}; hints.ai_family = AF_UNSPEC;
            addrinfo* res = nullptr;
            if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) return {"string", ""};
            char buf[INET6_ADDRSTRLEN] = {0};
            std::string out;
            for (addrinfo* p = res; p && out.empty(); p = p->ai_next) {
                void* addr = p->ai_family == AF_INET
                    ? (void*)&((sockaddr_in*)p->ai_addr)->sin_addr
                    : (void*)&((sockaddr_in6*)p->ai_addr)->sin6_addr;
                if (inet_ntop(p->ai_family, addr, buf, sizeof(buf))) out = buf;
            }
            freeaddrinfo(res);
            return {"string", out};
        }
#endif
        if (name == "fox" && args.size() == 0) {
            std::cout << "FoxLang" << std::endl;
            return {"void", ""};
        }
        if (name == "read_file" && args.size() == 1) {
            Value filenameVal = args[0]->eval(ctx);
            if (filenameVal.type != "string") {
                throw std::runtime_error("read_file() requires string filename");
            }

            std::ifstream file(filenameVal.value);
            if (!file.is_open()) {
                return {"string", ""};  // Возвращаем пустую строку при ошибке
            }

            std::string content;
            std::string line;
            while (std::getline(file, line)) {
                content += line + "\n";
            }

            file.close();
            return {"string", content};
        }
        if (name == "write_file" && args.size() == 2) {
            Value filenameVal = args[0]->eval(ctx);
            Value contentVal = args[1]->eval(ctx);
            if (filenameVal.type != "string" || contentVal.type != "string") {
                throw std::runtime_error("write_file() requires string filename and content");
            }
            std::ofstream file(filenameVal.value);
            if (file.is_open()) {
                file << contentVal.value;
                file.close();
                return {"bool", "true"};
            }
            return {"bool", "false"};
        }
        if (name == "append_file" && args.size() == 2) {
            Value filenameVal = args[0]->eval(ctx);
            Value contentVal = args[1]->eval(ctx);
            if (filenameVal.type != "string" || contentVal.type != "string") {
                throw std::runtime_error("append_file() requires string filename and content");
            }
            std::ofstream file(filenameVal.value, std::ios_base::app);
            if (file.is_open()) {
                file << contentVal.value;
                file.close();
                return {"bool", "true"};
            }
            return {"bool", "false"};
        }
        if (name == "size" && args.size() == 1) {
            Value val = args[0]->eval(ctx);
            if (val.type == "string") {
                return {"int", std::to_string(val.value.length())};
            }
            if (val.type == "array") {
                return {"int", std::to_string(ctx.getRoot()->arrays[val.value].size())};
            }
            throw std::runtime_error("size() requires array or string");
        }
        if (name == "http_get" && args.size() == 1) {
            Value urlVal = args[0]->eval(ctx);
            if (urlVal.type != "string") {
                throw std::runtime_error("http_get() requires string URL");
            }

            // Простая реализация через system curl
            std::string cmd = "curl -s \"" + urlVal.value + "\"";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                return {"string", ""};
            }

            std::string result;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);

            return {"string", result};
        }
        if (name == "json_get" && args.size() == 2) {
            Value jsonVal = args[0]->eval(ctx);
            Value keyVal = args[1]->eval(ctx);

            std::string json = jsonVal.value;
            std::string key = keyVal.value;

            // Простой JSON парсер для Telegram API
            if (key == "chat_id") {
                size_t pos = json.find("\"chat\":{\"id\":");
                if (pos != std::string::npos) {
                    pos += 13; // длина "\"chat\":{\"id\":"
                    size_t end = json.find(",", pos);
                    if (end != std::string::npos) {
                        return {"string", json.substr(pos, end - pos)};
                    }
                }
            }
            else if (key == "text") {
                size_t pos = json.find("\"text\":\"");
                if (pos != std::string::npos) {
                    pos += 8; // длина "\"text\":\""
                    size_t end = json.find("\"", pos);
                    if (end != std::string::npos) {
                        return {"string", json.substr(pos, end - pos)};
                    }
                }
            }
            else if (key == "update_id") {
                // Ищем последний update_id в массиве
                size_t lastPos = 0;
                size_t pos = json.find("\"update_id\":");
                while (pos != std::string::npos) {
                    lastPos = pos;
                    pos = json.find("\"update_id\":", pos + 1);
                }

                if (lastPos != 0) {
                    lastPos += 12; // длина "\"update_id\":"
                    size_t end = json.find(",", lastPos);
                    if (end != std::string::npos) {
                        return {"string", json.substr(lastPos, end - lastPos)};
                    }
                }
            }

            return {"string", ""};
        }
        if (name == "str_contains" && args.size() == 2) {
            Value textVal = args[0]->eval(ctx);
            Value substrVal = args[1]->eval(ctx);

            std::string text = textVal.value;
            std::string substr = substrVal.value;

            bool found = text.find(substr) != std::string::npos;
            return {"bool", found ? "true" : "false"};
        }
        if (name == "str_replace" && args.size() == 3) {
            Value strVal = args[0]->eval(ctx);
            Value oldVal = args[1]->eval(ctx);
            Value newVal = args[2]->eval(ctx);
            if (strVal.type != "string" || oldVal.type != "string" || newVal.type != "string") {
                throw std::runtime_error("str_replace() requires string arguments");
            }
            std::string s = strVal.value;
            size_t pos = 0;
            while ((pos = s.find(oldVal.value, pos)) != std::string::npos) {
                s.replace(pos, oldVal.value.length(), newVal.value);
                pos += newVal.value.length();
            }
            return {"string", s};
        }
        if (name == "str_split" && args.size() == 2) {
            Value strVal = args[0]->eval(ctx);
            Value delimVal = args[1]->eval(ctx);
            if (strVal.type != "string" || delimVal.type != "string") {
                throw std::runtime_error("str_split() requires string arguments");
            }
            std::vector<Value> result;
            std::string s = strVal.value;
            std::string delim = delimVal.value;
            size_t pos = 0;
            if (delim.empty()) {
                for (char c : s) {
                    result.push_back({"string", std::string(1, c)});
                }
            } else {
                while ((pos = s.find(delim)) != std::string::npos) {
                    result.push_back({"string", s.substr(0, pos)});
                    s.erase(0, pos + delim.length());
                }
                result.push_back({"string", s});
            }
            static int splitCounter = 0;
            std::string arrayId = "__split_" + std::to_string(splitCounter++);
            ctx.getRoot()->arrays[arrayId] = result;
            return {"array", arrayId};
        }
        if (name == "str_to_int" && args.size() == 1) {
            Value strVal = args[0]->eval(ctx);
            if (strVal.type != "string") {
                return {"int", "0"};
            }

            try {
                int result = std::stoi(strVal.value);
                return {"int", std::to_string(result)};
            } catch (...) {
                return {"int", "0"};
            }
        }
        if (name == "httpget" && args.size() == 1) {
            Value urlVal = args[0]->eval(ctx);
            std::string cmd = "curl -sS --fail-with-body --connect-timeout 10 --max-time 35 \"" + urlVal.value + "\" 2>&1";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                std::cerr << "[HTTP ERROR] Не удалось запустить curl" << std::endl;
                return {"string", ""};
            }

            std::string result;
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
            int status = pclose(pipe);
            if (status != 0) {
                std::cerr << "[HTTP ERROR] GET " << urlVal.value << " (curl status " << status << ")\n"
                          << result << std::endl;
                return {"string", ""};
            }
            return {"string", result};
        }
        if (name == "httppost" && args.size() >= 2) {
            Value urlVal = args[0]->eval(ctx);
            Value dataVal = args[1]->eval(ctx);
            std::string contentType = args.size() > 2 ? args[2]->eval(ctx).value : "application/json";

            std::string cmd = "curl -s -X POST -H \"Content-Type: " + contentType + "\" -d \"" + dataVal.value + "\" \"" + urlVal.value + "\"";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {"string", ""};

            std::string result;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            return {"string", result};
        }
        if (name == "httpput" && args.size() >= 2) {
            Value urlVal = args[0]->eval(ctx);
            Value dataVal = args[1]->eval(ctx);
            std::string contentType = args.size() > 2 ? args[2]->eval(ctx).value : "application/json";

            std::string cmd = "curl -s -X PUT -H \"Content-Type: " + contentType + "\" -d \"" + dataVal.value + "\" \"" + urlVal.value + "\"";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {"string", ""};

            std::string result;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            return {"string", result};
        }
        if (name == "httpdelete" && args.size() == 1) {
            Value urlVal = args[0]->eval(ctx);
            std::string cmd = "curl -s -X DELETE \"" + urlVal.value + "\"";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) return {"string", ""};

            std::string result;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            return {"string", result};
        }

        // FastAPI-подобные функции
        if (name == "server_start" && args.size() == 1) {
            int port = std::stoi(args[0]->eval(ctx).value);

            // Простая заглушка сервера
            std::cout << "HTTP Server started on port " << port << std::endl;
            std::cout << "Note: This is a simulation. Real server implementation requires additional setup." << std::endl;

            return {"string", "Server started on port " + std::to_string(port)};
        }

        if (name == "server_stop" && args.size() == 0) {
            std::cout << "HTTP Server stopped" << std::endl;
            return {"string", "Server stopped"};
        }

        if (name == "route_get" && args.size() == 2) {
            Value pathVal = args[0]->eval(ctx);
            Value handlerVal = args[1]->eval(ctx);

            std::cout << "Registered GET route: " << pathVal.value << " -> " << handlerVal.value << std::endl;
            return {"string", "GET route registered: " + pathVal.value};
        }

        if (name == "route_post" && args.size() == 2) {
            Value pathVal = args[0]->eval(ctx);
            Value handlerVal = args[1]->eval(ctx);

            std::cout << "Registered POST route: " << pathVal.value << " -> " << handlerVal.value << std::endl;
            return {"string", "POST route registered: " + pathVal.value};
        }

        if (name == "send_response" && args.size() == 1) {
            Value responseVal = args[0]->eval(ctx);

            std::cout << "HTTP Response: " << responseVal.value << std::endl;
            return {"void", ""};
        }

        // Пользовательские функции
        auto funcNodeBase = ctx.getFunc(name);
        if (!funcNodeBase) {
            throw std::runtime_error("Runtime Error: Function '" + name + "' not found!");
        }

        FuncDefNode* funcDef = static_cast<FuncDefNode*>(funcNodeBase.get());

        if (args.size() != funcDef->params.size()) {
            throw std::runtime_error("Args count mismatch for '" + name + "'");
        }

        std::vector<Value> argValues;
        for (auto& arg : args) argValues.push_back(arg->eval(ctx));

        Context funcScope;
        funcScope.parent = &ctx;

        for (size_t i = 0; i < funcDef->params.size(); i++) {
            funcScope.defineVar(funcDef->params[i].name, funcDef->params[i].type, argValues[i]);
        }

        try {
            funcDef->body->eval(funcScope);
        } catch (const ReturnValue& ret) {
            return ret.value;
        } catch (const BreakException&) {
            throw std::runtime_error("Runtime Error: 'break' outside of loop in function '" + name + "'");
        } catch (const ContinueException&) {
            throw std::runtime_error("Runtime Error: 'continue' outside of loop in function '" + name + "'");
        }

        return {"void", ""};
    }
};

struct NumberNode : Node {
    std::string val;
    bool isFloat;
    NumberNode(std::string v) : val(v) {
        isFloat = (v.find('.') != std::string::npos);
    }
    Value eval(Context& ctx) override { return {isFloat ? "float" : "int", val}; }
};

struct StringNode : Node {
    std::string val;
    StringNode(std::string v) : val(v) {}
    Value eval(Context& ctx) override { return {"string", val}; }
};

struct BoolNode : Node {
    bool val;
    BoolNode(bool v) : val(v) {}
    Value eval(Context& ctx) override { return {"bool", val ? "true" : "false"}; }
};

struct VarAccessNode : Node {
    std::string name;
    VarAccessNode(std::string n) : name(n) {}
    Value eval(Context& ctx) override { return ctx.getVar(name); }
};

struct VarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e)
        : type(t), name(n), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                val.value = std::to_string((int)std::stod(val.value));
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        ctx.defineVar(name, type, val);
        return {"void", ""};
    }
};

struct GlobalVarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    GlobalVarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e)
        : type(t), name(n), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Context* root = ctx.getRoot();
        Value val = expr->eval(ctx);
        if (type != val.type) {
            if (type == "float" && val.type == "int") {
                val.type = "float";
            } else if (type == "int" && val.type == "float") {
                val.type = "int";
                val.value = std::to_string((int)std::stod(val.value));
            } else if (type == "string") {
                val.type = "string";
            } else {
                throw std::runtime_error("Type Error: Cannot initialize global variable '" + name + "' of type '" + type + "' with value of type '" + val.type + "'");
            }
        }
        root->defineVar(name, type, val);
        return {"void", ""};
    }
};

struct VarAssignNode : Node {
    std::string name;
    std::unique_ptr<Node> expr;
    VarAssignNode(std::string n, std::unique_ptr<Node> e) : name(n), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        ctx.setVar(name, expr->eval(ctx));
        return {"void", ""};
    }
};

struct BinOpNode : Node {
    std::string op;
    std::unique_ptr<Node> left, right;
    BinOpNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}

    Value eval(Context& ctx) override {
        Value lval = left->eval(ctx);
        Value rval = right->eval(ctx);

        if (op == "+" || op == "+=") {
            if (lval.type == "string" || rval.type == "string") {
                return {"string", lval.value + rval.value};
            }
            if (lval.type == "float" || rval.type == "float") {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                return {"float", formatNumber(l + r)};
            }
            int l = std::stoi(lval.value), r = std::stoi(rval.value);
            return {"int", std::to_string(l + r)};
        }

        if (op == "-" || op == "-=" || op == "*" || op == "*=" || op == "/" || op == "/=" || op == "%") {
            if (lval.type == "float" || rval.type == "float") {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                double result = (op == "-" || op == "-=") ? l - r : (op == "*" || op == "*=") ? l * r :
                               (op == "/" || op == "/=") ? l / r : std::fmod(l, r);
                return {"float", formatNumber(result)};
            }
            int l = std::stoi(lval.value), r = std::stoi(rval.value);
            int result = (op == "-" || op == "-=") ? l - r : (op == "*" || op == "*=") ? l * r :
                        (op == "/" || op == "/=") ? l / r : l % r;
            return {"int", std::to_string(result)};
        }

        if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
            bool result;
            if (lval.type == "string" && rval.type == "string") {
                result = (op == "==") ? lval.value == rval.value :
                        (op == "!=") ? lval.value != rval.value :
                        (op == "<") ? lval.value < rval.value :
                        (op == "<=") ? lval.value <= rval.value :
                        (op == ">=") ? lval.value >= rval.value :
                        lval.value > rval.value;
            } else {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                result = (op == "==") ? l == r : (op == "!=") ? l != r :
                        (op == "<") ? l < r :
                        (op == "<=") ? l <= r :
                        (op == ">=") ? l >= r :
                        l > r;
            }
            return {"bool", result ? "true" : "false"};
        }

        if (op == "&&" || op == "||") {
            bool l = (lval.value == "true"), r = (rval.value == "true");
            bool result = (op == "&&") ? l && r : l || r;
            return {"bool", result ? "true" : "false"};
        }

        return {"void", ""};
    }
};

struct UnaryOpNode : Node {
    std::string op;
    std::unique_ptr<Node> operand;
    UnaryOpNode(std::string o, std::unique_ptr<Node> n) : op(o), operand(std::move(n)) {}
    Value eval(Context& ctx) override {
        Value val = operand->eval(ctx);
        if (op == "!") {
            bool b = (val.value == "true");
            return {"bool", b ? "false" : "true"};
        }
        return val;
    }
};

struct PostIncNode : Node {
    std::string name;
    PostIncNode(std::string n) : name(n) {}
    Value eval(Context& ctx) override {
        Value current = ctx.getVar(name);
        int val = std::stoi(current.value);
        ctx.setVar(name, {"int", std::to_string(val + 1)});
        return {"int", std::to_string(val)};
    }
};

struct ArrayDeclNode : Node {
    std::string name;
    std::unique_ptr<Node> sizeNode;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s) : name(n), sizeNode(std::move(s)) {}
    Value eval(Context& ctx) override {
        int sz = std::stoi(sizeNode->eval(ctx).value);
        static int arrayCounter = 0;
        std::string arrayId = "__arr_" + std::to_string(arrayCounter++);
        ctx.getRoot()->arrays[arrayId] = std::vector<Value>(sz, {"int", "0"});
        ctx.defineVar(name, "array", {"array", arrayId});
        return {"void", ""};
    }
};

struct ArraySetNode : Node {
    std::string name;
    std::unique_ptr<Node> index, value;
    ArraySetNode(std::string n, std::unique_ptr<Node> i, std::unique_ptr<Node> v)
        : name(n), index(std::move(i)), value(std::move(v)) {}
    Value eval(Context& ctx) override {
        Value arrVal = ctx.getVar(name);
        if (arrVal.type != "array") throw std::runtime_error("Runtime Error: '" + name + "' is not an array");
        int idx = std::stoi(index->eval(ctx).value);
        ctx.getRoot()->arrays[arrVal.value][idx] = value->eval(ctx);
        return {"void", ""};
    }
};

struct ArrayGetNode : Node {
    std::string name;
    std::unique_ptr<Node> index;
    ArrayGetNode(std::string n, std::unique_ptr<Node> i) : name(n), index(std::move(i)) {}
    Value eval(Context& ctx) override {
        Value arrVal = ctx.getVar(name);
        if (arrVal.type != "array") throw std::runtime_error("Runtime Error: '" + name + "' is not an array");
        int idx = std::stoi(index->eval(ctx).value);
        return ctx.getRoot()->arrays[arrVal.value][idx];
    }
};

struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> stmts;
    Value eval(Context& ctx) override {
        for (auto& stmt : stmts) stmt->eval(ctx);
        return {"void", ""};
    }
};

struct IfNode : Node {
    std::unique_ptr<Node> condition, thenB, elseB;
    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e = nullptr)
        : condition(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
    Value eval(Context& ctx) override {
        bool cond = (condition->eval(ctx).value == "true");
        if (cond) thenB->eval(ctx);
        else if (elseB) elseB->eval(ctx);
        return {"void", ""};
    }
};

struct WhileNode : Node {
    std::unique_ptr<Node> condition, body;
    WhileNode(std::unique_ptr<Node> c, std::unique_ptr<Node> b)
        : condition(std::move(c)), body(std::move(b)) {}
    Value eval(Context& ctx) override {
        while (condition->eval(ctx).value == "true") {
            try {
                body->eval(ctx);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                continue;
            }
        }
        return {"void", ""};
    }
};

struct ForNode : Node {
    std::unique_ptr<Node> init, condition, step, body;
    ForNode(std::unique_ptr<Node> i, std::unique_ptr<Node> c, std::unique_ptr<Node> s, std::unique_ptr<Node> b)
        : init(std::move(i)), condition(std::move(c)), step(std::move(s)), body(std::move(b)) {}
    Value eval(Context& ctx) override {
        if (init) init->eval(ctx);
        while (condition->eval(ctx).value == "true") {
            try {
                body->eval(ctx);
            } catch (const BreakException&) {
                break;
            } catch (const ContinueException&) {
                // continue - выполняем step и продолжаем цикл
            }
            if (step) step->eval(ctx);
        }
        return {"void", ""};
    }
};

struct BreakNode : Node {
    Value eval(Context& ctx) override {
        throw BreakException{};
    }
};

struct ContinueNode : Node {
    Value eval(Context& ctx) override {
        throw ContinueException{};
    }
};

struct WaitNode : Node {
    std::unique_ptr<Node> timeExpr;
    WaitNode(std::unique_ptr<Node> t) : timeExpr(std::move(t)) {}
    Value eval(Context& ctx) override {
        int milliseconds = std::stoi(timeExpr->eval(ctx).value);
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return {"void", ""};
    }
};

struct SwitchNode : Node {
    std::unique_ptr<Node> expr;
    std::vector<std::pair<std::unique_ptr<Node>, std::unique_ptr<Node>>> cases; // value, body
    std::unique_ptr<Node> defaultCase;

    SwitchNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}

    Value eval(Context& ctx) override {
        Value switchValue = expr->eval(ctx);
        bool executed = false;
        bool fallthrough = false;

        for (auto& caseItem : cases) {
            if (!executed && !fallthrough) {
                Value caseValue = caseItem.first->eval(ctx);
                if (switchValue.value == caseValue.value) {
                    executed = true;
                    fallthrough = true;
                }
            }

            if (fallthrough) {
                try {
                    caseItem.second->eval(ctx);
                } catch (const BreakException&) {
                    fallthrough = false;
                    break;
                }
            }
        }

        if (!executed && defaultCase) {
            defaultCase->eval(ctx);
        }

        return {"void", ""};
    }
};

struct InputNode : Node {
    Value eval(Context& ctx) override {
        std::string input; std::getline(std::cin, input);
        return {"string", input};
    }
};

struct ReadFileNode : Node {
    std::unique_ptr<Node> filename;
    ReadFileNode(std::unique_ptr<Node> fn) : filename(std::move(fn)) {}

    Value eval(Context& ctx) override {
        Value filenameVal = filename->eval(ctx);
        if (filenameVal.type != "string") {
            throw std::runtime_error("read_file() requires string filename");
        }

        std::ifstream file(filenameVal.value);
        if (!file.is_open()) {
            return {"string", ""};  // Возвращаем пустую строку при ошибке
        }

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }

        file.close();
        return {"string", content};
    }
};

struct UsingNode : Node {
    std::string libName;
    UsingNode(std::string lib) : libName(lib) {}
    Value eval(Context& ctx) override { return {"void", ""}; }
};
