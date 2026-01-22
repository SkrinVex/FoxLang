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
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
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
            int ch = getchar();
            if (ch != EOF) {
                ungetc(ch, stdin);
                return {"bool", "true"};
            }
            return {"bool", "false"};
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
            
            std::string line;
            while (std::getline(file, line)) {
                // Пропускаем комментарии и пустые строки
                if (line.empty() || line[0] == '#') continue;
                
                // Если строка не пустая и не комментарий, возвращаем её
                if (!line.empty()) {
                    file.close();
                    return {"string", line};
                }
            }
            
            file.close();
            return {"string", ""};
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
            std::string cmd = "curl -s \"" + urlVal.value + "\"";
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
        ctx.defineVar(name, type, expr->eval(ctx));
        return {"void", ""};
    }
};

struct GlobalVarDeclNode : Node {
    std::string type, name;
    std::unique_ptr<Node> expr;
    GlobalVarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e) 
        : type(t), name(n), expr(std::move(e)) {}
    Value eval(Context& ctx) override {
        Context* root = &ctx;
        while (root->parent != nullptr) root = root->parent;
        root->defineVar(name, type, expr->eval(ctx));
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
    char op;
    std::unique_ptr<Node> left, right;
    BinOpNode(char o, std::unique_ptr<Node> l, std::unique_ptr<Node> r) 
        : op(o), left(std::move(l)), right(std::move(r)) {}
    
    Value eval(Context& ctx) override {
        Value lval = left->eval(ctx);
        Value rval = right->eval(ctx);
        
        if (op == '+') {
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
        
        if (op == '-' || op == '*' || op == '/' || op == '%') {
            if (lval.type == "float" || rval.type == "float") {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                double result = (op == '-') ? l - r : (op == '*') ? l * r : 
                               (op == '/') ? l / r : std::fmod(l, r);
                return {"float", formatNumber(result)};
            }
            int l = std::stoi(lval.value), r = std::stoi(rval.value);
            int result = (op == '-') ? l - r : (op == '*') ? l * r : 
                        (op == '/') ? l / r : l % r;
            return {"int", std::to_string(result)};
        }
        
        if (op == '=' || op == '!' || op == '<' || op == '>') {
            bool result;
            if (lval.type == "string" && rval.type == "string") {
                result = (op == '=') ? lval.value == rval.value : 
                        (op == '!') ? lval.value != rval.value :
                        (op == '<') ? lval.value < rval.value : lval.value > rval.value;
            } else {
                double l = std::stod(lval.value), r = std::stod(rval.value);
                result = (op == '=') ? l == r : (op == '!') ? l != r :
                        (op == '<') ? l < r : l > r;
            }
            return {"bool", result ? "true" : "false"};
        }
        
        if (op == '&' || op == '|') {
            bool l = (lval.value == "true"), r = (rval.value == "true");
            bool result = (op == '&') ? l && r : l || r;
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
    int size;
    ArrayDeclNode(std::string n, int s) : name(n), size(s) {}
    Value eval(Context& ctx) override {
        ctx.arrays[name] = std::vector<Value>(size, {"int", "0"});
        return {"void", ""};
    }
};

struct ArraySetNode : Node {
    std::string name;
    std::unique_ptr<Node> index, value;
    ArraySetNode(std::string n, std::unique_ptr<Node> i, std::unique_ptr<Node> v) 
        : name(n), index(std::move(i)), value(std::move(v)) {}
    Value eval(Context& ctx) override {
        int idx = std::stoi(index->eval(ctx).value);
        ctx.getArray(name)[idx] = value->eval(ctx);
        return {"void", ""};
    }
};

struct ArrayGetNode : Node {
    std::string name;
    std::unique_ptr<Node> index;
    ArrayGetNode(std::string n, std::unique_ptr<Node> i) : name(n), index(std::move(i)) {}
    Value eval(Context& ctx) override {
        int idx = std::stoi(index->eval(ctx).value);
        return ctx.getArray(name)[idx];
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
        bool first = true;
        while (std::getline(file, line)) {
            // Пропускаем комментарии и пустые строки
            if (line.empty() || line[0] == '#') continue;
            
            // Если строка не пустая и не комментарий, возвращаем её
            if (!line.empty()) {
                file.close();
                return {"string", line};
            }
        }
        
        file.close();
        return {"string", ""};
    }
};

struct UsingNode : Node {
    std::string libName;
    UsingNode(std::string lib) : libName(lib) {}
    Value eval(Context& ctx) override { return {"void", ""}; }
};
