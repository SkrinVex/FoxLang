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

// Forward declaration
struct Node;

struct FuncParam {
    std::string type;
    std::string name;
};

// Unified Value type for the language
struct Value {
    std::string type; // "int", "float", "string", "bool", "void"
    std::string value;
};

// Контекст памяти (переменные и функции)
struct Context {
    Context* parent = nullptr; // Для глобальных переменных
    std::map<std::string, Value> variables; // Stores typed values
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
        if (variables.count(name)) {
            throw std::runtime_error("Error: Variable '" + name + "' already defined!");
        }
        std::string finalVal = value.value;
        if (type == "int") {
             try {
                finalVal = std::to_string((int)std::stod(value.value));
             } catch(...) { finalVal = "0"; }
        }
        variables[name] = {type, finalVal};
    }

    // Set variable with type checking/conversion
    void setVar(const std::string& name, Value val) {
        if (variables.count(name)) { 
            std::string targetType = variables[name].type;
            
            // Handle int/float conversion
            if (targetType == "int") {
                try {
                    double d = std::stod(val.value);
                    variables[name].value = std::to_string((int)d);
                } catch(...) { variables[name].value = "0"; }
            } else if (targetType == "float") {
                try {
                    double d = std::stod(val.value);
                    variables[name].value = formatNumber(d);
                } catch(...) { variables[name].value = "0.0"; }
            } else {
                variables[name].value = val.value;
            }
            return; 
        }
        if (parent) { parent->setVar(name, val); return; }
        throw std::runtime_error("Error: Variable '" + name + "' not defined!");
    }

    void defineGlobal(const std::string& name, std::string type, Value val) {
        if (parent) parent->defineGlobal(name, type, val);
        else defineVar(name, type, val);
    }
};

// Exception for return
struct ReturnValue {
    Value value;
};

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

// --- ОСНОВНЫЕ УЗЛЫ ---

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
        if (name == "print" && args.size() == 1) {
            std::cout << args[0]->eval(ctx).value << std::endl;
            return {"void", ""};
        }
        if (name == "input" && args.size() == 0) {
            std::string input; std::getline(std::cin, input);
            return {"string", input};
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

        Context* root = &ctx;
        while (root->parent != nullptr) root = root->parent;
        
        Context funcScope;
        funcScope.parent = root; 

        for (size_t i = 0; i < funcDef->params.size(); i++) {
            funcScope.defineVar(funcDef->params[i].name, funcDef->params[i].type, argValues[i]);
        }

        try {
            funcDef->body->eval(funcScope);
        } catch (const ReturnValue& ret) {
            return ret.value; 
        }

        return {"void", "0"};
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
            body->eval(ctx);
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

struct UsingNode : Node {
    std::string libName;
    UsingNode(std::string lib) : libName(lib) {}
    Value eval(Context& ctx) override { return {"void", ""}; }
};
