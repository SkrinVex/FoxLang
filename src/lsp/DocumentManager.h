#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "foxlang/SemanticAnalyzer.h"
#include "foxlang/Project.h"
#include "Protocol.h"

namespace foxlang {
namespace lsp {

struct DocumentState {
    std::string uri;
    std::string text;
    int version = 0;
    std::unique_ptr<SemanticAnalyzer> analyzer;
    std::vector<LspDiagnostic> diagnostics;
    std::vector<std::string> peers; // canonical paths of the other files of its program
};

// Open documents and their analysis. A document is analyzed together with the
// other files of its program, so a function defined in one file of a project is
// known in the others; the open buffers take the place of the files on disk.
class DocumentManager {
public:
    explicit DocumentManager(std::string foxHome = "");

    void setFoxHome(std::string home) { foxHome = std::move(home); }
    void setWorkspaceRoot(const std::string& uri);

    // Each returns the documents whose diagnostics may have changed.
    std::vector<std::string> openDocument(const std::string& uri, const std::string& text, int version);
    std::vector<std::string> updateDocument(const std::string& uri, const std::string& text, int version);
    std::vector<std::string> closeDocument(const std::string& uri);
    // After a file changed on disk (saved): analyze every open document again.
    std::vector<std::string> refresh();

    const DocumentState* getDocument(const std::string& uri) const;
    std::vector<LspDiagnostic> getDiagnostics(const std::string& uri) const;

    // file:// URIs are percent-encoded: spaces and Cyrillic in a path arrive as %XX.
    static std::string uriToFilePath(const std::string& uri);
    static std::string filePathToUri(const std::string& path);

private:
    std::string foxHome;
    std::unordered_map<std::string, DocumentState> documents;
    std::shared_ptr<OverlaySources> sources;
    std::unique_ptr<ProjectIndex> project;

    void analyze(DocumentState& doc);
    std::vector<std::string> reanalyzePeers(const std::string& uri, std::vector<std::string> peers);
};

} // namespace lsp
} // namespace foxlang
