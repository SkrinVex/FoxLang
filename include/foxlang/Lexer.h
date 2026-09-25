#pragma once
#include <string>
#include <vector>
#include "foxlang/Token.h"
#include "foxlang/SourceLocation.h"

namespace foxlang {

class Lexer {
public:
    explicit Lexer(std::string src, bool collectDiagnostics = false);
    std::vector<Token> tokenize();

    void setCollectDiagnostics(bool enable) { collectDiagnostics = enable; }
    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }

private:
    std::string source;
    size_t pos = 0;
    int line = 1;
    int column = 1;
    bool collectDiagnostics = false;
    std::vector<Diagnostic> diagnostics;
    // One entry per ${ still open: how many { of the expression inside are open.
    std::vector<int> interpolations;

    void advanceChar();
    // The text of a string up to its closing quote or the next ${; `resumed` when it
    // continues after the } that closed an expression inside it.
    void scanString(std::vector<Token>& tokens, SourcePosition start, bool resumed);
    SourcePosition currentPosition() const;
    std::string unicodeEscape(SourcePosition literalStart);
};

} // namespace foxlang

using foxlang::Lexer;
