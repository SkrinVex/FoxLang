#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "foxlang/AST.h"
#include "foxlang/SourceLocation.h"

namespace foxlang {

enum class SymbolKind {
    Variable,
    Function,
    Parameter,
    Builtin,
    Keyword,
    Module
};

struct Symbol {
    std::string name;
    std::string type;              // e.g. "int", "string", "void"
    SymbolKind kind = SymbolKind::Variable;
    SourceRange declRange;         // Location where symbol was declared
    std::vector<FuncParam> params; // If function
    std::string returnType;        // If function
    std::string documentation;     // Description for hover/completion
    std::string fileUri;           // URI or path of declaration
};

struct Scope {
    Scope* parent = nullptr;
    std::unordered_map<std::string, Symbol> symbols;

    Symbol* find(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) return &it->second;
        if (parent) return parent->find(name);
        return nullptr;
    }

    Symbol* findCurrent(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) return &it->second;
        return nullptr;
    }

    void getAllSymbols(std::vector<Symbol>& out) const {
        for (const auto& pair : symbols) {
            out.push_back(pair.second);
        }
        if (parent) parent->getAllSymbols(out);
    }
};

struct CompletionItem {
    std::string label;
    std::string kind;              // "Keyword", "Function", "Variable", "Type"
    std::string detail;            // e.g. "int counter" or "void print(...)"
    std::string documentation;
};

struct HoverInfo {
    std::string markdown;
    SourceRange range;
    bool found = false;
};

struct DefinitionInfo {
    SourceRange range;
    std::string fileUri;
    bool found = false;
};

struct DocumentSymbolInfo {
    std::string name;
    std::string kind;              // "Function", "Variable", etc.
    SourceRange range;
    SourceRange selectionRange;
    std::vector<DocumentSymbolInfo> children;
};

struct ParameterInfo {
    std::string label;
    std::string documentation;
};

struct SignatureInfo {
    std::string label;
    std::string documentation;
    std::vector<ParameterInfo> parameters;
};

struct SignatureHelpResult {
    std::vector<SignatureInfo> signatures;
    int activeSignature = 0;
    int activeParameter = 0;
    bool found = false;
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(std::string currentFile = "", std::string foxHome = "");
    ~SemanticAnalyzer();

    // Analyze the AST without executing user code
    void analyze(const BlockNode* root);

    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }

    // LSP query methods
    HoverInfo getHover(int line, int col) const;
    DefinitionInfo getDefinition(int line, int col) const;
    std::vector<CompletionItem> getCompletions(int line, int col) const;
    std::vector<DocumentSymbolInfo> getDocumentSymbols() const;
    SignatureHelpResult getSignatureHelp(const std::string& code, int line, int col) const;
    const Symbol* findFunction(const std::string& name) const;

private:
    std::string currentFile;
    std::string foxHome;
    std::vector<Diagnostic> diagnostics;

    std::unique_ptr<Scope> rootScope;
    Scope* currentScope = nullptr;

    struct SymbolRef {
        SourceRange range;
        Symbol symbol;
    };
    std::vector<SymbolRef> symbolRefs;
    std::vector<DocumentSymbolInfo> documentSymbols;

    std::string currentFuncReturnType;

    void visitNode(const Node* node);
    void visitBlock(const BlockNode* node);
    void visitFuncDef(const FuncDefNode* node);
    void visitVarDecl(const VarDeclNode* node);
    void visitGlobalVarDecl(const GlobalVarDeclNode* node);
    void visitVarAssign(const VarAssignNode* node);
    void visitFuncCall(const FuncCallNode* node);
    void visitVarAccess(const VarAccessNode* node);
    void visitReturn(const ReturnNode* node);
    void visitIf(const IfNode* node);
    void visitWhile(const WhileNode* node);
    void visitFor(const ForNode* node);
    void visitSwitch(const SwitchNode* node);
    void visitBinOp(const BinOpNode* node);
    void visitArrayDecl(const ArrayDeclNode* node);
    void visitUsing(const UsingNode* node);
    void visitInclude(const IncludeNode* node);

    std::string inferExprType(const Node* node);
    void enterScope();
    void exitScope();
    void addBuiltins();
    void loadModuleSymbols(const std::string& moduleName, SourceRange importRange);
};

} // namespace foxlang
