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

// Forward declaration
struct Node;

struct FuncParam {
    std::string type;
    std::string name;
};

// Контекст памяти (переменные и функции)
struct Context {
    Context* parent = nullptr; // Для глобальных переменных
    std::map<std::string, std::string> variables;
    std::map<std::string, std::shared_ptr<Node>> functions; // Храним функции
    std::map<std::string, std::vector<std::string>> arrays;

    bool exists(const std::string& name) {
        if (variables.count(name) || arrays.count(name)) return true;
        if (parent) return parent->exists(name);
        return false;
    }

    std::string getVar(const std::string& name) {
        if (variables.count(name)) return variables[name];
        if (parent) return parent->getVar(name);
        std::cerr << "Runtime Error: Variable '" << name << "' not found." << std::endl; exit(1);
    }
    
    std::vector<std::string>& getArray(const std::string& name) {
        if (arrays.count(name)) return arrays[name];
        if (parent) return parent->getArray(name);
        std::cerr << "Runtime Error: Array '" << name << "' not found." << std::endl; exit(1);
    }

    void setVar(const std::string& name, const std::string& val) {
        if (variables.count(name)) { variables[name] = val; return; }
        if (parent) { parent->setVar(name, val); return; }
        std::cerr << "Runtime Error: Variable '" << name << "' not defined." << std::endl; exit(1);
    }

    void defineVar(const std::string& name, const std::string& val) {
        if (variables.count(name)) {
            std::cerr << "Runtime Error: Variable '" << name << "' already defined." << std::endl; exit(1);
        }
        variables[name] = val;
    }

    void defineGlobal(const std::string& name, const std::string& val) {
        if (parent) parent->defineGlobal(name, val);
        else defineVar(name, val);
    }
    
    std::shared_ptr<Node> getFunc(const std::string& name) {
        if (functions.count(name)) return functions[name];
        if (parent) return parent->getFunc(name);
        return nullptr;
    }
    
    void defineFunc(const std::string& name, std::shared_ptr<Node> body) {
        functions[name] = body;
    }
};

// Специальный тип для RETURN
struct ReturnValue {
    std::string value;
};

static std::string formatNumber(double val) {
    std::string s = std::to_string(val);
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

struct Node {
    virtual ~Node() = default;
    virtual std::string eval(Context& ctx) = 0;
};

// --- ОСНОВНЫЕ УЗЛЫ ---

// Определение функции
struct FuncDefNode : Node {
    std::string returnType;
    std::string name;
    std::vector<FuncParam> params;
    std::shared_ptr<Node> body;

    FuncDefNode(std::string rt, std::string n, std::vector<FuncParam> p, std::shared_ptr<Node> b) 
        : returnType(rt), name(n), params(p), body(b) {}

    // Eval ничего не делает, функция регистрируется Парсером
    std::string eval(Context& ctx) override { return ""; }
};

// RETURN - выбрасывает исключение с значением
struct ReturnNode : Node {
    std::unique_ptr<Node> expr;
    ReturnNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        std::string result = expr ? expr->eval(ctx) : "0";
        throw ReturnValue{result}; 
    }
};

// ВЫЗОВ ФУНКЦИИ - самое важное для тебя!
struct FuncCallNode : Node {
    std::string name;
    std::vector<std::unique_ptr<Node>> args;

    FuncCallNode(std::string n, std::vector<std::unique_ptr<Node>> a) 
        : name(n), args(std::move(a)) {}

    std::string eval(Context& ctx) override {
        auto funcNodeBase = ctx.getFunc(name);
        if (!funcNodeBase) {
            std::cerr << "Runtime Error: Function '" << name << "' not found." << std::endl; exit(1);
        }
        
        FuncDefNode* funcDef = static_cast<FuncDefNode*>(funcNodeBase.get());
        
        if (args.size() != funcDef->params.size()) {
            std::cerr << "Args count mismatch for '" << name << "'" << std::endl; exit(1);
        }

        std::vector<std::string> argValues;
        for (auto& arg : args) argValues.push_back(arg->eval(ctx));

        // Находим глобальный контекст
        Context* root = &ctx;
        while (root->parent != nullptr) root = root->parent;
        
        // Создаем локальную область видимости
        Context funcScope;
        funcScope.parent = root; 

        for (size_t i = 0; i < funcDef->params.size(); i++) {
            funcScope.defineVar(funcDef->params[i].name, argValues[i]);
        }

        try {
            funcDef->body->eval(funcScope);
        } catch (const ReturnValue& ret) {
            return ret.value; // ВОЗВРАЩАЕМ ЗНАЧЕНИЕ В ПЕРЕМЕННУЮ
        }

        return "0";
    }
};

struct NumberNode : Node {
    std::string val;
    NumberNode(std::string v) : val(v) {}
    std::string eval(Context& ctx) override { return val; }
};
struct StringNode : Node {
    std::string val;
    StringNode(std::string v) : val(v) {}
    std::string eval(Context& ctx) override { return val; }
};
struct VarAccessNode : Node {
    std::string name;
    VarAccessNode(std::string n) : name(n) {}
    std::string eval(Context& ctx) override { return ctx.getVar(name); }
};
struct GlobalVarDeclNode : Node {
    std::string type, name; std::unique_ptr<Node> expr;
    GlobalVarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e) : type(t), name(n), expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        Context* root = &ctx;
        while (root->parent != nullptr) root = root->parent;
        root->defineVar(name, expr->eval(ctx)); 
        return "";
    }
};
struct VarDeclNode : Node {
    std::string type, name; std::unique_ptr<Node> expr;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e) : type(t), name(n), expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        // Здесь expr->eval(ctx) может быть FuncCallNode, который вернет результат!
        ctx.defineVar(name, expr->eval(ctx));
        return "";
    }
};
struct AssignNode : Node {
    std::string name; std::unique_ptr<Node> expr;
    AssignNode(std::string n, std::unique_ptr<Node> e) : name(n), expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        ctx.setVar(name, expr->eval(ctx));
        return "";
    }
};
struct BinOpNode : Node {
    char op; std::unique_ptr<Node> left, right;
    BinOpNode(char o, std::unique_ptr<Node> l, std::unique_ptr<Node> r) : op(o), left(std::move(l)), right(std::move(r)) {}
    std::string eval(Context& ctx) override {
        std::string lStr = left->eval(ctx); std::string rStr = right->eval(ctx);
        bool isStr = (lStr.find_first_not_of("0123456789.-") != std::string::npos) || (rStr.find_first_not_of("0123456789.-") != std::string::npos);
        if (op == '+' && isStr) return lStr + rStr;
        double l = std::stod(lStr); double r = std::stod(rStr);
        if (op == '+') return formatNumber(l+r);
        if (op == '-') return formatNumber(l-r);
        if (op == '*') return formatNumber(l*r);
        if (op == '/') return formatNumber(r!=0 ? l/r : 0);
        if (op == '%') return formatNumber((int)l % (int)r);
        return "0";
    }
};
struct CompareNode : Node {
    std::string op; std::unique_ptr<Node> left, right;
    CompareNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r) : op(o), left(std::move(l)), right(std::move(r)) {}
    std::string eval(Context& ctx) override {
        double l = std::stod(left->eval(ctx)); double r = std::stod(right->eval(ctx));
        if (op == "==") return (std::abs(l - r) < 0.001) ? "1" : "0";
        if (op == "!=") return (std::abs(l - r) > 0.001) ? "1" : "0";
        if (op == "<") return (l < r) ? "1" : "0";
        if (op == ">") return (l > r) ? "1" : "0";
        return "0";
    }
};
struct IfNode : Node {
    std::unique_ptr<Node> cond, thenB, elseB;
    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e) : cond(std::move(c)), thenB(std::move(t)), elseB(std::move(e)) {}
    std::string eval(Context& ctx) override {
        if (cond->eval(ctx) == "1") thenB->eval(ctx);
        else if (elseB) elseB->eval(ctx);
        return "";
    }
};
struct WhileNode : Node {
    std::unique_ptr<Node> cond, body;
    WhileNode(std::unique_ptr<Node> c, std::unique_ptr<Node> b) : cond(std::move(c)), body(std::move(b)) {}
    std::string eval(Context& ctx) override {
        while (cond->eval(ctx) == "1") body->eval(ctx);
        return "";
    }
};
struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> stmts;
    std::string eval(Context& ctx) override {
        for(auto& s : stmts) s->eval(ctx);
        return "";
    }
};
struct PrintNode : Node {
    std::unique_ptr<Node> expr;
    PrintNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval(Context& ctx) override { std::cout << expr->eval(ctx) << std::endl; return ""; }
};
struct InputNode : Node {
    std::string eval(Context& ctx) override { std::string b; std::getline(std::cin, b); return b; }
};
struct ArrayDeclNode : Node {
    std::string name; std::unique_ptr<Node> size;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s) : name(n), size(std::move(s)) {}
    std::string eval(Context& ctx) override {
        ctx.arrays[name] = std::vector<std::string>(std::stoi(size->eval(ctx)), "0");
        return "";
    }
};
struct ArraySetNode : Node {
    std::string name; std::unique_ptr<Node> idx, val;
    ArraySetNode(std::string n, std::unique_ptr<Node> i, std::unique_ptr<Node> v) : name(n), idx(std::move(i)), val(std::move(v)) {}
    std::string eval(Context& ctx) override {
        auto& arr = ctx.getArray(name); arr[std::stoi(idx->eval(ctx))] = val->eval(ctx); return "";
    }
};
struct ArrayGetNode : Node {
    std::string name; std::unique_ptr<Node> idx;
    ArrayGetNode(std::string n, std::unique_ptr<Node> i) : name(n), idx(std::move(i)) {}
    std::string eval(Context& ctx) override { return ctx.getArray(name)[std::stoi(idx->eval(ctx))]; }
};
struct IncludeNode : Node { std::string eval(Context& ctx) override { return ""; } };
struct FoxNode : Node { std::string eval(Context& ctx) override { std::cout << "FoxLang" << std::endl; return ""; } };