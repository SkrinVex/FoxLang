#include "foxlang/Parser.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace foxlang {

Parser::Parser(std::vector<Token> t, std::string curFile)
    : currentFile(std::move(curFile)), tokens(std::move(t)) {}

Token Parser::consume(TokenType type) {
    if (pos < tokens.size() && tokens[pos].type == type) {
        return tokens[pos++];
    }
    std::string gotVal = (pos < tokens.size()) ? tokens[pos].value : "EOF";
    int line = (pos < tokens.size()) ? tokens[pos].line : (tokens.empty() ? 1 : tokens.back().line);
    throw std::runtime_error("Syntax Error: Expected token " + std::string(tokenTypeName(type)) +
                             " got '" + gotVal + "' line " + std::to_string(line));
}

const Token& Parser::peek(size_t offset) const {
    if (pos + offset < tokens.size()) {
        return tokens[pos + offset];
    }
    return tokens.back();
}

bool Parser::match(TokenType type) {
    if (pos < tokens.size() && tokens[pos].type == type) {
        pos++;
        return true;
    }
    return false;
}

void Parser::synchronize() {
    if (pos >= tokens.size()) return;
    pos++;
    while (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        if (tokens[pos - 1].type == TokenType::SEMICOLON || tokens[pos - 1].type == TokenType::RBRACE) {
            return;
        }
        switch (tokens[pos].type) {
            case TokenType::INT_KW:
            case TokenType::FLOAT_KW:
            case TokenType::STRING_KW:
            case TokenType::BOOL_KW:
            case TokenType::VOID_KW:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::RETURN:
            case TokenType::SWITCH:
            case TokenType::GLOBAL:
            case TokenType::INCLUDE:
            case TokenType::USING:
            case TokenType::ARRAY:
                return;
            default:
                pos++;
                break;
        }
    }
}

std::unique_ptr<BlockNode> Parser::parseProgram() {
    auto program = std::make_unique<BlockNode>();
    program->scoped = false;
    SourcePosition startPos = peek().range.start;
    while (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        auto stmt = statement();
        if (stmt) {
            program->stmts.push_back(std::move(stmt));
        }
    }
    SourcePosition endPos = (pos > 0 ? tokens[pos - 1].range.end : startPos);
    program->range = {startPos, endPos};
    return program;
}

std::unique_ptr<BlockNode> Parser::parseProgramWithDiagnostics(std::vector<Diagnostic>& outDiagnostics) {
    collectDiagnostics = true;
    diagnostics.clear();
    auto program = std::make_unique<BlockNode>();
    program->scoped = false;
    SourcePosition startPos = peek().range.start;
    while (pos < tokens.size() && tokens[pos].type != TokenType::END) {
        try {
            auto stmt = statement();
            if (stmt) {
                program->stmts.push_back(std::move(stmt));
            }
        } catch (const std::exception& e) {
            SourcePosition errStart = (pos < tokens.size()) ? tokens[pos].range.start : SourcePosition{1, 1, 0};
            SourcePosition errEnd = (pos < tokens.size()) ? tokens[pos].range.end : errStart;
            diagnostics.push_back({DiagnosticSeverity::Error, e.what(), {errStart, errEnd}});
            synchronize();
        }
    }
    SourcePosition endPos = (pos > 0 ? tokens[pos - 1].range.end : startPos);
    program->range = {startPos, endPos};
    outDiagnostics = diagnostics;
    return program;
}

std::unique_ptr<Node> Parser::primary() {
    if (pos >= tokens.size()) {
        throw std::runtime_error("Parser Error: Unexpected end of tokens");
    }

    SourcePosition startPos = peek().range.start;

    // 1. Unary minus (-5, -var)
    if (tokens[pos].type == TokenType::MINUS) {
        consume(TokenType::MINUS);
        auto operand = primary();
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<BinOpNode>("-",
            std::make_unique<NumberNode>("0"),
            std::move(operand)
        );
        node->range = {startPos, endPos};
        return node;
    }

    // 1.5. Unary NOT (!condition)
    if (tokens[pos].type == TokenType::NOT) {
        consume(TokenType::NOT);
        auto operand = primary();
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<UnaryOpNode>("!", std::move(operand));
        node->range = {startPos, endPos};
        return node;
    }

    // 2. Number
    if (tokens[pos].type == TokenType::NUMBER) {
        Token tok = consume(TokenType::NUMBER);
        auto node = std::make_unique<NumberNode>(tok.value);
        node->range = tok.range;
        return node;
    }

    // 3. String literal (with adjacent string concatenation)
    if (tokens[pos].type == TokenType::STRING_LITERAL) {
        Token tok = consume(TokenType::STRING_LITERAL);
        std::string value = tok.value;
        SourcePosition endPos = tok.range.end;
        while (pos < tokens.size() && tokens[pos].type == TokenType::STRING_LITERAL) {
            Token nextTok = consume(TokenType::STRING_LITERAL);
            value += nextTok.value;
            endPos = nextTok.range.end;
        }
        auto node = std::make_unique<StringNode>(value);
        node->range = {startPos, endPos};
        return node;
    }

    // 4. Boolean literals
    if (tokens[pos].type == TokenType::TRUE_KW) {
        Token tok = consume(TokenType::TRUE_KW);
        auto node = std::make_unique<BoolNode>(true);
        node->range = tok.range;
        return node;
    }
    if (tokens[pos].type == TokenType::FALSE_KW) {
        Token tok = consume(TokenType::FALSE_KW);
        auto node = std::make_unique<BoolNode>(false);
        node->range = tok.range;
        return node;
    }

    // 5. Identifier (variable or function call)
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        Token idTok = consume(TokenType::IDENTIFIER);
        std::string name = idTok.value;
        SourceRange nameRange = idTok.range;

        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN);
            std::vector<std::unique_ptr<Node>> args;
            if (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                args.push_back(expression());
                while (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
                    consume(TokenType::COMMA);
                    args.push_back(expression());
                }
            }
            consume(TokenType::RPAREN);
            SourcePosition endPos = tokens[pos - 1].range.end;
            auto node = std::make_unique<FuncCallNode>(name, std::move(args), nameRange);
            node->range = {startPos, endPos};
            return node;
        }

        if (pos < tokens.size() && tokens[pos].type == TokenType::INC) {
            Token incTok = consume(TokenType::INC);
            auto node = std::make_unique<PostIncNode>(name);
            node->range = {startPos, incTok.range.end};
            return node;
        }

        auto node = std::make_unique<VarAccessNode>(name, nameRange);
        node->range = nameRange;
        return node;
    }

    // 6. Parenthesized expression
    if (tokens[pos].type == TokenType::LPAREN) {
        consume(TokenType::LPAREN);
        auto expr = logicalOr();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        expr->range = {startPos, endPos};
        return expr;
    }

    // 7. Built-in functions in expressions
    if (tokens[pos].type == TokenType::INPUT) {
        consume(TokenType::INPUT);
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<InputNode>();
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::ROUND) {
        consume(TokenType::ROUND);
        consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(expr));
        auto node = std::make_unique<FuncCallNode>("round", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::RANDOM) {
        consume(TokenType::RANDOM);
        consume(TokenType::LPAREN);
        auto minExpr = expression();
        consume(TokenType::COMMA);
        auto maxExpr = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(minExpr));
        args.push_back(std::move(maxExpr));
        auto node = std::make_unique<FuncCallNode>("random", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::READ_FILE) {
        consume(TokenType::READ_FILE);
        consume(TokenType::LPAREN);
        auto path = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(path));
        auto node = std::make_unique<FuncCallNode>("read_file", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::JSON_GET) {
        consume(TokenType::JSON_GET);
        consume(TokenType::LPAREN);
        auto json = expression();
        consume(TokenType::COMMA);
        auto key = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(json));
        args.push_back(std::move(key));
        auto node = std::make_unique<FuncCallNode>("json_get", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::STR_CONTAINS) {
        consume(TokenType::STR_CONTAINS);
        consume(TokenType::LPAREN);
        auto str = expression();
        consume(TokenType::COMMA);
        auto substr = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(str));
        args.push_back(std::move(substr));
        auto node = std::make_unique<FuncCallNode>("str_contains", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::STR_TO_INT) {
        consume(TokenType::STR_TO_INT);
        consume(TokenType::LPAREN);
        auto str = expression();
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(str));
        auto node = std::make_unique<FuncCallNode>("str_to_int", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SIZE) {
        consume(TokenType::SIZE);
        consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::make_unique<VarAccessNode>(name));
        auto node = std::make_unique<FuncCallNode>("size", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::GET) {
        consume(TokenType::GET);
        consume(TokenType::LPAREN);
        auto firstArg = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(firstArg));
        if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("get", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::HTTP_GET) {
        consume(TokenType::HTTP_GET);
        consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("httpget", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::HTTP_POST) {
        consume(TokenType::HTTP_POST);
        consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::COMMA);
        auto data = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        args.push_back(std::move(data));
        if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("httppost", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::HTTP_PUT) {
        consume(TokenType::HTTP_PUT);
        consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::COMMA);
        auto data = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        args.push_back(std::move(data));
        if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("httpput", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::HTTP_DELETE) {
        consume(TokenType::HTTP_DELETE);
        consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("httpdelete", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::GETCH) {
        consume(TokenType::GETCH);
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("getch", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::KBHIT) {
        consume(TokenType::KBHIT);
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("kbhit", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SERVER_START) {
        consume(TokenType::SERVER_START);
        consume(TokenType::LPAREN);
        auto port = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(port));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("server_start", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SERVER_STOP) {
        consume(TokenType::SERVER_STOP);
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("server_stop", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::ROUTE_GET) {
        consume(TokenType::ROUTE_GET);
        consume(TokenType::LPAREN);
        auto path = expression();
        consume(TokenType::COMMA);
        auto handler = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(path));
        args.push_back(std::move(handler));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("route_get", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::ROUTE_POST) {
        consume(TokenType::ROUTE_POST);
        consume(TokenType::LPAREN);
        auto path = expression();
        consume(TokenType::COMMA);
        auto handler = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(path));
        args.push_back(std::move(handler));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("route_post", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SEND_RESPONSE) {
        consume(TokenType::SEND_RESPONSE);
        consume(TokenType::LPAREN);
        auto first = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(first));
        if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("send_response", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    throw std::runtime_error("Parser Error: Unexpected token " + tokens[pos].value + " at line " + std::to_string(tokens[pos].line));
}

std::unique_ptr<Node> Parser::multiplication() {
    SourcePosition startPos = peek().range.start;
    auto node = primary();
    while (pos < tokens.size() &&
           (tokens[pos].type == TokenType::STAR || tokens[pos].type == TokenType::SLASH || tokens[pos].type == TokenType::MOD)) {
        Token op = tokens[pos++];
        auto right = primary();
        SourcePosition endPos = tokens[pos - 1].range.end;
        node = std::make_unique<BinOpNode>(op.value, std::move(node), std::move(right));
        node->range = {startPos, endPos};
    }
    return node;
}

std::unique_ptr<Node> Parser::addition() {
    SourcePosition startPos = peek().range.start;
    auto node = multiplication();
    while (pos < tokens.size() && (tokens[pos].type == TokenType::PLUS || tokens[pos].type == TokenType::MINUS)) {
        Token op = tokens[pos++];
        auto right = multiplication();
        SourcePosition endPos = tokens[pos - 1].range.end;
        node = std::make_unique<BinOpNode>(op.value, std::move(node), std::move(right));
        node->range = {startPos, endPos};
    }
    return node;
}

std::unique_ptr<Node> Parser::comparison() {
    SourcePosition startPos = peek().range.start;
    auto node = addition();
    while (pos < tokens.size() &&
           (tokens[pos].type == TokenType::EQ || tokens[pos].type == TokenType::NEQ ||
            tokens[pos].type == TokenType::LT || tokens[pos].type == TokenType::GT ||
            tokens[pos].type == TokenType::LTE || tokens[pos].type == TokenType::GTE)) {
        Token op = tokens[pos++];
        auto right = addition();
        SourcePosition endPos = tokens[pos - 1].range.end;
        node = std::make_unique<BinOpNode>(op.value, std::move(node), std::move(right));
        node->range = {startPos, endPos};
    }
    return node;
}

std::unique_ptr<Node> Parser::logicalAnd() {
    SourcePosition startPos = peek().range.start;
    auto node = comparison();
    while (pos < tokens.size() && tokens[pos].type == TokenType::AND) {
        pos++;
        auto right = comparison();
        SourcePosition endPos = tokens[pos - 1].range.end;
        node = std::make_unique<BinOpNode>("&&", std::move(node), std::move(right));
        node->range = {startPos, endPos};
    }
    return node;
}

std::unique_ptr<Node> Parser::logicalOr() {
    SourcePosition startPos = peek().range.start;
    auto node = logicalAnd();
    while (pos < tokens.size() && tokens[pos].type == TokenType::OR) {
        pos++;
        auto right = logicalAnd();
        SourcePosition endPos = tokens[pos - 1].range.end;
        node = std::make_unique<BinOpNode>("||", std::move(node), std::move(right));
        node->range = {startPos, endPos};
    }
    return node;
}

std::unique_ptr<Node> Parser::expression() {
    return logicalOr();
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    SourcePosition startPos = peek().range.start;
    consume(TokenType::LBRACE);
    auto block = std::make_unique<BlockNode>();
    while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
        block->stmts.push_back(statement());
    }
    consume(TokenType::RBRACE);
    SourcePosition endPos = tokens[pos - 1].range.end;
    block->range = {startPos, endPos};
    return block;
}

std::unique_ptr<Node> Parser::statement() {
    SourcePosition startPos = peek().range.start;

    if (tokens[pos].type == TokenType::INCLUDE) {
        consume(TokenType::INCLUDE);
        consume(TokenType::LPAREN);
        std::string file = consume(TokenType::STRING_LITERAL).value;
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<IncludeNode>(file, currentFile);
        imports.push_back({file, false});
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::USING) {
        consume(TokenType::USING);
        if (tokens[pos].type == TokenType::END || tokens[pos].type == TokenType::SEMICOLON) {
            throw std::runtime_error("Module Error: Expected module name after 'using'");
        }
        std::string libName = tokens[pos++].value;
        if (pos < tokens.size() && tokens[pos].type == TokenType::DOT) {
            consume(TokenType::DOT);
            if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
                libName += "." + consume(TokenType::IDENTIFIER).value;
            }
        }
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<UsingNode>(libName, currentFile);
        imports.push_back({libName, true});
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::GLOBAL) {
        consume(TokenType::GLOBAL);
        std::string type;
        if (tokens[pos].type == TokenType::INT_KW) type = "int";
        else if (tokens[pos].type == TokenType::FLOAT_KW) type = "float";
        else if (tokens[pos].type == TokenType::STRING_KW) type = "string";
        else if (tokens[pos].type == TokenType::BOOL_KW) type = "bool";
        pos++;
        Token nameTok = consume(TokenType::IDENTIFIER);
        std::string name = nameTok.value;
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<GlobalVarDeclNode>(type, name, std::move(expr), nameTok.range);
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::INT_KW || tokens[pos].type == TokenType::FLOAT_KW ||
        tokens[pos].type == TokenType::STRING_KW || tokens[pos].type == TokenType::BOOL_KW ||
        tokens[pos].type == TokenType::VOID_KW) {
        std::string type = tokens[pos].value;
        pos++;
        Token nameTok = consume(TokenType::IDENTIFIER);
        std::string name = nameTok.value;

        // Function definition
        if (pos < tokens.size() && tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN);
            std::vector<FuncParam> params;
            if (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                while (true) {
                    std::string pType = tokens[pos].value;
                    pos++;
                    std::string pName = consume(TokenType::IDENTIFIER).value;
                    params.push_back({pType, pName});
                    if (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
                        consume(TokenType::COMMA);
                    } else {
                        break;
                    }
                }
            }
            consume(TokenType::RPAREN);
            auto body = parseBlock();
            SourcePosition endPos = tokens[pos - 1].range.end;
            auto node = std::make_unique<FuncDefNode>(type, name, params, std::move(body), nameTok.range);
            node->range = {startPos, endPos};
            return node;
        }

        // Variable declaration
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<VarDeclNode>(type, name, std::move(expr), nameTok.range);
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::RETURN) {
        consume(TokenType::RETURN);
        std::unique_ptr<Node> expr = nullptr;
        if (pos < tokens.size() && tokens[pos].type != TokenType::SEMICOLON) {
            expr = expression();
        }
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<ReturnNode>(std::move(expr));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::WHILE) {
        consume(TokenType::WHILE);
        consume(TokenType::LPAREN);
        auto cond = logicalOr();
        consume(TokenType::RPAREN);
        auto body = parseBlock();
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<WhileNode>(std::move(cond), std::move(body));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::FOR) {
        consume(TokenType::FOR);
        consume(TokenType::LPAREN);

        std::unique_ptr<Node> init = nullptr;
        if (tokens[pos].type != TokenType::SEMICOLON) {
            init = statement();
        } else {
            consume(TokenType::SEMICOLON);
        }

        std::unique_ptr<Node> cond = nullptr;
        if (tokens[pos].type != TokenType::SEMICOLON) {
            cond = comparison();
        } else {
            cond = std::make_unique<NumberNode>("1");
        }
        consume(TokenType::SEMICOLON);

        std::unique_ptr<Node> step = nullptr;
        if (tokens[pos].type != TokenType::RPAREN) {
            if (tokens[pos].type == TokenType::IDENTIFIER && pos + 1 < tokens.size() &&
                (tokens[pos+1].type == TokenType::ASSIGN || tokens[pos+1].type == TokenType::PLUS_ASSIGN ||
                 tokens[pos+1].type == TokenType::MINUS_ASSIGN || tokens[pos+1].type == TokenType::STAR_ASSIGN ||
                 tokens[pos+1].type == TokenType::SLASH_ASSIGN)) {
                Token nameTok = consume(TokenType::IDENTIFIER);
                std::string name = nameTok.value;
                auto opToken = tokens[pos++];
                auto expr = expression();
                if (opToken.type != TokenType::ASSIGN) {
                    expr = std::make_unique<BinOpNode>(opToken.value, std::make_unique<VarAccessNode>(name, nameTok.range), std::move(expr));
                }
                step = std::make_unique<VarAssignNode>(name, std::move(expr), nameTok.range);
            } else {
                step = expression();
            }
        }
        consume(TokenType::RPAREN);

        auto body = parseBlock();
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<ForNode>(std::move(init), std::move(cond), std::move(step), std::move(body));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::IF) {
        consume(TokenType::IF);
        consume(TokenType::LPAREN);
        auto cond = logicalOr();
        consume(TokenType::RPAREN);
        auto thenB = parseBlock();
        std::unique_ptr<Node> elseB = nullptr;
        if (pos < tokens.size() && tokens[pos].type == TokenType::ELSE) {
            consume(TokenType::ELSE);
            if (pos < tokens.size() && tokens[pos].type == TokenType::IF) {
                elseB = statement();
            } else {
                elseB = parseBlock();
            }
        }
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<IfNode>(std::move(cond), std::move(thenB), std::move(elseB));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::BREAK) {
        consume(TokenType::BREAK);
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<BreakNode>();
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::CONTINUE) {
        consume(TokenType::CONTINUE);
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<ContinueNode>();
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::WAIT) {
        consume(TokenType::WAIT);
        consume(TokenType::LPAREN);
        auto timeExpr = expression();
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<WaitNode>(std::move(timeExpr));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SWITCH) {
        consume(TokenType::SWITCH);
        consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN);
        consume(TokenType::LBRACE);

        auto switchNode = std::make_unique<SwitchNode>(std::move(expr));

        while (pos < tokens.size() && (tokens[pos].type == TokenType::CASE || tokens[pos].type == TokenType::DEFAULT)) {
            if (tokens[pos].type == TokenType::CASE) {
                consume(TokenType::CASE);
                auto caseValue = expression();
                consume(TokenType::COLON);

                std::vector<std::unique_ptr<Node>> caseStatements;
                while (pos < tokens.size() &&
                       tokens[pos].type != TokenType::CASE &&
                       tokens[pos].type != TokenType::DEFAULT &&
                       tokens[pos].type != TokenType::RBRACE) {
                    caseStatements.push_back(statement());
                }

                auto caseBody = std::make_unique<BlockNode>();
                for (auto& stmt : caseStatements) {
                    caseBody->stmts.push_back(std::move(stmt));
                }

                switchNode->cases.push_back(std::make_pair(std::move(caseValue), std::move(caseBody)));
            } else if (tokens[pos].type == TokenType::DEFAULT) {
                consume(TokenType::DEFAULT);
                consume(TokenType::COLON);

                std::vector<std::unique_ptr<Node>> defaultStatements;
                while (pos < tokens.size() && tokens[pos].type != TokenType::RBRACE) {
                    defaultStatements.push_back(statement());
                }

                auto defaultBody = std::make_unique<BlockNode>();
                for (auto& stmt : defaultStatements) {
                    defaultBody->stmts.push_back(std::move(stmt));
                }

                switchNode->defaultCase = std::move(defaultBody);
            }
        }

        consume(TokenType::RBRACE);
        SourcePosition endPos = tokens[pos - 1].range.end;
        switchNode->range = {startPos, endPos};
        return switchNode;
    }

    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT);
        consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(expr));
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("print", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::FOX) {
        consume(TokenType::FOX);
        consume(TokenType::LPAREN);
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        std::vector<std::unique_ptr<Node>> args;
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<FuncCallNode>("fox", std::move(args));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::ARRAY) {
        consume(TokenType::ARRAY);
        std::string name = consume(TokenType::IDENTIFIER).value;
        auto sizeNode = expression();
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<ArrayDeclNode>(name, std::move(sizeNode));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::SET) {
        consume(TokenType::SET);
        consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::COMMA);
        auto idx = expression();
        consume(TokenType::COMMA);
        auto val = expression();
        consume(TokenType::RPAREN);
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        auto node = std::make_unique<ArraySetNode>(name, std::move(idx), std::move(val));
        node->range = {startPos, endPos};
        return node;
    }

    if (tokens[pos].type == TokenType::IDENTIFIER) {
        if (pos + 1 < tokens.size() &&
            (tokens[pos+1].type == TokenType::ASSIGN || tokens[pos+1].type == TokenType::PLUS_ASSIGN ||
             tokens[pos+1].type == TokenType::MINUS_ASSIGN || tokens[pos+1].type == TokenType::STAR_ASSIGN ||
             tokens[pos+1].type == TokenType::SLASH_ASSIGN)) {
            Token nameTok = consume(TokenType::IDENTIFIER);
            std::string name = nameTok.value;
            auto opToken = tokens[pos++];
            auto expr = expression();
            consume(TokenType::SEMICOLON);
            if (opToken.type != TokenType::ASSIGN) {
                expr = std::make_unique<BinOpNode>(opToken.value, std::make_unique<VarAccessNode>(name, nameTok.range), std::move(expr));
            }
            SourcePosition endPos = tokens[pos - 1].range.end;
            auto node = std::make_unique<VarAssignNode>(name, std::move(expr), nameTok.range);
            node->range = {startPos, endPos};
            return node;
        }

        if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::LPAREN) {
            Token nameTok = consume(TokenType::IDENTIFIER);
            std::string name = nameTok.value;
            consume(TokenType::LPAREN);
            std::vector<std::unique_ptr<Node>> args;
            if (pos < tokens.size() && tokens[pos].type != TokenType::RPAREN) {
                args.push_back(expression());
                while (pos < tokens.size() && tokens[pos].type == TokenType::COMMA) {
                    consume(TokenType::COMMA);
                    args.push_back(expression());
                }
            }
            consume(TokenType::RPAREN);
            consume(TokenType::SEMICOLON);
            SourcePosition endPos = tokens[pos - 1].range.end;
            auto node = std::make_unique<FuncCallNode>(name, std::move(args), nameTok.range);
            node->range = {startPos, endPos};
            return node;
        }

        if (pos + 1 < tokens.size() && tokens[pos+1].type == TokenType::INC) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::INC);
            consume(TokenType::SEMICOLON);
            SourcePosition endPos = tokens[pos - 1].range.end;
            auto node = std::make_unique<PostIncNode>(name);
            node->range = {startPos, endPos};
            return node;
        }
    }

    // Built-in function calls as statements
    if (tokens[pos].type == TokenType::SERVER_START ||
        tokens[pos].type == TokenType::SERVER_STOP ||
        tokens[pos].type == TokenType::ROUTE_GET ||
        tokens[pos].type == TokenType::ROUTE_POST ||
        tokens[pos].type == TokenType::SEND_RESPONSE ||
        tokens[pos].type == TokenType::HTTP_GET ||
        tokens[pos].type == TokenType::HTTP_POST ||
        tokens[pos].type == TokenType::HTTP_PUT ||
        tokens[pos].type == TokenType::HTTP_DELETE) {
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        SourcePosition endPos = tokens[pos - 1].range.end;
        expr->range = {startPos, endPos};
        return expr;
    }

    if (tokens[pos].type == TokenType::LBRACE) {
        return parseBlock();
    }

    throw std::runtime_error("Syntax Error: Unknown statement '" + tokens[pos].value + "' at line " + std::to_string(tokens[pos].line));
}

void Parser::run() {
    auto program = parseProgram();
    try {
        program->eval(globalContext);
    } catch (const BreakException&) {
        throw std::runtime_error("Runtime Error: 'break' outside of loop in global scope");
    } catch (const ContinueException&) {
        throw std::runtime_error("Runtime Error: 'continue' outside of loop in global scope");
    }
}

} // namespace foxlang
