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

// --- ПАМЯТЬ ---
struct Context {
    std::map<std::string, std::string> variables;
    std::map<std::string, std::shared_ptr<Node>> functions;
    // МАССИВЫ: Имя -> Вектор значений
    std::map<std::string, std::vector<std::string>> arrays;

    bool exists(const std::string& name) {
        return variables.count(name) || arrays.count(name);
    }
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

// --- БАЗОВЫЕ НОДЫ (Без изменений) ---
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
    std::string eval(Context& ctx) override {
        if (ctx.variables.find(name) == ctx.variables.end()) {
             // Проверка, может это массив?
             if(ctx.arrays.count(name)) return "ARRAY:" + name;
             std::cerr << "Runtime Error: Var '" << name << "' not found." << std::endl; exit(1);
        }
        return ctx.variables[name];
    }
};

// --- ЛОГИКА И УПРАВЛЕНИЕ ---

// Сравнение: ==, !=, <, >
// Сравнение: ==, !=, <, >
struct CompareNode : Node {
    std::string op;
    std::unique_ptr<Node> left, right;
    CompareNode(std::string o, std::unique_ptr<Node> l, std::unique_ptr<Node> r) 
        : op(o), left(std::move(l)), right(std::move(r)) {}
    
    std::string eval(Context& ctx) override {
        std::string lStr = left->eval(ctx);
        std::string rStr = right->eval(ctx);

        // Проверяем, являются ли значения числами
        bool lIsNum = (lStr.find_first_not_of("0123456789.-") == std::string::npos);
        bool rIsNum = (rStr.find_first_not_of("0123456789.-") == std::string::npos);

        // Если хотя бы одно значение - текст, сравниваем как СТРОКИ
        if (!lIsNum || !rIsNum) {
            if (op == "==") return (lStr == rStr) ? "1" : "0";
            if (op == "!=") return (lStr != rStr) ? "1" : "0";
            // Для строк операции > и < работают лексикографически (по алфавиту)
            if (op == "<") return (lStr < rStr) ? "1" : "0";
            if (op == ">") return (lStr > rStr) ? "1" : "0";
            return "0";
        }

        // Если оба числа - переводим в double и сравниваем математически
        double l = std::stod(lStr);
        double r = std::stod(rStr);

        if (op == "==") return (std::abs(l - r) < 0.00001) ? "1" : "0";
        if (op == "!=") return (std::abs(l - r) > 0.00001) ? "1" : "0";
        if (op == "<") return (l < r) ? "1" : "0";
        if (op == ">") return (l > r) ? "1" : "0";
        return "0";
    }
};

// IF / ELSE
struct IfNode : Node {
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> thenBlock;
    std::unique_ptr<Node> elseBlock; // Может быть nullptr

    IfNode(std::unique_ptr<Node> c, std::unique_ptr<Node> t, std::unique_ptr<Node> e)
        : condition(std::move(c)), thenBlock(std::move(t)), elseBlock(std::move(e)) {}
    
    std::string eval(Context& ctx) override {
        std::string res = condition->eval(ctx);
        if (res == "1") { // Истина
            thenBlock->eval(ctx);
        } else if (elseBlock) {
            elseBlock->eval(ctx);
        }
        return "";
    }
};

// WHILE
struct WhileNode : Node {
    std::unique_ptr<Node> condition;
    std::unique_ptr<Node> body;
    WhileNode(std::unique_ptr<Node> c, std::unique_ptr<Node> b)
        : condition(std::move(c)), body(std::move(b)) {}
    
    std::string eval(Context& ctx) override {
        while (true) {
            std::string res = condition->eval(ctx);
            if (res != "1") break; // Если не 1, выходим
            body->eval(ctx);
        }
        return "";
    }
};

// --- МАССИВЫ ---

// array myArr 10;
struct ArrayDeclNode : Node {
    std::string name;
    std::unique_ptr<Node> sizeExpr;
    ArrayDeclNode(std::string n, std::unique_ptr<Node> s) : name(n), sizeExpr(std::move(s)) {}
    std::string eval(Context& ctx) override {
        int size = std::stoi(sizeExpr->eval(ctx));
        ctx.arrays[name] = std::vector<std::string>(size, "0");
        return "";
    }
};

// set(arr, index, value);
struct ArraySetNode : Node {
    std::string name;
    std::unique_ptr<Node> idx;
    std::unique_ptr<Node> val;
    ArraySetNode(std::string n, std::unique_ptr<Node> i, std::unique_ptr<Node> v)
        : name(n), idx(std::move(i)), val(std::move(v)) {}
    std::string eval(Context& ctx) override {
        if (!ctx.arrays.count(name)) { std::cerr << "Array not found: " << name << std::endl; exit(1); }
        int i = std::stoi(idx->eval(ctx));
        if (i < 0 || i >= ctx.arrays[name].size()) { std::cerr << "Index out of bounds" << std::endl; exit(1); }
        ctx.arrays[name][i] = val->eval(ctx);
        return "";
    }
};

// get(arr, index)
struct ArrayGetNode : Node {
    std::string name;
    std::unique_ptr<Node> idx;
    ArrayGetNode(std::string n, std::unique_ptr<Node> i) : name(n), idx(std::move(i)) {}
    std::string eval(Context& ctx) override {
        if (!ctx.arrays.count(name)) { std::cerr << "Array not found: " << name << std::endl; exit(1); }
        int i = std::stoi(idx->eval(ctx));
        if (i < 0 || i >= ctx.arrays[name].size()) { std::cerr << "Index out of bounds" << std::endl; exit(1); }
        return ctx.arrays[name][i];
    }
};

struct ArraySizeNode : Node {
    std::string name;
    ArraySizeNode(std::string n) : name(n) {}
    std::string eval(Context& ctx) override {
        if (!ctx.arrays.count(name)) return "0";
        return std::to_string(ctx.arrays[name].size());
    }
};

// --- INCLUDE ---
// Чтобы не было круговых зависимостей, мы парсим файл прямо тут (костыль, но рабочий для .h файла)
// В реальном проекте парсер нужно передавать или делать Include отдельным механизмом
struct IncludeNode : Node {
    std::string filename;
    // Нам нужен указатель на функцию парсинга... но мы схитрим
    // Мы просто прочитаем файл и добавим его как текст, но AST строится статически.
    // Поэтому Include в интерпретаторах часто работает на этапе парсинга, а не выполнения.
    // Но ты просил "библиотеки". Сделаем Runtime Include.
    
    // ВНИМАНИЕ: Для полноценного include здесь нужен доступ к Parser. 
    // Я оставлю заглушку, а реализацию сделаю в Parser.cpp через рекурсию.
    // Это просто маркер.
    std::string eval(Context& ctx) override { return ""; } 
};


// Остальные ноды (VarDecl, Assign, BinOp, Functions...) остаются такими же, 
// скопируй их из предыдущего ответа, я добавлю только ИСПРАВЛЕННЫЙ FOX.

struct VarDeclNode : Node {
    std::string type, name; std::unique_ptr<Node> expr;
    VarDeclNode(std::string t, std::string n, std::unique_ptr<Node> e) : type(t), name(n), expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        if(ctx.exists(name)) { std::cerr << "Redeclaration: " << name << std::endl; exit(1); }
        ctx.variables[name] = expr->eval(ctx);
        return ctx.variables[name];
    }
};

struct AssignNode : Node {
    std::string name; std::unique_ptr<Node> expr;
    AssignNode(std::string n, std::unique_ptr<Node> e) : name(n), expr(std::move(e)) {}
    std::string eval(Context& ctx) override {
        if(!ctx.exists(name)) { std::cerr << "Unknown var: " << name << std::endl; exit(1); }
        ctx.variables[name] = expr->eval(ctx);
        return ctx.variables[name];
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

struct BlockNode : Node {
    std::vector<std::unique_ptr<Node>> statements;
    std::string eval(Context& ctx) override {
        std::string last = "";
        for(auto& s : statements) last = s->eval(ctx);
        return last;
    }
};

struct FuncDefNode : Node {
    std::string name; std::shared_ptr<Node> body;
    FuncDefNode(std::string n, std::shared_ptr<Node> b) : name(n), body(b) {}
    std::string eval(Context& ctx) override { ctx.functions[name] = body; return ""; }
};

struct FuncCallNode : Node {
    std::string name;
    FuncCallNode(std::string n) : name(n) {}
    std::string eval(Context& ctx) override {
        if(!ctx.functions.count(name)) { std::cerr << "Unknown func: " << name << std::endl; exit(1); }
        return ctx.functions[name]->eval(ctx);
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
struct RoundNode : Node {
    std::unique_ptr<Node> expr;
    RoundNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval(Context& ctx) override { return formatNumber(std::round(std::stod(expr->eval(ctx)))); }
};
struct RandomNode : Node {
    std::string eval(Context& ctx) override { return std::to_string(rand() % 100); }
};

// ИСПРАВЛЕННЫЙ FOX NODE: Теперь он выводит текст сам!
struct FoxNode : Node {
    std::string eval(Context& ctx) override {
        std::cout << " (\\_/)\n (o.o)  FoxLang v4.0\n (> <)" << std::endl;
        return "fox";
    }
};