#include "Parser.h"
#include <iostream>

Parser::Parser(std::vector<Token> t) : tokens(t) {}

Token Parser::consume(TokenType type) {
    if (tokens[pos].type == type) return tokens[pos++];
    std::cerr << "Syntax Error: Expected token type " << (int)type
    << " but found '" << tokens[pos].value
    << "' on line " << tokens[pos].line << std::endl;
    exit(1);
}

// Выражения (возвращают значение)
std::unique_ptr<Node> Parser::primary() {
    if (tokens[pos].type == TokenType::NUMBER) return std::make_unique<NumberNode>(consume(TokenType::NUMBER).value);
    if (tokens[pos].type == TokenType::STRING) return std::make_unique<NumberNode>(consume(TokenType::STRING).value);

    // input()
    if (tokens[pos].type == TokenType::INPUT) {
        consume(TokenType::INPUT); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        return std::make_unique<InputNode>();
    }

    // round( expr )
    if (tokens[pos].type == TokenType::ROUND) {
        consume(TokenType::ROUND);
        consume(TokenType::LPAREN);
        auto node = expression();
        consume(TokenType::RPAREN);
        return std::make_unique<RoundNode>(std::move(node));
    }

    // random()
    if (tokens[pos].type == TokenType::RANDOM) {
        consume(TokenType::RANDOM); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        return std::make_unique<RandomNode>();
    }

    if (tokens[pos].type == TokenType::LPAREN) {
        consume(TokenType::LPAREN);
        auto node = expression();
        consume(TokenType::RPAREN);
        return node;
    }

    std::cerr << "Error: Unexpected token '" << tokens[pos].value << "' in expression at line " << tokens[pos].line << std::endl;
    exit(1);
}

std::unique_ptr<Node> Parser::multiplication() {
    auto node = primary();
    while (tokens[pos].type == TokenType::STAR || tokens[pos].type == TokenType::SLASH) {
        char op = tokens[pos].value[0]; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), primary());
    }
    return node;
}

std::unique_ptr<Node> Parser::expression() {
    auto node = multiplication();
    while (tokens[pos].type == TokenType::PLUS || tokens[pos].type == TokenType::MINUS) {
        char op = tokens[pos].value[0]; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), multiplication());
    }
    return node;
}

// Блоки кода
void Parser::block() {
    consume(TokenType::LBRACE);
    while (tokens[pos].type != TokenType::RBRACE && tokens[pos].type != TokenType::END) {
        statement();
    }
    consume(TokenType::RBRACE);
}

// Инструкции (заканчиваются ;)
void Parser::statement() {
    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT);
        consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        auto pNode = std::make_unique<PrintNode>(std::move(expr));
        pNode->eval();
    }
    else if (tokens[pos].type == TokenType::FOX) {
        consume(TokenType::FOX);
        consume(TokenType::LPAREN); // fox (
            consume(TokenType::RPAREN); // )
            consume(TokenType::SEMICOLON); // ;
            std::cout << " (\\_/) \n (o.o) \n (> <) OFOX!" << std::endl;
    }
    else if (tokens[pos].type == TokenType::LBRACE) {
        block();
    }
    else {
        // ОБРАБОТКА ОШИБОК: Если встретили слово, которое не является командой
        std::cerr << "Runtime Error: Unknown command or function '" << tokens[pos].value
        << "' at line " << tokens[pos].line << std::endl;
        exit(1);
    }
}

void Parser::run() {
    while (tokens[pos].type != TokenType::END) {
        statement();
    }
}
