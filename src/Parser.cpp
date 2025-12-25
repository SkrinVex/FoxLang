#include "Parser.h"
#include "Lexer.h" 
#include <iostream>
#include <fstream>
#include <sstream>

std::string getDirectory(const std::string& filepath) {
    size_t found = filepath.find_last_of("/\\");
    if (found == std::string::npos) return "./";
    return filepath.substr(0, found + 1);
}

Parser::Parser(std::vector<Token> t) : tokens(t) {}

Token Parser::consume(TokenType type) {
    if (tokens[pos].type == type) return tokens[pos++];
    std::cerr << "Syntax Error: Expected token " << (int)type 
              << " got '" << tokens[pos].value << "' line " << tokens[pos].line << std::endl;
    exit(1);
}

std::unique_ptr<Node> Parser::primary() {
    // 1. УНАРНЫЙ МИНУС (Обработка отрицательных чисел: -5, -var)
    if (tokens[pos].type == TokenType::MINUS) {
        consume(TokenType::MINUS);
        // Превращаем -5 в (0 - 5)
        return std::make_unique<BinOpNode>('-', 
            std::make_unique<NumberNode>("0"), 
            primary() // Рекурсивно вызываем primary, чтобы считать само число или скобку
        );
    }

    // 2. Числа
    if (tokens[pos].type == TokenType::NUMBER) {
        return std::make_unique<NumberNode>(consume(TokenType::NUMBER).value);
    }

    // 3. Строки
    if (tokens[pos].type == TokenType::STRING_LITERAL) {
        return std::make_unique<StringNode>(consume(TokenType::STRING_LITERAL).value);
    }
    
    // 4. Переменные и Вызовы функций
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        std::string name = consume(TokenType::IDENTIFIER).value;
        
        // Если дальше скобка '(', значит это ВЫЗОВ ФУНКЦИИ
        if (tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN);
            std::vector<std::unique_ptr<Node>> args;
            if (tokens[pos].type != TokenType::RPAREN) {
                args.push_back(expression());
                while (tokens[pos].type == TokenType::COMMA) {
                    consume(TokenType::COMMA);
                    args.push_back(expression());
                }
            }
            consume(TokenType::RPAREN);
            return std::make_unique<FuncCallNode>(name, std::move(args));
        }
        // Иначе это просто доступ к переменной
        return std::make_unique<VarAccessNode>(name);
    }

    // Остальные проверки (массивы, input, скобки)
    if (tokens[pos].type == TokenType::GET) {
        consume(TokenType::GET); consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::COMMA); auto idx = expression();
        consume(TokenType::RPAREN); return std::make_unique<ArrayGetNode>(name, std::move(idx));
    }
    
    if (tokens[pos].type == TokenType::INPUT) { 
        consume(TokenType::INPUT); consume(TokenType::LPAREN); consume(TokenType::RPAREN); 
        return std::make_unique<InputNode>(); 
    }

    if (tokens[pos].type == TokenType::LPAREN) { 
        consume(TokenType::LPAREN); 
        auto n = expression(); 
        consume(TokenType::RPAREN); 
        return n; 
    }
    
    std::cerr << "Parser Error: Unexpected token '" << tokens[pos].value 
              << "' at line " << tokens[pos].line << std::endl;
    exit(1);
}

std::unique_ptr<Node> Parser::multiplication() {
    auto node = primary();
    while (tokens[pos].type == TokenType::STAR || tokens[pos].type == TokenType::SLASH || tokens[pos].type == TokenType::MOD) {
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

std::unique_ptr<Node> Parser::comparison() {
    auto node = expression();
    if (tokens[pos].type == TokenType::EQ || tokens[pos].type == TokenType::NEQ || 
        tokens[pos].type == TokenType::LT || tokens[pos].type == TokenType::GT) {
        std::string op = tokens[pos].value; pos++;
        return std::make_unique<CompareNode>(op, std::move(node), expression());
    }
    return node;
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    consume(TokenType::LBRACE);
    auto block = std::make_unique<BlockNode>();
    while (tokens[pos].type != TokenType::RBRACE && tokens[pos].type != TokenType::END) {
        block->stmts.push_back(statement());
    }
    consume(TokenType::RBRACE);
    return block;
}

void processInclude(std::string filename, Context& ctx, std::string currentFile) {
    std::string dir = getDirectory(currentFile);
    std::string fullPath = dir + filename; 
    std::ifstream file(fullPath);
    if (!file.is_open()) { file.open(filename); if(!file.is_open()) exit(1); }
    std::stringstream buffer; buffer << file.rdbuf();
    Lexer lexer(buffer.str());
    Parser parser(lexer.tokenize());
    parser.globalContext = ctx; parser.currentFile = fullPath; parser.importMode = true; 
    parser.run(); ctx = parser.globalContext; 
}

std::unique_ptr<Node> Parser::statement() {
    if (tokens[pos].type == TokenType::INCLUDE) {
        consume(TokenType::INCLUDE); consume(TokenType::LPAREN);
        std::string file = consume(TokenType::STRING_LITERAL).value;
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        processInclude(file, globalContext, currentFile);
        return std::make_unique<BlockNode>(); 
    }

    if (tokens[pos].type == TokenType::GLOBAL) {
        consume(TokenType::GLOBAL);
        std::string type;
        if (tokens[pos].type == TokenType::INT_KW) type = "int";
        else if (tokens[pos].type == TokenType::STRING_KW) type = "string";
        pos++;
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<GlobalVarDeclNode>(type, name, std::move(expr));
    }

    if (tokens[pos].type == TokenType::INT_KW || tokens[pos].type == TokenType::STRING_KW || tokens[pos].type == TokenType::VOID_KW) {
        std::string type = tokens[pos].value; pos++;
        std::string name = consume(TokenType::IDENTIFIER).value;
        
        // Определение функции
        if (tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN);
            std::vector<FuncParam> params;
            if (tokens[pos].type != TokenType::RPAREN) {
                while(true) {
                    std::string pType = tokens[pos].value; pos++;
                    std::string pName = consume(TokenType::IDENTIFIER).value;
                    params.push_back({pType, pName});
                    if (tokens[pos].type == TokenType::COMMA) consume(TokenType::COMMA); else break;
                }
            }
            consume(TokenType::RPAREN);
            auto body = parseBlock();
            globalContext.defineFunc(name, std::make_shared<FuncDefNode>(type, name, params, std::move(body)));
            return std::make_unique<BlockNode>();
        }

        // Объявление переменной
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<VarDeclNode>(type, name, std::move(expr));
    }

    if (tokens[pos].type == TokenType::RETURN) {
        consume(TokenType::RETURN);
        std::unique_ptr<Node> expr = nullptr;
        if (tokens[pos].type != TokenType::SEMICOLON) expr = expression();
        consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<ReturnNode>(std::move(expr));
    }

    if (tokens[pos].type == TokenType::WHILE) {
        consume(TokenType::WHILE); consume(TokenType::LPAREN);
        auto cond = comparison(); consume(TokenType::RPAREN);
        auto body = parseBlock();
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<WhileNode>(std::move(cond), std::move(body));
    }
    
    if (tokens[pos].type == TokenType::IF) {
        consume(TokenType::IF); consume(TokenType::LPAREN);
        auto cond = comparison(); consume(TokenType::RPAREN);
        auto thenB = parseBlock();
        std::unique_ptr<Node> elseB = nullptr;
        if (tokens[pos].type == TokenType::ELSE) {
            consume(TokenType::ELSE);
            if (tokens[pos].type == TokenType::IF) elseB = statement(); else elseB = parseBlock();
        }
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<IfNode>(std::move(cond), std::move(thenB), std::move(elseB));
    }

    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT); consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<PrintNode>(std::move(expr));
    }
    
    if (tokens[pos].type == TokenType::FOX) {
         consume(TokenType::FOX); consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
         if (importMode) return std::make_unique<BlockNode>();
         return std::make_unique<FoxNode>();
    }

    if (tokens[pos].type == TokenType::ARRAY) {
        consume(TokenType::ARRAY);
        std::string name = consume(TokenType::IDENTIFIER).value;
        auto size = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<ArrayDeclNode>(name, std::move(size));
    }
    
    if (tokens[pos].type == TokenType::SET) {
        consume(TokenType::SET); consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::COMMA); auto idx = expression();
        consume(TokenType::COMMA); auto val = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<ArraySetNode>(name, std::move(idx), std::move(val));
    }

    if (tokens[pos].type == TokenType::IDENTIFIER) {
        if (tokens[pos+1].type == TokenType::ASSIGN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::ASSIGN);
            auto expr = expression();
            consume(TokenType::SEMICOLON);
            if (importMode) return std::make_unique<BlockNode>();
            return std::make_unique<AssignNode>(name, std::move(expr));
        }
        if (tokens[pos+1].type == TokenType::LPAREN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::LPAREN);
            std::vector<std::unique_ptr<Node>> args;
            if (tokens[pos].type != TokenType::RPAREN) {
                args.push_back(expression());
                while (tokens[pos].type == TokenType::COMMA) {
                    consume(TokenType::COMMA);
                    args.push_back(expression());
                }
            }
            consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
            if (importMode) return std::make_unique<BlockNode>();
            return std::make_unique<FuncCallNode>(name, std::move(args));
        }
    }

    std::cerr << "Unknown statement " << tokens[pos].value << std::endl; exit(1);
}

void Parser::run() {
    while (tokens[pos].type != TokenType::END) {
        statement()->eval(globalContext);
    }
}