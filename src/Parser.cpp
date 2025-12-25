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

std::unique_ptr<Node> Parser::primary() {
    if (tokens[pos].type == TokenType::NUMBER) 
        return std::make_unique<NumberNode>(consume(TokenType::NUMBER).value);
    
    if (tokens[pos].type == TokenType::STRING_LITERAL) 
        return std::make_unique<StringNode>(consume(TokenType::STRING_LITERAL).value);
    
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        std::string name = consume(TokenType::IDENTIFIER).value;
        if (tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN);
            consume(TokenType::RPAREN);
            return std::make_unique<FuncCallNode>(name);
        }
        return std::make_unique<VarAccessNode>(name);
    }

    if (tokens[pos].type == TokenType::INPUT) { 
        consume(TokenType::INPUT); consume(TokenType::LPAREN); consume(TokenType::RPAREN); 
        return std::make_unique<InputNode>(); 
    }
    if (tokens[pos].type == TokenType::ROUND) {
        consume(TokenType::ROUND); consume(TokenType::LPAREN); auto n = expression(); consume(TokenType::RPAREN);
        return std::make_unique<RoundNode>(std::move(n));
    }
    if (tokens[pos].type == TokenType::RANDOM) {
        consume(TokenType::RANDOM); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        return std::make_unique<RandomNode>();
    }
    if (tokens[pos].type == TokenType::LPAREN) {
        consume(TokenType::LPAREN); auto n = expression(); consume(TokenType::RPAREN); return n;
    }

    std::cerr << "Error: Unexpected token '" << tokens[pos].value << "' line " << tokens[pos].line << std::endl;
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

// ИЗМЕНЕНИЕ: Возвращаем unique_ptr и используем make_unique
std::unique_ptr<BlockNode> Parser::parseBlock() {
    consume(TokenType::LBRACE);
    auto block = std::make_unique<BlockNode>(); // Здесь было make_shared
    while (tokens[pos].type != TokenType::RBRACE && tokens[pos].type != TokenType::END) {
        block->statements.push_back(statement());
    }
    consume(TokenType::RBRACE);
    return block;
}

std::unique_ptr<Node> Parser::statement() {
    // 1. Объявление переменных
    if (tokens[pos].type == TokenType::INT_KW || tokens[pos].type == TokenType::STRING_KW) {
        std::string type = tokens[pos].value; 
        pos++;
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<VarDeclNode>(type, name, std::move(expr));
    }

    // 2. Объявление функций
    if (tokens[pos].type == TokenType::VOID_KW) {
        consume(TokenType::VOID_KW);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        
        // Тут магия: unique_ptr автоматически превратится в shared_ptr при создании FuncDefNode
        auto body = parseBlock(); 
        return std::make_unique<FuncDefNode>(name, std::move(body));
    }

    // 3. Команда PRINT
    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT); consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        return std::make_unique<PrintNode>(std::move(expr));
    }

    if (tokens[pos].type == TokenType::FOX) {
        consume(TokenType::FOX); consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        return std::make_unique<StringNode>("Fox says: Hello!"); 
    }

    if (tokens[pos].type == TokenType::IDENTIFIER) {
        if (tokens[pos+1].type == TokenType::ASSIGN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::ASSIGN);
            auto expr = expression();
            consume(TokenType::SEMICOLON);
            return std::make_unique<AssignNode>(name, std::move(expr));
        }
        if (tokens[pos+1].type == TokenType::LPAREN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
            return std::make_unique<FuncCallNode>(name);
        }
    }

    // ИЗМЕНЕНИЕ: Просто возвращаем результат parseBlock, никаких преобразований не нужно
    if (tokens[pos].type == TokenType::LBRACE) {
        return parseBlock(); 
    }

    std::cerr << "Syntax Error: Unknown statement '" << tokens[pos].value << "' at line " << tokens[pos].line << std::endl;
    exit(1);
}

void Parser::run() {
    while (tokens[pos].type != TokenType::END) {
        auto stmt = statement();
        stmt->eval(globalContext);
    }
}