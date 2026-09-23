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

    void advanceChar();
    SourcePosition currentPosition() const;
};

} // namespace foxlang

using foxlang::Lexer;
