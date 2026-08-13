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
    throw std::runtime_error("Syntax Error: Expected token " + std::to_string((int)type) + 
                         " got '" + tokens[pos].value + "' line " + std::to_string(tokens[pos].line));
}

std::unique_ptr<Node> Parser::primary() {
    // 1. УНАРНЫЙ МИНУС (Обработка отрицательных чисел: -5, -var)
    if (tokens[pos].type == TokenType::MINUS) {
        consume(TokenType::MINUS);
        // Превращаем -5 в (0 - 5)
        return std::make_unique<BinOpNode>("-", 
            std::make_unique<NumberNode>("0"), 
            primary() // Рекурсивно вызываем primary, чтобы считать само число или скобку
        );
    }

    // 1.5. УНАРНЫЙ НЕ (Обработка !condition)
    if (tokens[pos].type == TokenType::NOT) {
        consume(TokenType::NOT);
        return std::make_unique<UnaryOpNode>("!", primary());
    }

    // 2. Числа
    if (tokens[pos].type == TokenType::NUMBER) {
        return std::make_unique<NumberNode>(consume(TokenType::NUMBER).value);
    }

    // 3. Строки
    if (tokens[pos].type == TokenType::STRING_LITERAL) {
        return std::make_unique<StringNode>(consume(TokenType::STRING_LITERAL).value);
    }
    
    // 4. Boolean литералы
    if (tokens[pos].type == TokenType::TRUE_KW) {
        consume(TokenType::TRUE_KW);
        return std::make_unique<BoolNode>(true);
    }
    if (tokens[pos].type == TokenType::FALSE_KW) {
        consume(TokenType::FALSE_KW);
        return std::make_unique<BoolNode>(false);
    }
    
    // 5. Переменные и Вызовы функций
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
        
        // Post increment operator (expression context: x = i++ + 5)
        if (tokens[pos].type == TokenType::INC) {
            consume(TokenType::INC);
            return std::make_unique<PostIncNode>(name);
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
        consume(TokenType::INPUT); consume(TokenType::LPAREN);
        std::vector<std::unique_ptr<Node>> args;
        if (tokens[pos].type != TokenType::RPAREN) {
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        return std::make_unique<FuncCallNode>("input", std::move(args));
    }

    if (tokens[pos].type == TokenType::READ_FILE) {
        consume(TokenType::READ_FILE); consume(TokenType::LPAREN);
        auto filename = expression();
        consume(TokenType::RPAREN);
        return std::make_unique<ReadFileNode>(std::move(filename));
    }

    // HTTP функции в выражениях
    if (tokens[pos].type == TokenType::HTTP_GET) {
        consume(TokenType::HTTP_GET); consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        return std::make_unique<FuncCallNode>("httpget", std::move(args));
    }

    if (tokens[pos].type == TokenType::HTTP_POST) {
        consume(TokenType::HTTP_POST); consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::COMMA);
        auto data = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        args.push_back(std::move(data));
        if (tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        return std::make_unique<FuncCallNode>("httppost", std::move(args));
    }

    if (tokens[pos].type == TokenType::HTTP_PUT) {
        consume(TokenType::HTTP_PUT); consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::COMMA);
        auto data = expression();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        args.push_back(std::move(data));
        if (tokens[pos].type == TokenType::COMMA) {
            consume(TokenType::COMMA);
            args.push_back(expression());
        }
        consume(TokenType::RPAREN);
        return std::make_unique<FuncCallNode>("httpput", std::move(args));
    }

    if (tokens[pos].type == TokenType::HTTP_DELETE) {
        consume(TokenType::HTTP_DELETE); consume(TokenType::LPAREN);
        auto url = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(url));
        return std::make_unique<FuncCallNode>("httpdelete", std::move(args));
    }

    // Функции ввода
    if (tokens[pos].type == TokenType::GETCH) {
        consume(TokenType::GETCH); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        return std::make_unique<FuncCallNode>("getch", std::move(args));
    }

    if (tokens[pos].type == TokenType::KBHIT) {
        consume(TokenType::KBHIT); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        return std::make_unique<FuncCallNode>("kbhit", std::move(args));
    }

    // FastAPI функции
    if (tokens[pos].type == TokenType::SERVER_START) {
        consume(TokenType::SERVER_START); consume(TokenType::LPAREN);
        auto port = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(port));
        return std::make_unique<FuncCallNode>("server_start", std::move(args));
    }

    if (tokens[pos].type == TokenType::SERVER_STOP) {
        consume(TokenType::SERVER_STOP); consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        return std::make_unique<FuncCallNode>("server_stop", std::move(args));
    }

    if (tokens[pos].type == TokenType::ROUTE_GET) {
        consume(TokenType::ROUTE_GET); consume(TokenType::LPAREN);
        auto path = expression();
        consume(TokenType::COMMA);
        auto handler = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(path));
        args.push_back(std::move(handler));
        return std::make_unique<FuncCallNode>("route_get", std::move(args));
    }

    if (tokens[pos].type == TokenType::ROUTE_POST) {
        consume(TokenType::ROUTE_POST); consume(TokenType::LPAREN);
        auto path = expression();
        consume(TokenType::COMMA);
        auto handler = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(path));
        args.push_back(std::move(handler));
        return std::make_unique<FuncCallNode>("route_post", std::move(args));
    }

    if (tokens[pos].type == TokenType::SEND_RESPONSE) {
        consume(TokenType::SEND_RESPONSE); consume(TokenType::LPAREN);
        auto response = expression();
        consume(TokenType::RPAREN);
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(response));
        return std::make_unique<FuncCallNode>("send_response", std::move(args));
    }

    if (tokens[pos].type == TokenType::LPAREN) { 
        consume(TokenType::LPAREN); 
        auto n = expression(); 
        consume(TokenType::RPAREN); 
        return n; 
    }
    
    throw std::runtime_error("Parser Error: Unexpected token '" + tokens[pos].value + 
                         "' at line " + std::to_string(tokens[pos].line));
}

std::unique_ptr<Node> Parser::multiplication() {
    auto node = primary();
    while (tokens[pos].type == TokenType::STAR || tokens[pos].type == TokenType::SLASH || tokens[pos].type == TokenType::MOD) {
        std::string op = tokens[pos].value; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), primary());
    }
    return node;
}

std::unique_ptr<Node> Parser::expression() {
    auto node = multiplication();
    while (tokens[pos].type == TokenType::PLUS || tokens[pos].type == TokenType::MINUS) {
        std::string op = tokens[pos].value; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), multiplication());
    }
    return node;
}

std::unique_ptr<Node> Parser::comparison() {
    auto node = expression();
    if (tokens[pos].type == TokenType::EQ || tokens[pos].type == TokenType::NEQ || 
        tokens[pos].type == TokenType::LT || tokens[pos].type == TokenType::GT ||
        tokens[pos].type == TokenType::LTE || tokens[pos].type == TokenType::GTE) {
        std::string op = tokens[pos].value; pos++;
        return std::make_unique<BinOpNode>(op, std::move(node), expression());
    }
    return node;
}

std::unique_ptr<Node> Parser::logicalAnd() {
    auto node = comparison();
    while (tokens[pos].type == TokenType::AND) {
        std::string op = tokens[pos].value; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), comparison());
    }
    return node;
}

std::unique_ptr<Node> Parser::logicalOr() {
    auto node = logicalAnd();
    while (tokens[pos].type == TokenType::OR) {
        std::string op = tokens[pos].value; pos++;
        node = std::make_unique<BinOpNode>(op, std::move(node), logicalAnd());
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
    
    // Если не нашли по полному пути, ищем рядом с исполняемым файлом
    if (!file.is_open()) { 
        file.open(filename); 
        if(!file.is_open()) {
            throw std::runtime_error("Include Error: File '" + filename + "' not found.");
        }
    }

    std::stringstream buffer; 
    buffer << file.rdbuf();
    
    Lexer lexer(buffer.str());
    std::vector<Token> tokens = lexer.tokenize(); 

    Parser parser(tokens);
    
    // Копируем контекст и устанавливаем режим импорта
    parser.globalContext = ctx; 
    parser.currentFile = fullPath; 
    parser.importMode = true; // Только парсим функции, не выполняем код
    
    parser.run(); 
    
    // Возвращаем обновленный контекст с новыми функциями
    ctx = parser.globalContext; 
}

void Parser::processUsing(const std::string& libName, Context& ctx) {
    if (libName == "net.fox" || libName == "net") {
        // Добавляем сетевые функции в контекст
        // Эти функции будут доступны как встроенные
        // Реализация уже есть в FuncCallNode
    }
}

std::unique_ptr<Node> Parser::statement() {
    if (tokens[pos].type == TokenType::INCLUDE) {
        consume(TokenType::INCLUDE); consume(TokenType::LPAREN);
        std::string file = consume(TokenType::STRING_LITERAL).value;
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        processInclude(file, globalContext, currentFile);
        return std::make_unique<BlockNode>(); 
    }

    if (tokens[pos].type == TokenType::USING) {
        consume(TokenType::USING);
        std::string libName = consume(TokenType::IDENTIFIER).value;
        if (pos < tokens.size() && tokens[pos].type == TokenType::DOT) {
            consume(TokenType::DOT);
            if (pos < tokens.size() && tokens[pos].type == TokenType::IDENTIFIER) {
                libName += "." + consume(TokenType::IDENTIFIER).value;
            }
        }
        consume(TokenType::SEMICOLON);
        processUsing(libName, globalContext);
        return std::make_unique<UsingNode>(libName);
    }

    if (tokens[pos].type == TokenType::GLOBAL) {
        consume(TokenType::GLOBAL);
        std::string type;
        if (tokens[pos].type == TokenType::INT_KW) type = "int";
        else if (tokens[pos].type == TokenType::FLOAT_KW) type = "float";
        else if (tokens[pos].type == TokenType::STRING_KW) type = "string";
        else if (tokens[pos].type == TokenType::BOOL_KW) type = "bool";
        pos++;
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<GlobalVarDeclNode>(type, name, std::move(expr));
    }

    if (tokens[pos].type == TokenType::INT_KW || tokens[pos].type == TokenType::FLOAT_KW || tokens[pos].type == TokenType::STRING_KW || tokens[pos].type == TokenType::BOOL_KW || tokens[pos].type == TokenType::VOID_KW) {
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
        return std::make_unique<ReturnNode>(std::move(expr));
    }

    if (tokens[pos].type == TokenType::WHILE) {
        consume(TokenType::WHILE); consume(TokenType::LPAREN);
        auto cond = logicalOr(); consume(TokenType::RPAREN);
        auto body = parseBlock();
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<WhileNode>(std::move(cond), std::move(body));
    }

    if (tokens[pos].type == TokenType::FOR) {
        consume(TokenType::FOR); consume(TokenType::LPAREN);
        
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
            if (tokens[pos].type == TokenType::IDENTIFIER && (tokens[pos+1].type == TokenType::ASSIGN || tokens[pos+1].type == TokenType::PLUS_ASSIGN || tokens[pos+1].type == TokenType::MINUS_ASSIGN || tokens[pos+1].type == TokenType::STAR_ASSIGN || tokens[pos+1].type == TokenType::SLASH_ASSIGN)) {
                 std::string name = consume(TokenType::IDENTIFIER).value;
                 auto opToken = tokens[pos++]; // ASSIGN, PLUS_ASSIGN, etc.
                 auto expr = expression();
                 if (opToken.type != TokenType::ASSIGN) {
                     expr = std::make_unique<BinOpNode>(opToken.value, std::make_unique<VarAccessNode>(name), std::move(expr));
                 }
                 step = std::make_unique<VarAssignNode>(name, std::move(expr));
            } else {
                 step = expression();
            }
        }
        consume(TokenType::RPAREN);
        
        auto body = parseBlock();
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<ForNode>(std::move(init), std::move(cond), std::move(step), std::move(body));
    }
    
    if (tokens[pos].type == TokenType::IF) {
        consume(TokenType::IF); consume(TokenType::LPAREN);
        auto cond = logicalOr(); consume(TokenType::RPAREN);
        auto thenB = parseBlock();
        std::unique_ptr<Node> elseB = nullptr;
        if (tokens[pos].type == TokenType::ELSE) {
            consume(TokenType::ELSE);
            if (tokens[pos].type == TokenType::IF) elseB = statement(); else elseB = parseBlock();
        }
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<IfNode>(std::move(cond), std::move(thenB), std::move(elseB));
    }

    if (tokens[pos].type == TokenType::BREAK) {
        consume(TokenType::BREAK); consume(TokenType::SEMICOLON);
        return std::make_unique<BreakNode>();
    }

    if (tokens[pos].type == TokenType::CONTINUE) {
        consume(TokenType::CONTINUE); consume(TokenType::SEMICOLON);
        return std::make_unique<ContinueNode>();
    }

    if (tokens[pos].type == TokenType::WAIT) {
        consume(TokenType::WAIT); consume(TokenType::LPAREN);
        auto timeExpr = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        return std::make_unique<WaitNode>(std::move(timeExpr));
    }

    if (tokens[pos].type == TokenType::SWITCH) {
        consume(TokenType::SWITCH); consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN); consume(TokenType::LBRACE);
        
        auto switchNode = std::make_unique<SwitchNode>(std::move(expr));
        
        while (tokens[pos].type == TokenType::CASE || tokens[pos].type == TokenType::DEFAULT) {
            if (tokens[pos].type == TokenType::CASE) {
                consume(TokenType::CASE);
                auto caseValue = expression();
                consume(TokenType::COLON);
                
                std::vector<std::unique_ptr<Node>> caseStatements;
                while (tokens[pos].type != TokenType::CASE && 
                       tokens[pos].type != TokenType::DEFAULT && 
                       tokens[pos].type != TokenType::RBRACE) {
                    caseStatements.push_back(statement());
                }
                
                auto caseBody = std::make_unique<BlockNode>();
                for (auto& stmt : caseStatements) {
                    static_cast<BlockNode*>(caseBody.get())->stmts.push_back(std::move(stmt));
                }
                
                switchNode->cases.push_back(std::make_pair(std::move(caseValue), std::move(caseBody)));
            } else if (tokens[pos].type == TokenType::DEFAULT) {
                consume(TokenType::DEFAULT); consume(TokenType::COLON);
                
                std::vector<std::unique_ptr<Node>> defaultStatements;
                while (tokens[pos].type != TokenType::RBRACE) {
                    defaultStatements.push_back(statement());
                }
                
                auto defaultBody = std::make_unique<BlockNode>();
                for (auto& stmt : defaultStatements) {
                    static_cast<BlockNode*>(defaultBody.get())->stmts.push_back(std::move(stmt));
                }
                
                switchNode->defaultCase = std::move(defaultBody);
            }
        }
        
        consume(TokenType::RBRACE);
        return std::move(switchNode);
    }

    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT); consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>();
        std::vector<std::unique_ptr<Node>> args;
        args.push_back(std::move(expr));
        return std::make_unique<FuncCallNode>("print", std::move(args));
    }
    
    if (tokens[pos].type == TokenType::FOX) {
         consume(TokenType::FOX); consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
         if (importMode) return std::make_unique<BlockNode>();
         std::vector<std::unique_ptr<Node>> args;
         return std::make_unique<FuncCallNode>("fox", std::move(args));
    }

    if (tokens[pos].type == TokenType::ARRAY) {
        consume(TokenType::ARRAY);
        std::string name = consume(TokenType::IDENTIFIER).value;
        auto sizeNode = expression();
        consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>();
        return std::make_unique<ArrayDeclNode>(name, std::move(sizeNode));
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
        if (tokens[pos+1].type == TokenType::ASSIGN || tokens[pos+1].type == TokenType::PLUS_ASSIGN || tokens[pos+1].type == TokenType::MINUS_ASSIGN || tokens[pos+1].type == TokenType::STAR_ASSIGN || tokens[pos+1].type == TokenType::SLASH_ASSIGN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            auto opToken = tokens[pos++]; // ASSIGN, PLUS_ASSIGN, etc.
            auto expr = expression();
            consume(TokenType::SEMICOLON);
            if (importMode) return std::make_unique<BlockNode>();
            if (opToken.type != TokenType::ASSIGN) {
                expr = std::make_unique<BinOpNode>(opToken.value, std::make_unique<VarAccessNode>(name), std::move(expr));
            }
            return std::make_unique<VarAssignNode>(name, std::move(expr));
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
        if (tokens[pos+1].type == TokenType::INC) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::INC);
            consume(TokenType::SEMICOLON);
            if (importMode) return std::make_unique<BlockNode>();
            return std::make_unique<PostIncNode>(name);
        }
    }

    if (tokens[pos].type == TokenType::LBRACE) {
        return parseBlock();
    }

    throw std::runtime_error("Unknown statement " + tokens[pos].value);
}

void Parser::run() {
    try {
        while (tokens[pos].type != TokenType::END) {
            statement()->eval(globalContext);
        }
    } catch (const BreakException&) {
        throw std::runtime_error("Runtime Error: 'break' outside of loop in global scope");
    } catch (const ContinueException&) {
        throw std::runtime_error("Runtime Error: 'continue' outside of loop in global scope");
    }
}