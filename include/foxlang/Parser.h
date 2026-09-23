#pragma once
#include <vector>
#include <memory>
#include <string>
#include "foxlang/Token.h"
#include "foxlang/AST.h"
#include "foxlang/SourceLocation.h"

namespace foxlang {

class Parser {
public:
    explicit Parser(std::vector<Token> t, std::string currentFile = "");

    // Pure AST parsing
    std::unique_ptr<BlockNode> parseProgram();
    std::unique_ptr<BlockNode> parseProgramWithDiagnostics(std::vector<Diagnostic>& outDiagnostics);
    std::unique_ptr<Node> statement();
    std::unique_ptr<BlockNode> parseBlock();

    std::unique_ptr<Node> expression();
    std::unique_ptr<Node> logicalOr();
    std::unique_ptr<Node> logicalAnd();
    std::unique_ptr<Node> comparison();
    std::unique_ptr<Node> addition();
    std::unique_ptr<Node> multiplication();
    std::unique_ptr<Node> primary();

    Token consume(TokenType type);
    const Token& peek(size_t offset = 0) const;
    bool match(TokenType type);

    void setCollectDiagnostics(bool enable) { collectDiagnostics = enable; }
    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }

    // Context & compatibility properties
    Context globalContext;
    std::string currentFile;
    bool importMode = false;
    void run();

private:
    std::vector<Token> tokens;
    size_t pos = 0;
    bool collectDiagnostics = false;
    std::vector<Diagnostic> diagnostics;

    void synchronize();
};

} // namespace foxlang

using foxlang::Parser;
