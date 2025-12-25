#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <cmath>
#include <random>

// Хелпер для красивого вывода чисел
static std::string formatNumber(double val) {
    std::string s = std::to_string(val);
    // Удаляем лишние нули и точку, если число целое
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (s.back() == '.') s.pop_back();
    return s;
}

struct Node {
    virtual ~Node() = default;
    virtual std::string eval() = 0;
};

struct NumberNode : Node {
    std::string val;
    NumberNode(std::string v) : val(v) {}
    std::string eval() override { return val; }
};

struct BinOpNode : Node {
    char op;
    std::unique_ptr<Node> left, right;
    BinOpNode(char o, std::unique_ptr<Node> l, std::unique_ptr<Node> r)
    : op(o), left(std::move(l)), right(std::move(r)) {}

    std::string eval() override {
        double l = std::stod(left->eval());
        double r = std::stod(right->eval());
        double res = 0;
        if (op == '+') res = l + r;
        else if (op == '-') res = l - r;
        else if (op == '*') res = l * r;
        else if (op == '/') res = (r != 0) ? l / r : 0;
        return formatNumber(res);
    }
};

struct RoundNode : Node {
    std::unique_ptr<Node> expr;
    RoundNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval() override {
        double val = std::stod(expr->eval());
        return formatNumber(std::round(val));
    }
};

struct RandomNode : Node {
    std::string eval() override {
        return std::to_string(rand() % 100); // Случайное от 0 до 99
    }
};

struct PrintNode : Node {
    std::unique_ptr<Node> expr;
    PrintNode(std::unique_ptr<Node> e) : expr(std::move(e)) {}
    std::string eval() override {
        std::cout << expr->eval() << std::endl;
        return "";
    }
};

struct InputNode : Node {
    std::string eval() override {
        std::string buffer;
        std::getline(std::cin, buffer);
        return buffer;
    }
};
