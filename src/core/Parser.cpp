#include "foxlang/Parser.h"
#include <stdexcept>
#include <utility>

namespace foxlang {

namespace {

bool isAssignment(TokenType type) {
    return type == TokenType::ASSIGN || type == TokenType::PLUS_ASSIGN || type == TokenType::MINUS_ASSIGN ||
           type == TokenType::STAR_ASSIGN || type == TokenType::SLASH_ASSIGN || type == TokenType::MOD_ASSIGN;
}

template <typename NodeType>
std::unique_ptr<NodeType> at(std::unique_ptr<NodeType> node, SourcePosition start, SourcePosition end) {
    node->range = {start, end};
    return node;
}

} // namespace

Parser::Parser(std::vector<Token> t, std::string curFile)
    : currentFile(std::move(curFile)), tokens(std::move(t)), file(runtime::internFile(currentFile)) {
    if (tokens.empty() || tokens.back().type != TokenType::END) tokens.push_back({TokenType::END, "", 1, 1});
}

const Token& Parser::peek(size_t offset) const {
    return pos + offset < tokens.size() ? tokens[pos + offset] : tokens.back();
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    ++pos;
    return true;
}

void Parser::fail(const std::string& message) const {
    const Token& token = peek();
    bool atEnd = token.type == TokenType::END;
    std::string found = atEnd ? "end of file" : "'" + token.value + "'";
    // At the end of the file, the line that is missing something is the last one written.
    int line = atEnd && pos > 0 ? tokens[pos - 1].line : token.line;
    throw SyntaxError("Syntax Error: " + message + " but found " + found, line);
}

Token Parser::consume(TokenType type) {
    if (!check(type)) fail(std::string("expected ") + tokenTypeName(type));
    return tokens[pos++];
}

SourcePosition Parser::previousEnd() const {
    return pos > 0 ? tokens[pos - 1].range.end : peek().range.start;
}

bool Parser::isTypeKeyword(size_t offset) const {
    switch (peek(offset).type) {
        case TokenType::INT_KW: case TokenType::FLOAT_KW: case TokenType::STRING_KW:
        case TokenType::BOOL_KW: case TokenType::VOID_KW: case TokenType::ARRAY: case TokenType::MAP_KW:
        case TokenType::FUNC_KW:
            return true;
        default:
            return false;
    }
}

bool Parser::isUserType(size_t offset) const {
    return peek(offset).type == TokenType::IDENTIFIER && peek(offset + 1).type == TokenType::IDENTIFIER;
}

// After `type name`: a parameter list followed by a body, not a parenthesized size.
bool Parser::looksLikeFunction() const {
    if (!check(TokenType::LPAREN)) return false;
    if (peek(1).type == TokenType::RPAREN) return peek(2).type == TokenType::LBRACE;
    return (isTypeKeyword(1) && peek(2).type == TokenType::IDENTIFIER) || isUserType(1);
}

void Parser::synchronize() {
    if (check(TokenType::END)) return;
    ++pos;
    while (!check(TokenType::END)) {
        TokenType previous = tokens[pos - 1].type;
        if (previous == TokenType::SEMICOLON || previous == TokenType::RBRACE) return;
        switch (peek().type) {
            case TokenType::INT_KW: case TokenType::FLOAT_KW: case TokenType::STRING_KW: case TokenType::BOOL_KW:
            case TokenType::VOID_KW: case TokenType::ARRAY: case TokenType::IF: case TokenType::WHILE:
            case TokenType::FOR: case TokenType::RETURN: case TokenType::SWITCH: case TokenType::GLOBAL:
            case TokenType::INCLUDE: case TokenType::USING: case TokenType::MAP_KW: case TokenType::FUNC_KW:
            case TokenType::STRUCT:
            case TokenType::TRY: case TokenType::THROW:
                return;
            default:
                ++pos;
        }
    }
}

std::unique_ptr<BlockNode> Parser::parseProgram() {
    auto program = std::make_unique<BlockNode>();
    program->scoped = false;
    program->file = file;
    SourcePosition start = peek().range.start;
    while (!check(TokenType::END)) program->stmts.push_back(statement());
    program->range = {start, previousEnd()};
    return program;
}

std::unique_ptr<Node> Parser::parseExpression() {
    auto node = expression();
    if (!check(TokenType::END)) fail("unexpected '" + peek().value + "' after the expression");
    return node;
}

std::unique_ptr<BlockNode> Parser::parseProgramWithDiagnostics(std::vector<Diagnostic>& outDiagnostics) {
    diagnostics.clear();
    auto program = std::make_unique<BlockNode>();
    program->scoped = false;
    program->file = file;
    SourcePosition start = peek().range.start;
    while (!check(TokenType::END)) {
        try {
            program->stmts.push_back(statement());
        } catch (const SyntaxError& e) {
            diagnostics.push_back({DiagnosticSeverity::Error, e.message(), peek().range});
            synchronize();
        } catch (const std::exception& e) {
            diagnostics.push_back({DiagnosticSeverity::Error, e.what(), peek().range});
            synchronize();
        }
    }
    program->range = {start, previousEnd()};
    outDiagnostics = diagnostics;
    return program;
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    SourcePosition start = peek().range.start;
    consume(TokenType::LBRACE);
    auto block = std::make_unique<BlockNode>();
    block->file = file;
    while (!check(TokenType::RBRACE)) {
        if (check(TokenType::END)) fail("expected '}' to close the block");
        block->stmts.push_back(statement());
    }
    consume(TokenType::RBRACE);
    return at(std::move(block), start, previousEnd());
}

std::unique_ptr<Node> Parser::statement() {
    SourcePosition start = peek().range.start;
    switch (peek().type) {
        case TokenType::INCLUDE: {
            ++pos;
            consume(TokenType::LPAREN);
            std::string file = consume(TokenType::STRING_LITERAL).value;
            consume(TokenType::RPAREN);
            consume(TokenType::SEMICOLON);
            imports.push_back({file, false});
            return at(std::make_unique<IncludeNode>(file, currentFile), start, previousEnd());
        }
        case TokenType::USING: {
            ++pos;
            // Any word may name a module, including ones that are keywords elsewhere.
            if (check(TokenType::END) || check(TokenType::SEMICOLON) || peek().value.empty())
                fail("expected a module name after 'using'");
            std::string name = tokens[pos++].value;
            if (match(TokenType::DOT)) name += "." + consume(TokenType::IDENTIFIER).value;
            consume(TokenType::SEMICOLON);
            imports.push_back({name, true});
            return at(std::make_unique<UsingNode>(name, currentFile), start, previousEnd());
        }
        case TokenType::GLOBAL:
            ++pos;
            if (!isType() || check(TokenType::VOID_KW)) fail("expected a variable type after 'global'");
            return declaration(true);
        case TokenType::INT_KW: case TokenType::FLOAT_KW: case TokenType::STRING_KW:
        case TokenType::BOOL_KW: case TokenType::VOID_KW: case TokenType::ARRAY: case TokenType::MAP_KW:
        case TokenType::FUNC_KW:
            return declaration(false);
        case TokenType::STRUCT:
            return structDefinition();
        case TokenType::TRY:
            return tryStatement();
        case TokenType::THROW: {
            ++pos;
            auto message = expression();
            consume(TokenType::SEMICOLON);
            return at(std::make_unique<ThrowNode>(std::move(message)), start, previousEnd());
        }
        case TokenType::IDENTIFIER:
            if (isUserType()) return declaration(false);
            break;
        case TokenType::RETURN: {
            ++pos;
            std::unique_ptr<Node> value;
            if (!check(TokenType::SEMICOLON)) value = expression();
            consume(TokenType::SEMICOLON);
            return at(std::make_unique<ReturnNode>(std::move(value)), start, previousEnd());
        }
        case TokenType::WHILE: {
            ++pos;
            consume(TokenType::LPAREN);
            auto condition = expression();
            consume(TokenType::RPAREN);
            auto body = parseBlock();
            return at(std::make_unique<WhileNode>(std::move(condition), std::move(body)), start, previousEnd());
        }
        case TokenType::FOR:
            return forStatement();
        case TokenType::IF:
            return ifStatement();
        case TokenType::SWITCH:
            return switchStatement();
        case TokenType::BREAK:
            ++pos;
            consume(TokenType::SEMICOLON);
            return at(std::make_unique<BreakNode>(), start, previousEnd());
        case TokenType::CONTINUE:
            ++pos;
            consume(TokenType::SEMICOLON);
            return at(std::make_unique<ContinueNode>(), start, previousEnd());
        case TokenType::LBRACE:
            return parseBlock();
        default:
            break;
    }
    auto node = simpleStatement();
    consume(TokenType::SEMICOLON);
    node->range = {start, previousEnd()};
    return node;
}

// struct Name { type field; type field = default; }
std::unique_ptr<Node> Parser::structDefinition() {
    SourcePosition start = peek().range.start;
    consume(TokenType::STRUCT);
    Token name = consume(TokenType::IDENTIFIER);
    auto node = std::make_unique<StructDefNode>();
    node->type = std::make_shared<StructType>();
    node->type->name = name.value;
    node->nameRange = name.range;
    consume(TokenType::LBRACE);
    while (!check(TokenType::RBRACE)) {
        if (check(TokenType::END)) fail("expected '}' to close the struct");
        if (!(isTypeKeyword() || check(TokenType::IDENTIFIER)) || check(TokenType::VOID_KW)) fail("expected a field type");
        std::string type = tokens[pos++].value;
        Token field = consume(TokenType::IDENTIFIER);
        for (const auto& existing : node->type->fields)
            if (existing.name == field.value) fail("field '" + field.value + "' is declared twice");
        std::shared_ptr<Node> initial;
        if (match(TokenType::ASSIGN)) initial = expression();
        consume(TokenType::SEMICOLON);
        node->type->fields.push_back({type, field.value});
        node->type->defaults.push_back(std::move(initial));
        node->fieldRanges.push_back(field.range);
    }
    consume(TokenType::RBRACE);
    return at(std::move(node), start, previousEnd());
}

// try { ... } catch (string error) { ... } finally { ... }; catch and finally are
// each optional, but one of them must be there.
std::unique_ptr<Node> Parser::tryStatement() {
    SourcePosition start = peek().range.start;
    consume(TokenType::TRY);
    auto node = std::make_unique<TryNode>();
    node->body = parseBlock();
    if (match(TokenType::CATCH)) {
        if (match(TokenType::LPAREN)) {
            if (check(TokenType::STRING_KW)) ++pos;
            Token name = consume(TokenType::IDENTIFIER);
            node->errorName = name.value;
            node->errorRange = name.range;
            consume(TokenType::RPAREN);
        }
        node->handler = parseBlock();
    }
    if (match(TokenType::FINALLY)) node->cleanup = parseBlock();
    if (!node->handler && !node->cleanup) fail("expected 'catch' or 'finally' after the try block");
    return at(std::move(node), start, previousEnd());
}

// Assignment, element assignment, i++ / i--, or an expression such as a call.
std::unique_ptr<Node> Parser::simpleStatement() {
    SourcePosition start = peek().range.start;
    if (check(TokenType::IDENTIFIER) && isAssignment(peek(1).type)) {
        Token name = consume(TokenType::IDENTIFIER);
        Token op = tokens[pos++];
        auto value = expression();
        if (op.type != TokenType::ASSIGN)
            value = std::make_unique<BinOpNode>(op.value, std::make_unique<VarAccessNode>(name.value, name.range), std::move(value));
        return at(std::make_unique<VarAssignNode>(name.value, std::move(value), name.range), start, previousEnd());
    }
    if (check(TokenType::IDENTIFIER) && (peek(1).type == TokenType::LBRACKET || peek(1).type == TokenType::DOT)) {
        // items[i].name = value is a statement; items[i].name alone is an expression.
        size_t mark = pos;
        Token name = consume(TokenType::IDENTIFIER);
        auto target = postfix(at(std::make_unique<VarAccessNode>(name.value, name.range), start, name.range.end), start);
        if (isAssignment(peek().type)) {
            Token op = tokens[pos++];
            auto value = expression();
            std::string combine = op.type == TokenType::ASSIGN ? "=" : op.value.substr(0, 1);
            return at(std::make_unique<SetNode>(std::move(target), std::move(value), combine), start, previousEnd());
        }
        pos = mark;
    }
    if (check(TokenType::IDENTIFIER) && (peek(1).type == TokenType::INC || peek(1).type == TokenType::DEC)) {
        Token name = consume(TokenType::IDENTIFIER);
        int delta = tokens[pos++].type == TokenType::INC ? 1 : -1;
        return at(std::make_unique<PostIncNode>(name.value, delta), start, previousEnd());
    }
    if (check(TokenType::END)) fail("expected a statement");
    return expression();
}

std::unique_ptr<Node> Parser::declaration(bool global) {
    SourcePosition start = pos > 0 && global ? tokens[pos - 1].range.start : peek().range.start;
    if (check(TokenType::ARRAY)) {
        ++pos;
        return arrayDeclaration(global, start);
    }
    std::string type = tokens[pos++].value;
    Token name = consume(TokenType::IDENTIFIER);
    if (check(TokenType::LPAREN)) {
        if (global) fail("expected '=' after the global variable name");
        return functionDefinition(type, name, start);
    }
    if (type == "void") fail("expected '(' after a void function name");
    // A map or a struct may start empty; a scalar needs its value.
    bool container = type == "map" || tokens[pos - 2].type == TokenType::IDENTIFIER;
    std::unique_ptr<Node> value;
    if (!(container && check(TokenType::SEMICOLON))) {
        consume(TokenType::ASSIGN);
        value = expression();
    }
    consume(TokenType::SEMICOLON);
    return at(std::make_unique<VarDeclNode>(type, name.value, std::move(value), name.range, global), start, previousEnd());
}

// array name size;  array name = value;  array name;  array name(params) { ... }
std::unique_ptr<Node> Parser::arrayDeclaration(bool global, SourcePosition start) {
    Token name = consume(TokenType::IDENTIFIER);
    if (!global && looksLikeFunction()) return functionDefinition("array", name, start);
    std::unique_ptr<ArrayDeclNode> node;
    if (match(TokenType::ASSIGN)) {
        node = std::make_unique<ArrayDeclNode>(name.value, nullptr, expression(), name.range);
    } else if (check(TokenType::SEMICOLON)) {
        node = std::make_unique<ArrayDeclNode>(name.value, nullptr, nullptr, name.range);
    } else {
        node = std::make_unique<ArrayDeclNode>(name.value, expression(), nullptr, name.range);
    }
    node->global = global;
    consume(TokenType::SEMICOLON);
    return at(std::move(node), start, previousEnd());
}

std::unique_ptr<Node> Parser::functionDefinition(const std::string& returnType, const Token& name, SourcePosition start) {
    consume(TokenType::LPAREN);
    std::vector<FuncParam> params;
    if (!check(TokenType::RPAREN)) {
        do {
            if (!(isTypeKeyword() || check(TokenType::IDENTIFIER)) || check(TokenType::VOID_KW)) fail("expected a parameter type");
            // A parameter without a type takes any value: void get(string path, handler)
            std::string type;
            if (!(check(TokenType::IDENTIFIER) && (peek(1).type == TokenType::COMMA || peek(1).type == TokenType::RPAREN)))
                type = tokens[pos++].value;
            Token name = consume(TokenType::IDENTIFIER);
            params.push_back({type, name.value, name.range});
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN);
    std::shared_ptr<Node> body = parseBlock();
    return at(std::make_unique<FuncDefNode>(returnType, name.value, std::move(params), std::move(body), name.range),
              start, previousEnd());
}

bool Parser::forInAhead() const {
    size_t at = 0;
    auto isIn = [&](size_t offset) { return peek(offset).type == TokenType::IDENTIFIER && peek(offset).value == "in"; };
    auto variable = [&] {
        bool typed = (isTypeKeyword(at) || peek(at).type == TokenType::IDENTIFIER) &&
                     peek(at + 1).type == TokenType::IDENTIFIER && !isIn(at + 1);
        if (typed) at += 2;
        else if (peek(at).type == TokenType::IDENTIFIER) at += 1;
        else return false;
        return true;
    };
    if (!variable()) return false;
    if (peek(at).type == TokenType::COMMA) {
        ++at;
        if (!variable()) return false;
    }
    return isIn(at);
}

std::unique_ptr<Node> Parser::forInStatement(SourcePosition start) {
    auto node = std::make_unique<ForInNode>();
    do {
        ForInNode::Variable variable;
        if (isTypeKeyword() || (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::IDENTIFIER && peek(1).value != "in")) {
            if (check(TokenType::VOID_KW)) fail("a loop variable cannot be void");
            variable.type = tokens[pos++].value;
        }
        Token name = consume(TokenType::IDENTIFIER);
        variable.name = name.value;
        variable.range = name.range;
        node->variables.push_back(variable);
    } while (match(TokenType::COMMA));
    if (node->variables.size() == 2 && node->variables[0].name == node->variables[1].name)
        fail("the two loop variables need different names");
    ++pos; // in
    node->iterable = expression();
    consume(TokenType::RPAREN);
    node->body = parseBlock();
    return at(std::move(node), start, previousEnd());
}

std::unique_ptr<Node> Parser::forStatement() {
    SourcePosition start = peek().range.start;
    consume(TokenType::FOR);
    consume(TokenType::LPAREN);
    if (forInAhead()) return forInStatement(start);
    std::unique_ptr<Node> init;
    if (isType()) {
        init = declaration(false);
    } else if (!match(TokenType::SEMICOLON)) {
        init = simpleStatement();
        consume(TokenType::SEMICOLON);
    }
    std::unique_ptr<Node> condition;
    if (!check(TokenType::SEMICOLON)) condition = expression();
    consume(TokenType::SEMICOLON);
    std::unique_ptr<Node> step;
    if (!check(TokenType::RPAREN)) step = simpleStatement();
    consume(TokenType::RPAREN);
    auto body = parseBlock();
    return at(std::make_unique<ForNode>(std::move(init), std::move(condition), std::move(step), std::move(body)),
              start, previousEnd());
}

std::unique_ptr<Node> Parser::ifStatement() {
    SourcePosition start = peek().range.start;
    consume(TokenType::IF);
    consume(TokenType::LPAREN);
    auto condition = expression();
    consume(TokenType::RPAREN);
    auto thenBranch = parseBlock();
    std::unique_ptr<Node> elseBranch;
    if (match(TokenType::ELSE)) elseBranch = check(TokenType::IF) ? ifStatement() : parseBlock();
    return at(std::make_unique<IfNode>(std::move(condition), std::move(thenBranch), std::move(elseBranch)), start, previousEnd());
}

std::unique_ptr<Node> Parser::switchStatement() {
    SourcePosition start = peek().range.start;
    consume(TokenType::SWITCH);
    consume(TokenType::LPAREN);
    auto node = std::make_unique<SwitchNode>(expression());
    consume(TokenType::RPAREN);
    consume(TokenType::LBRACE);
    auto caseBody = [&] {
        SourcePosition bodyStart = peek().range.start;
        auto body = std::make_unique<BlockNode>();
        body->file = file;
        while (!check(TokenType::CASE) && !check(TokenType::DEFAULT) && !check(TokenType::RBRACE)) {
            if (check(TokenType::END)) fail("expected '}' to close the switch");
            body->stmts.push_back(statement());
        }
        return at(std::move(body), bodyStart, previousEnd());
    };
    while (!check(TokenType::RBRACE)) {
        if (match(TokenType::CASE)) {
            auto value = expression();
            consume(TokenType::COLON);
            node->cases.emplace_back(std::move(value), caseBody());
        } else if (match(TokenType::DEFAULT)) {
            if (node->defaultCase) fail("a switch may have only one 'default'");
            consume(TokenType::COLON);
            node->defaultCase = caseBody();
            if (!check(TokenType::RBRACE)) fail("expected 'default' to be the last branch of the switch");
        } else {
            fail("expected 'case' or 'default'");
        }
    }
    consume(TokenType::RBRACE);
    return at(std::move(node), start, previousEnd());
}

std::unique_ptr<Node> Parser::expression() { return logicalOr(); }

std::unique_ptr<Node> Parser::logicalOr() {
    SourcePosition start = peek().range.start;
    auto node = logicalAnd();
    while (match(TokenType::OR)) node = at(std::make_unique<BinOpNode>("||", std::move(node), logicalAnd()), start, previousEnd());
    return node;
}

std::unique_ptr<Node> Parser::logicalAnd() {
    SourcePosition start = peek().range.start;
    auto node = comparison();
    while (match(TokenType::AND)) node = at(std::make_unique<BinOpNode>("&&", std::move(node), comparison()), start, previousEnd());
    return node;
}

std::unique_ptr<Node> Parser::comparison() {
    SourcePosition start = peek().range.start;
    auto node = addition();
    while (check(TokenType::EQ) || check(TokenType::NEQ) || check(TokenType::LT) ||
           check(TokenType::GT) || check(TokenType::LTE) || check(TokenType::GTE)) {
        std::string op = tokens[pos++].value;
        node = at(std::make_unique<BinOpNode>(op, std::move(node), addition()), start, previousEnd());
    }
    return node;
}

std::unique_ptr<Node> Parser::addition() {
    SourcePosition start = peek().range.start;
    auto node = multiplication();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = tokens[pos++].value;
        node = at(std::make_unique<BinOpNode>(op, std::move(node), multiplication()), start, previousEnd());
    }
    return node;
}

std::unique_ptr<Node> Parser::multiplication() {
    SourcePosition start = peek().range.start;
    auto node = unary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::MOD)) {
        std::string op = tokens[pos++].value;
        node = at(std::make_unique<BinOpNode>(op, std::move(node), unary()), start, previousEnd());
    }
    return node;
}

std::unique_ptr<Node> Parser::unary() {
    SourcePosition start = peek().range.start;
    if (match(TokenType::MINUS)) {
        // A negative literal is one number, so -2147483648 is a valid int.
        if (check(TokenType::NUMBER)) {
            Token number = tokens[pos++];
            return at(std::make_unique<NumberNode>("-" + number.value), start, previousEnd());
        }
        return at(std::make_unique<UnaryOpNode>("-", unary()), start, previousEnd());
    }
    if (match(TokenType::NOT)) return at(std::make_unique<UnaryOpNode>("!", unary()), start, previousEnd());
    return primary();
}

std::vector<std::unique_ptr<Node>> Parser::arguments(TokenType close) {
    std::vector<std::unique_ptr<Node>> list;
    if (!check(close)) {
        do list.push_back(expression());
        while (match(TokenType::COMMA));
    }
    consume(close);
    return list;
}

std::unique_ptr<Node> Parser::postfix(std::unique_ptr<Node> node, SourcePosition start) {
    for (;;) {
        if (match(TokenType::LBRACKET)) {
            auto index = expression();
            consume(TokenType::RBRACKET);
            node = at(std::make_unique<IndexNode>(std::move(node), std::move(index)), start, previousEnd());
        } else if (match(TokenType::DOT)) {
            Token field = consume(TokenType::IDENTIFIER);
            node = at(std::make_unique<FieldNode>(std::move(node), field.value, field.range), start, previousEnd());
        } else {
            return node;
        }
    }
}

// {"key": value, name: value}: a bare word before ':' is a text key.
std::unique_ptr<Node> Parser::mapLiteral() {
    SourcePosition start = peek().range.start;
    consume(TokenType::LBRACE);
    auto node = std::make_unique<MapLiteralNode>();
    while (!check(TokenType::RBRACE)) {
        std::unique_ptr<Node> key;
        if (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::COLON) {
            Token word = tokens[pos++];
            key = at(std::make_unique<StringNode>(word.value), word.range.start, word.range.end);
        } else {
            key = expression();
        }
        consume(TokenType::COLON);
        node->entries.emplace_back(std::move(key), expression());
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RBRACE);
    return at(std::move(node), start, previousEnd());
}

// At '(': a lambda's parameter list, the parentheses followed by =>.
bool Parser::lambdaAhead() const {
    int depth = 0;
    for (size_t at = 0;; ++at) {
        TokenType type = peek(at).type;
        if (type == TokenType::END) return false;
        if (type == TokenType::LPAREN) ++depth;
        else if (type == TokenType::RPAREN && --depth == 0) return peek(at + 1).type == TokenType::ARROW;
    }
}

// (type a, b) => expression,  (a) => { statements },  a => expression
std::unique_ptr<Node> Parser::lambda(SourcePosition start) {
    auto node = std::make_unique<LambdaNode>();
    auto parameter = [&] {
        std::string type;
        if (isTypeKeyword() || (check(TokenType::IDENTIFIER) && peek(1).type == TokenType::IDENTIFIER)) {
            if (check(TokenType::VOID_KW)) fail("expected a parameter type");
            type = tokens[pos++].value;
        }
        Token name = consume(TokenType::IDENTIFIER);
        for (const auto& existing : node->params)
            if (existing.name == name.value) fail("parameter '" + name.value + "' is named twice");
        node->params.push_back({type, name.value, name.range});
    };
    if (match(TokenType::LPAREN)) {
        if (!check(TokenType::RPAREN)) {
            do parameter();
            while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN);
    } else {
        parameter();
    }
    consume(TokenType::ARROW);
    if (check(TokenType::LBRACE)) {
        node->body = std::shared_ptr<BlockNode>(parseBlock().release());
    } else {
        SourcePosition valueStart = peek().range.start;
        auto value = expression();
        auto body = std::make_shared<BlockNode>();
        body->file = file;
        body->stmts.push_back(at(std::make_unique<ReturnNode>(std::move(value)), valueStart, previousEnd()));
        body->range = {valueStart, previousEnd()};
        node->body = body;
    }
    return at(std::move(node), start, previousEnd());
}

std::unique_ptr<Node> Parser::primary() {
    SourcePosition start = peek().range.start;
    return postfix(atom(), start);
}

std::unique_ptr<Node> Parser::atom() {
    SourcePosition start = peek().range.start;
    const Token& token = peek();
    switch (token.type) {
        case TokenType::NUMBER: {
            Token number = tokens[pos++];
            return at(std::make_unique<NumberNode>(number.value), start, previousEnd());
        }
        case TokenType::STRING_LITERAL:
        case TokenType::STRING_BEGIN: {
            // Adjacent literals join, so long text can span several lines; ${expression}
            // parts make the string an interpolation.
            std::vector<std::unique_ptr<Node>> parts;
            std::string text;
            SourcePosition textStart = peek().range.start;
            auto flush = [&] {
                if (!text.empty()) parts.push_back(at(std::make_unique<StringNode>(text), textStart, previousEnd()));
                text.clear();
            };
            while (check(TokenType::STRING_LITERAL) || check(TokenType::STRING_BEGIN)) {
                if (match(TokenType::STRING_LITERAL)) {
                    text += tokens[pos - 1].value;
                    continue;
                }
                text += tokens[pos++].value;
                for (;;) {
                    flush();
                    if (check(TokenType::STRING_MIDDLE) || check(TokenType::STRING_END)) fail("expected an expression inside ${ }");
                    parts.push_back(expression());
                    textStart = peek().range.start;
                    if (match(TokenType::STRING_END)) {
                        text += tokens[pos - 1].value;
                        break;
                    }
                    if (!check(TokenType::STRING_MIDDLE)) fail("expected '}' to close ${ in the string");
                    text += tokens[pos++].value;
                }
            }
            bool interpolated = false;
            for (const auto& part : parts)
                if (!dynamic_cast<StringNode*>(part.get())) interpolated = true;
            if (!interpolated) {
                std::string whole;
                for (const auto& part : parts) whole += static_cast<StringNode*>(part.get())->val;
                return at(std::make_unique<StringNode>(whole + text), start, previousEnd());
            }
            flush();
            auto node = std::make_unique<InterpolationNode>();
            node->parts = std::move(parts);
            return at(std::move(node), start, previousEnd());
        }
        case TokenType::TRUE_KW:
        case TokenType::FALSE_KW: {
            bool value = tokens[pos++].type == TokenType::TRUE_KW;
            return at(std::make_unique<BoolNode>(value), start, previousEnd());
        }
        case TokenType::LPAREN: {
            if (lambdaAhead()) return lambda(start);
            ++pos;
            auto inner = expression();
            consume(TokenType::RPAREN);
            inner->range = {start, previousEnd()};
            return inner;
        }
        case TokenType::LBRACKET: {
            ++pos;
            auto literal = std::make_unique<ArrayLiteralNode>();
            literal->elements = arguments(TokenType::RBRACKET);
            return at(std::move(literal), start, previousEnd());
        }
        case TokenType::LBRACE:
            return mapLiteral();
        case TokenType::IDENTIFIER: {
            if (peek(1).type == TokenType::ARROW) return lambda(start);
            Token name = tokens[pos++];
            if (match(TokenType::LPAREN)) {
                auto args = arguments(TokenType::RPAREN);
                return at(std::make_unique<FuncCallNode>(name.value, std::move(args), name.range), start, previousEnd());
            }
            if (check(TokenType::INC) || check(TokenType::DEC)) {
                int delta = tokens[pos++].type == TokenType::INC ? 1 : -1;
                return at(std::make_unique<PostIncNode>(name.value, delta), start, previousEnd());
            }
            return at(std::make_unique<VarAccessNode>(name.value, name.range), start, previousEnd());
        }
        default:
            fail("expected an expression");
    }
}

} // namespace foxlang
