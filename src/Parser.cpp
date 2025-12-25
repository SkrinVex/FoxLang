#include "Parser.h"
#include "Lexer.h" 
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem> // Нужно для C++17, чтобы работать с путями красиво

// Хелпер для получения папки из пути к файлу
// Пример: "test/main.fox" -> "test/"
// Пример: "script.fox" -> "./"
std::string getDirectory(const std::string& filepath) {
    size_t found = filepath.find_last_of("/\\");
    if (found == std::string::npos) return "./";
    return filepath.substr(0, found + 1);
}

Parser::Parser(std::vector<Token> t) : tokens(t) {}

Token Parser::consume(TokenType type) {
    if (tokens[pos].type == type) return tokens[pos++];
    std::cerr << "Syntax Error: Expected token type " << (int)type 
              << " but found '" << tokens[pos].value 
              << "' on line " << tokens[pos].line << std::endl;
    exit(1);
}

// --- ВЫРАЖЕНИЯ (Остаются без изменений) ---
std::unique_ptr<Node> Parser::primary() {
    if (tokens[pos].type == TokenType::NUMBER) return std::make_unique<NumberNode>(consume(TokenType::NUMBER).value);
    if (tokens[pos].type == TokenType::STRING_LITERAL) return std::make_unique<StringNode>(consume(TokenType::STRING_LITERAL).value);
    
    if (tokens[pos].type == TokenType::GET) {
        consume(TokenType::GET); consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::COMMA); auto idx = expression();
        consume(TokenType::RPAREN); return std::make_unique<ArrayGetNode>(name, std::move(idx));
    }
    if (tokens[pos].type == TokenType::SIZE) {
        consume(TokenType::SIZE); consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::RPAREN); return std::make_unique<ArraySizeNode>(name);
    }
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        std::string name = consume(TokenType::IDENTIFIER).value;
        if (tokens[pos].type == TokenType::LPAREN) {
            consume(TokenType::LPAREN); consume(TokenType::RPAREN);
            return std::make_unique<FuncCallNode>(name);
        }
        return std::make_unique<VarAccessNode>(name);
    }
    if (tokens[pos].type == TokenType::INPUT) { consume(TokenType::INPUT); consume(TokenType::LPAREN); consume(TokenType::RPAREN); return std::make_unique<InputNode>(); }
    if (tokens[pos].type == TokenType::ROUND) { consume(TokenType::ROUND); consume(TokenType::LPAREN); auto n = expression(); consume(TokenType::RPAREN); return std::make_unique<RoundNode>(std::move(n)); }
    if (tokens[pos].type == TokenType::RANDOM) { consume(TokenType::RANDOM); consume(TokenType::LPAREN); consume(TokenType::RPAREN); return std::make_unique<RandomNode>(); }
    if (tokens[pos].type == TokenType::LPAREN) { consume(TokenType::LPAREN); auto n = expression(); consume(TokenType::RPAREN); return n; }
    
    std::cerr << "Error: Unexpected token '" << tokens[pos].value << "' line " << tokens[pos].line << std::endl;
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
        block->statements.push_back(statement());
    }
    consume(TokenType::RBRACE);
    return block;
}

// --- ЛОГИКА ИМПОРТА ---
void processInclude(std::string filename, Context& ctx, std::string currentFile) {
    // 1. Вычисляем путь: Папка текущего файла + Имя подключаемого
    std::string dir = getDirectory(currentFile);
    std::string fullPath = dir + filename; 

    // Открываем файл
    std::ifstream file(fullPath);
    if (!file.is_open()) { 
        // Если не нашли, пробуем искать просто по имени (в текущей рабочей папке)
        file.open(filename);
        if (!file.is_open()) {
            std::cerr << "Include Error: File '" << fullPath << "' (or '" << filename << "') not found!" << std::endl; 
            exit(1); 
        }
        fullPath = filename; // Если нашли локально, обновляем путь
    }

    std::stringstream buffer; buffer << file.rdbuf();
    Lexer lexer(buffer.str());
    auto tokens = lexer.tokenize();
    
    Parser parser(tokens);
    parser.globalContext = ctx; // Передаем память
    parser.currentFile = fullPath; // Указываем новый файл как текущий
    
    // ВАЖНО: Включаем режим импорта!
    // Это заставит парсер игнорировать вызовы функций в mainLogic
    parser.importMode = true; 
    
    parser.run();
    ctx = parser.globalContext; // Забираем обновленную память
}

std::unique_ptr<Node> Parser::statement() {
    // 1. INCLUDE
    if (tokens[pos].type == TokenType::INCLUDE) {
        consume(TokenType::INCLUDE); consume(TokenType::LPAREN);
        std::string file = consume(TokenType::STRING_LITERAL).value;
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        
        // ВАЖНО: Передаем текущий файл, чтобы вычислить относительный путь
        processInclude(file, globalContext, currentFile);
        
        return std::make_unique<BlockNode>(); 
    }

    // 2. ОБЪЯВЛЕНИЕ ПЕРЕМЕННЫХ (Всегда выполняем, даже при импорте!)
    if (tokens[pos].type == TokenType::INT_KW || tokens[pos].type == TokenType::STRING_KW) {
        std::string type = tokens[pos].value; pos++;
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::ASSIGN);
        auto expr = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<VarDeclNode>(type, name, std::move(expr));
    }

    // 3. ФУНКЦИИ (Всегда выполняем, то есть запоминаем их)
    if (tokens[pos].type == TokenType::VOID_KW) {
        consume(TokenType::VOID_KW);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::LPAREN); consume(TokenType::RPAREN);
        auto body = parseBlock(); 
        return std::make_unique<FuncDefNode>(name, std::move(body));
    }

    // 4. МАССИВЫ (Выполняем, глобальные массивы нужны)
    if (tokens[pos].type == TokenType::ARRAY) {
        consume(TokenType::ARRAY);
        std::string name = consume(TokenType::IDENTIFIER).value;
        auto size = expression();
        consume(TokenType::SEMICOLON);
        return std::make_unique<ArrayDeclNode>(name, std::move(size));
    }

    // =========================================================
    // БЛОК ИГНОРИРОВАНИЯ (Код ниже НЕ выполняется при include)
    // =========================================================

    // 5. WHILE
    if (tokens[pos].type == TokenType::WHILE) {
        consume(TokenType::WHILE); consume(TokenType::LPAREN);
        auto cond = comparison(); 
        consume(TokenType::RPAREN);
        auto body = parseBlock();
        
        if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор
        return std::make_unique<WhileNode>(std::move(cond), std::move(body));
    }

    // 6. IF
    if (tokens[pos].type == TokenType::IF) {
        consume(TokenType::IF); consume(TokenType::LPAREN);
        auto cond = comparison();
        consume(TokenType::RPAREN);
        auto thenB = parseBlock();
        std::unique_ptr<Node> elseB = nullptr;
        if (tokens[pos].type == TokenType::ELSE) {
            consume(TokenType::ELSE);
            if (tokens[pos].type == TokenType::IF) elseB = statement(); 
            else elseB = parseBlock();
        }

        if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор
        return std::make_unique<IfNode>(std::move(cond), std::move(thenB), std::move(elseB));
    }

    // 7. PRINT
    if (tokens[pos].type == TokenType::PRINT) {
        consume(TokenType::PRINT); consume(TokenType::LPAREN);
        auto expr = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);

        if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор
        return std::make_unique<PrintNode>(std::move(expr));
    }

    // 8. FOX
    if (tokens[pos].type == TokenType::FOX) {
        consume(TokenType::FOX); consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор
        return std::make_unique<FoxNode>(); 
    }

    // 9. SET (Arrays)
    if (tokens[pos].type == TokenType::SET) {
        consume(TokenType::SET); consume(TokenType::LPAREN);
        std::string name = consume(TokenType::IDENTIFIER).value;
        consume(TokenType::COMMA); auto idx = expression();
        consume(TokenType::COMMA); auto val = expression();
        consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
        if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор
        return std::make_unique<ArraySetNode>(name, std::move(idx), std::move(val));
    }

    // 10. IDENTIFIER (Присваивание или Вызов функции)
    if (tokens[pos].type == TokenType::IDENTIFIER) {
        if (tokens[pos+1].type == TokenType::ASSIGN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::ASSIGN);
            auto expr = expression();
            consume(TokenType::SEMICOLON);
            
            if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор присваивания
            return std::make_unique<AssignNode>(name, std::move(expr));
        }
        if (tokens[pos+1].type == TokenType::LPAREN) {
            std::string name = consume(TokenType::IDENTIFIER).value;
            consume(TokenType::LPAREN); consume(TokenType::RPAREN); consume(TokenType::SEMICOLON);
            
            if (importMode) return std::make_unique<BlockNode>(); // <-- Игнор вызова (mainLogic не сработает)
            return std::make_unique<FuncCallNode>(name);
        }
    }

    if (tokens[pos].type == TokenType::LBRACE) return parseBlock();

    std::cerr << "Syntax Error: Unknown statement '" << tokens[pos].value << "' at line " << tokens[pos].line << std::endl;
    exit(1);
}

void Parser::run() {
    while (tokens[pos].type != TokenType::END) {
        auto stmt = statement();
        // stmt->eval вызывается, но если мы вернули BlockNode (пустышку) при импорте,
        // то ничего не произойдет.
        stmt->eval(globalContext);
    }
}