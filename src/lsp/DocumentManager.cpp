#include "DocumentManager.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/Runtime.h"

namespace foxlang {
namespace lsp {

DocumentManager::DocumentManager(std::string home) : foxHome(std::move(home)) {}

std::string DocumentManager::uriToFilePath(const std::string& uri) const {
    if (uri.compare(0, 7, "file://") == 0) {
        std::string path = uri.substr(7);
#ifdef _WIN32
        if (path.size() >= 3 && path[0] == '/' && path[2] == ':') {
            path = path.substr(1); // Strip leading slash before drive letter
        }
#endif
        return path;
    }
    return uri;
}

void DocumentManager::openDocument(const std::string& uri, const std::string& text, int version) {
    DocumentState doc;
    doc.uri = uri;
    doc.text = text;
    doc.version = version;
    analyze(doc);
    documents[uri] = std::move(doc);
}

void DocumentManager::updateDocument(const std::string& uri, const std::string& text, int version) {
    auto it = documents.find(uri);
    if (it != documents.end()) {
        it->second.text = text;
        it->second.version = version;
        analyze(it->second);
    } else {
        openDocument(uri, text, version);
    }
}

void DocumentManager::closeDocument(const std::string& uri) {
    documents.erase(uri);
}

const DocumentState* DocumentManager::getDocument(const std::string& uri) const {
    auto it = documents.find(uri);
    return it != documents.end() ? &it->second : nullptr;
}

std::vector<LspDiagnostic> DocumentManager::getDiagnostics(const std::string& uri) const {
    const DocumentState* doc = getDocument(uri);
    return doc ? doc->diagnostics : std::vector<LspDiagnostic>{};
}

void DocumentManager::analyze(DocumentState& doc) {
    std::string filePath = uriToFilePath(doc.uri);

    // 1. Lexical analysis with diagnostic collection
    Lexer lexer(doc.text, true);
    auto tokens = lexer.tokenize();
    std::vector<Diagnostic> allDiags = lexer.getDiagnostics();

    // 2. Syntax analysis with error recovery
    Parser parser(std::move(tokens), filePath);
    std::vector<Diagnostic> parserDiags;
    auto program = parser.parseProgramWithDiagnostics(parserDiags);
    allDiags.insert(allDiags.end(), parserDiags.begin(), parserDiags.end());

    // 3. Semantic analysis (without executing code)
    doc.analyzer = std::make_unique<SemanticAnalyzer>(filePath, foxHome);
    doc.analyzer->analyze(program.get());
    const auto& semDiags = doc.analyzer->getDiagnostics();
    allDiags.insert(allDiags.end(), semDiags.begin(), semDiags.end());

    // 4. Convert all diagnostics to LSP format
    doc.diagnostics.clear();
    for (const auto& d : allDiags) {
        doc.diagnostics.push_back(LspDiagnostic::fromDiagnostic(d));
    }
}

} // namespace lsp
} // namespace foxlang
