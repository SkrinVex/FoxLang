#pragma once
#include <vector>
#include <memory>
#include <string>
#include "foxlang/Token.h"
#include "foxlang/AST.h"
#include "foxlang/SourceLocation.h"
#include "foxlang/SourceProvider.h"

namespace foxlang {

class Parser {
public:
    explicit Parser(std::vector<Token> t, std::string currentFile = "");

    std::unique_ptr<BlockNode> parseProgram();
    // Collects syntax errors instead of stopping at the first one, for editors.
    std::unique_ptr<BlockNode> parseProgramWithDiagnostics(std::vector<Diagnostic>& outDiagnostics);

    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }
    const std::vector<ModuleImport>& getImports() const { return imports; }

    std::string currentFile;

private:
    std::vector<Token> tokens;
    size_t pos = 0;
    std::vector<Diagnostic> diagnostics;
    std::vector<ModuleImport> imports;

    std::unique_ptr<Node> statement();
    std::unique_ptr<BlockNode> parseBlock();
    std::unique_ptr<Node> simpleStatement();
    std::unique_ptr<Node> declaration(bool global);
    std::unique_ptr<Node> arrayDeclaration(bool global, SourcePosition start);
    std::unique_ptr<Node> functionDefinition(const std::string& returnType, const Token& name, SourcePosition start);
    std::unique_ptr<Node> forStatement();
    std::unique_ptr<Node> switchStatement();
    std::unique_ptr<Node> ifStatement();

    std::unique_ptr<Node> expression();
    std::unique_ptr<Node> logicalOr();
    std::unique_ptr<Node> logicalAnd();
    std::unique_ptr<Node> comparison();
    std::unique_ptr<Node> addition();
    std::unique_ptr<Node> multiplication();
    std::unique_ptr<Node> unary();
    std::unique_ptr<Node> primary();
    std::vector<std::unique_ptr<Node>> arguments(TokenType close);

    Token consume(TokenType type);
    const Token& peek(size_t offset = 0) const;
    bool check(TokenType type) const { return peek().type == type; }
    bool match(TokenType type);
    bool isTypeKeyword(size_t offset = 0) const;
    bool looksLikeFunction() const;
    SourcePosition previousEnd() const;
    [[noreturn]] void fail(const std::string& message) const;
    void synchronize();
};

} // namespace foxlang
