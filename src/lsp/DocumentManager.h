#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "foxlang/SemanticAnalyzer.h"
#include "Protocol.h"

namespace foxlang {
namespace lsp {

struct DocumentState {
    std::string uri;
    std::string text;
    int version = 0;
    std::unique_ptr<SemanticAnalyzer> analyzer;
    std::vector<LspDiagnostic> diagnostics;
};

class DocumentManager {
public:
    explicit DocumentManager(std::string foxHome = "");

    void setFoxHome(std::string home) { foxHome = std::move(home); }

    void openDocument(const std::string& uri, const std::string& text, int version);
    void updateDocument(const std::string& uri, const std::string& text, int version);
    void closeDocument(const std::string& uri);

    const DocumentState* getDocument(const std::string& uri) const;
    std::vector<LspDiagnostic> getDiagnostics(const std::string& uri) const;

private:
    std::string foxHome;
    std::unordered_map<std::string, DocumentState> documents;

    void analyze(DocumentState& doc);
    std::string uriToFilePath(const std::string& uri) const;
};

} // namespace lsp
} // namespace foxlang
