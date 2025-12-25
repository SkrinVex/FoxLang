#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>
#include <random>

// Предварительное объявление
struct Node;

// --- ПАМЯТЬ ЯЗЫКА ---
struct Context {
    // Переменные: Имя -> Значение
    std::map<std::string, std::string> variables;
    // Типы переменных: Имя -> Тип ("int", "string")
    std::map<std::string, std::string> varTypes;
    // Функции: Имя -> Узел тела функции (BlockNode)
    std::map<std::string, std::shared_ptr<Node>> functions;

    bool exists(const std::string& name) {
        return variables.find(name) != variables.end();
    }
};

static std::string formatNumber(double val) {
    std::string s = std::to_string(val);
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

// --- УЗЛЫ ---
struct Node {
    virtual ~Node() = default;
    virtual std::string eval(Context& ctx) = 0;
};

// 1. Базовые значения
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

// 2. Доступ к переменной (x)
struct VarAccessNode : Node {
    std::string name;
    VarAccessNode(std::string n) : name(n) {}
    std::string eval(Context& ctx) override {
        if (!ctx.exists(name)) {
            std::cerr << "Runtime Error: Variable '" << name << "' is not defined." << std::endl;
            exit(1);
        }
        return ctx.variables[name];
    }
};

// 3. Объявление переменной (int x = 5;)
struct VarDeclNode : Node {
    std::string type;
    std::string name;
    std::unique_ptr<Node> expr;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e) 
        : type(t), name(n), expr(std::move(e)) {}
    
    std::string eval(Context& ctx) override {
        if (ctx.exists(name)) {
            std::cerr << "Error: Variable '" << name << "' already exists." << std::endl;
            exit(1);
        }
        std::string val = expr->eval(ctx);
        // Простая проверка типа (можно усложнить)
        if (type == "int") {
            try { std::stod(val); } catch(...) { 
                std::cerr << "Type Error: Cannot assign string to int '" << name << "'" << std::endl; exit(1); 
            }
        }
        ctx.variables[name] = val;
        ctx.varTypes[name] = type;
        return val;
    }
};

// 4. Присваивание (x = 10;)
struct AssignNode : Node {
    std::string name;
    std::unique_ptr<Node> expr;
    AssignNode(std::string n, std::unique_ptr<Node> e) : name(n), expr(std::move(e)) {}
    
    std::string eval(Context& ctx) override {
        if (!ctx.exists(name)) {
            std::cerr << "Error: Variable '" << name << "' not declared." << std::endl;
            exit(1);
        }
        std::string val = expr->eval(ctx);
        ctx.variables[name] = val; // Тут можно добавить проверку типа снова
        return val;
    }
};

// 5. Математика
struct BinOpNode : Node {
    char op;
    std::unique_ptr<Node> left, right;
    BinOpNode(char o, std::unique_ptr<Node> l, std::unique_ptr<Node> r) 
        : op(o), left(std::move(l)), right(std::move(r)) {}
    
    std::string eval(Context& ctx) override {
        std::string lStr = left->eval(ctx);
        std::string rStr = right->eval(ctx);
        
        // Если это строки и операция +, конкатенируем
        bool isStr = (lStr.find_first_not_of("0123456789.-") != std::string::npos) || 
                     (rStr.find_first_not_of("0123456789.-") != std::string::npos);
        
        if (op == '+' && isStr) return lStr + rStr;

        double l = std::stod(lStr);
        double r = std::stod(rStr);
        if (op == '+') return formatNumber(l + r);
        if (op == '-') return formatNumber(l - r);
        if (op == '*') return formatNumber(l * r);
        if (op == '/') return formatNumber((r!=0)? l/r : 0);
        return "0";
    }
};

// 6. Блок кода { ... }
struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> statements;
    std::string eval(Context& ctx) override {
        std::string lastVal = "";
        for (auto& stmt : statements) {
            lastVal = stmt->eval(ctx);
        }
        return lastVal;
    }
};

// 7. Объявление функции (void myFunc() { ... })
// Мы не выполняем тело, а СОХРАНЯЕМ его в память.
struct FuncDefNode : Node {
    std::string name;
    std::shared_ptr<Node> body; // Используем shared_ptr, чтобы хранить в мапе
    FuncDefNode(std::string n, std::shared_ptr<Node> b) : name(n), body(b) {}
    
    std::string eval(Context& ctx) override {
        ctx.functions[name] = body;
        return "";
    }
};

// 8. Вызов функции (myFunc())
struct FuncCallNode : Node {
    std::string name;
    FuncCallNode(std::string n) : name(n) {}
    
    std::string eval(Context& ctx) override {
        if (ctx.functions.find(name) == ctx.functions.end()) {
            std::cerr << "Error: Function '" << name << "' is not defined." << std::endl;
            exit(1);
        }
        // Выполняем сохраненное тело
        return ctx.functions[name]->eval(ctx);
    }
};

// Стандартные функции
struct PrintNode : Node {
    std::unique_ptr<Node> expr;
    PrintNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        std::cout << expr->eval(ctx) << std::endl;
        return "";
    }
};

struct InputNode : Node {
    std::string eval(Context& ctx) override {
        std::string b; std::getline(std::cin, b); return b;
    }
};
struct RoundNode : Node {
    std::unique_ptr<Node> expr;
    RoundNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval(Context& ctx) override { return formatNumber(std::round(std::stod(expr->eval(ctx)))); }
};
struct RandomNode : Node {
    std::string eval(Context& ctx) override { return std::to_string(rand() % 100); }
};