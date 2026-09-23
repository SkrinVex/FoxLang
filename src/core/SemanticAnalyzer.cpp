#include "foxlang/SemanticAnalyzer.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/Runtime.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace foxlang {

SemanticAnalyzer::SemanticAnalyzer(std::string curFile, std::string home)
    : currentFile(std::move(curFile)), foxHome(std::move(home)) {
    rootScope = std::make_unique<Scope>();
    currentScope = rootScope.get();
    addBuiltins();
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

void SemanticAnalyzer::enterScope() {
    auto newScope = std::make_unique<Scope>();
    newScope->parent = currentScope;
    currentScope = newScope.release();
}

void SemanticAnalyzer::exitScope() {
    if (currentScope && currentScope->parent) {
        Scope* parent = currentScope->parent;
        delete currentScope;
        currentScope = parent;
    }
}

void SemanticAnalyzer::addBuiltins() {
    auto addFn = [this](std::string name, std::string ret, std::vector<FuncParam> params, std::string doc) {
        Symbol sym;
        sym.name = name;
        sym.type = ret;
        sym.kind = SymbolKind::Builtin;
        sym.returnType = ret;
        sym.params = std::move(params);
        sym.documentation = std::move(doc);
        rootScope->symbols[name] = sym;
    };

    addFn("print", "void", {}, "print(...args): Print values to standard output");
    addFn("input", "string", {}, "input(): Read a line from standard input");
    addFn("fox", "void", {}, "fox(): Print FoxLang ASCII banner");
    addFn("round", "int", {{"float", "value"}}, "round(float val): Round float to nearest integer");
    addFn("random", "int", {{"int", "min"}, {"int", "max"}}, "random(int min, int max): Generate random integer between min and max");
    addFn("readfile", "string", {{"string", "path"}}, "readfile(string path): Read entire file contents as string");
    addFn("read_file", "string", {{"string", "path"}}, "read_file(string path): Read file contents as string");
    addFn("json_get", "string", {{"string", "json"}, {"string", "key"}}, "json_get(string json, string key): Extract value from JSON by key");
    addFn("str_contains", "bool", {{"string", "str"}, {"string", "sub"}}, "str_contains(string str, string sub): Check if substring exists");
    addFn("str_to_int", "int", {{"string", "str"}}, "str_to_int(string str): Parse integer from string");
    addFn("getch", "string", {}, "getch(): Read single character without echo");
    addFn("kbhit", "bool", {}, "kbhit(): Check if a key was pressed");

    addFn("httpget", "string", {{"string", "url"}}, "httpget(string url): Send HTTP GET request");
    addFn("httppost", "string", {{"string", "url"}, {"string", "data"}}, "httppost(string url, string data, string type = \"application/json\"): Send HTTP POST request");
    addFn("httpput", "string", {{"string", "url"}, {"string", "data"}}, "httpput(string url, string data, string type = \"application/json\"): Send HTTP PUT request");
    addFn("httpdelete", "string", {{"string", "url"}}, "httpdelete(string url): Send HTTP DELETE request");

    addFn("server_start", "string", {{"int", "port"}}, "server_start(int port): Start HTTP server on specified port");
    addFn("server_stop", "string", {}, "server_stop(): Gracefully stop running HTTP server");
    addFn("route_get", "string", {{"string", "path"}, {"string", "handler"}}, "route_get(string path, string handler): Register HTTP GET route");
    addFn("route_post", "string", {{"string", "path"}, {"string", "handler"}}, "route_post(string path, string handler): Register HTTP POST route");
    addFn("send_response", "void", {{"string", "data"}}, "send_response(string data): Send HTTP response body");

    addFn("array", "void", {{"identifier", "name"}, {"int", "size"}}, "array <name> <size>: Declare fixed-size array");
    addFn("set", "void", {{"array", "arr"}, {"int", "idx"}, {"any", "val"}}, "set(arr, idx, val): Set array element");
    addFn("get", "any", {{"array", "arr"}, {"int", "idx"}}, "get(arr, idx): Get array element or server GET route");
    addFn("size", "int", {{"array", "arr"}}, "size(arr): Get number of elements in array");
}

void SemanticAnalyzer::loadModuleSymbols(const std::string& moduleName, SourceRange importRange) {
    auto addFn = [this](std::string name, std::string ret, std::vector<FuncParam> params, std::string doc) {
        Symbol sym;
        sym.name = name;
        sym.type = ret;
        sym.kind = SymbolKind::Function;
        sym.returnType = ret;
        sym.params = std::move(params);
        sym.documentation = std::move(doc);
        rootScope->symbols[name] = sym;
    };

    if (moduleName == "server") {
        addFn("listen", "void", {{"int", "port"}}, "listen(int port): Start listening on port");
        addFn("respond", "void", {{"string", "data"}}, "respond(string data): Send HTTP response 200");
        addFn("respond_status", "void", {{"int", "status"}, {"string", "data"}}, "respond_status(int status, string data): Send response with status code");
        addFn("body", "string", {}, "body(): Get request body");
        addFn("method", "string", {}, "method(): Get request method (GET, POST)");
        addFn("path", "string", {}, "path(): Get request URL path");
        addFn("get", "void", {{"string", "path"}, {"string", "handler"}}, "get(string path, string handler): Register GET handler");
        addFn("post", "void", {{"string", "path"}, {"string", "handler"}}, "post(string path, string handler): Register POST handler");
    } else if (moduleName == "http") {
        addFn("http_fetch", "string", {{"string", "url"}}, "http_fetch(string url): Perform HTTP GET request");
        addFn("http_post_json", "string", {{"string", "url"}, {"string", "body"}}, "http_post_json(string url, string body): Send JSON POST request");
        addFn("http_post_as", "string", {{"string", "url"}, {"string", "body"}, {"string", "content_type"}}, "http_post_as(string url, string body, string type): Send POST with content type");
        addFn("http_put_json", "string", {{"string", "url"}, {"string", "body"}}, "http_put_json(string url, string body): Send JSON PUT request");
        addFn("http_remove", "string", {{"string", "url"}}, "http_remove(string url): Send HTTP DELETE request");
    } else if (moduleName == "env") {
        addFn("env", "string", {{"string", "key"}}, "env(string key, string fallback = \"\"): Get environment variable");
        addFn("secret", "string", {{"string", "key"}}, "secret(string key): Get required secret from env or error");
    } else if (moduleName == "log") {
        addFn("debug", "void", {{"string", "message"}}, "debug(string msg): Log message at DEBUG level");
        addFn("info", "void", {{"string", "message"}}, "info(string msg): Log message at INFO level");
        addFn("warn", "void", {{"string", "message"}}, "warn(string msg): Log message at WARN level");
        addFn("error", "void", {{"string", "message"}}, "error(string msg): Log message at ERROR level");
    } else if (moduleName == "json") {
        addFn("json_path", "string", {{"string", "json"}, {"string", "path"}}, "json_path(string json, string path): Extract value at nested path");
        addFn("json_safe", "string", {{"string", "str"}}, "json_safe(string str): Escape string for JSON embedding");
    } else if (moduleName == "math") {
        addFn("abs", "int", {{"int", "x"}}, "abs(int x): Absolute value");
        addFn("min", "int", {{"int", "a"}, {"int", "b"}}, "min(int a, int b): Minimum of two integers");
        addFn("max", "int", {{"int", "a"}, {"int", "b"}}, "max(int a, int b): Maximum of two integers");
        addFn("pow", "int", {{"int", "base"}, {"int", "exp"}}, "pow(int base, int exp): Power function");
        addFn("sqrt", "int", {{"int", "x"}}, "sqrt(int x): Integer square root");
    } else if (moduleName == "string") {
        addFn("str_len", "int", {{"string", "s"}}, "str_len(string s): Length of string");
        addFn("str_sub", "string", {{"string", "s"}, {"int", "start"}, {"int", "len"}}, "str_sub(string s, int start, int len): Substring");
        addFn("str_find", "int", {{"string", "s"}, {"string", "sub"}}, "str_find(string s, string sub): Find substring index");
        addFn("str_replace", "string", {{"string", "s"}, {"string", "from"}, {"string", "to"}}, "str_replace(string s, string from, string to): Replace substring");
        addFn("str_upper", "string", {{"string", "s"}}, "str_upper(string s): Convert to uppercase");
        addFn("str_lower", "string", {{"string", "s"}}, "str_lower(string s): Convert to lowercase");
        addFn("str_trim", "string", {{"string", "s"}}, "str_trim(string s): Trim whitespace");
        addFn("str_split", "string", {{"string", "s"}, {"string", "delim"}}, "str_split(string s, string delim): Split string into array");
    } else if (moduleName == "time") {
        addFn("time_ms", "string", {}, "time_ms(): Current UNIX time in milliseconds");
        addFn("time_str", "string", {}, "time_str(): Current date/time formatted string");
        addFn("sleep_ms", "void", {{"int", "ms"}}, "sleep_ms(int ms): Sleep for given milliseconds");
    } else if (moduleName == "net") {
        addFn("resolve_host", "string", {{"string", "host"}}, "resolve_host(string host): Resolve hostname to IP");
        addFn("connect_tcp", "int", {{"string", "host"}, {"int", "port"}}, "connect_tcp(string host, int port): Connect TCP socket");
        addFn("send_tcp", "int", {{"int", "sock"}, {"string", "data"}}, "send_tcp(int sock, string data): Send TCP data");
        addFn("recv_tcp", "string", {{"int", "sock"}, {"int", "len"}}, "recv_tcp(int sock, int len): Receive TCP data");
        addFn("close_tcp", "void", {{"int", "sock"}}, "close_tcp(int sock): Close TCP socket");
    } else if (moduleName == "terminal") {
        addFn("term_clear", "void", {}, "term_clear(): Clear terminal screen");
        addFn("term_home", "void", {}, "term_home(): Move cursor to home (top-left)");
        addFn("term_write", "void", {{"string", "s"}}, "term_write(string s): Write raw string to terminal");
        addFn("term_goto", "void", {{"int", "row"}, {"int", "col"}}, "term_goto(int row, int col): Move cursor to row/col");
        addFn("term_color", "void", {{"string", "c"}}, "term_color(string code): Set ANSI color");
        addFn("term_reset", "void", {}, "term_reset(): Reset ANSI attributes");
    } else {
        // Attempt to resolve file on disk
        std::string modPath = moduleName;
        if (modPath.size() < 4 || modPath.substr(modPath.size() - 4) != ".fox") {
            modPath += ".fox";
        }
        std::string fullPath;
        try {
            fullPath = runtime::resolveFoxFile("std/" + modPath, currentFile, foxHome);
        } catch (...) {
            try {
                fullPath = runtime::resolveFoxFile(modPath, currentFile, foxHome);
            } catch (...) {
                diagnostics.push_back({DiagnosticSeverity::Warning, "Module '" + moduleName + "' not found", importRange});
                return;
            }
        }

        std::ifstream file(fullPath);
        if (file.is_open()) {
            std::stringstream buf;
            buf << file.rdbuf();
            try {
                Lexer modLexer(buf.str(), true);
                auto modTokens = modLexer.tokenize();
                Parser modParser(std::move(modTokens), fullPath);
                std::vector<Diagnostic> modDiags;
                auto modProg = modParser.parseProgramWithDiagnostics(modDiags);
                for (const auto& stmt : modProg->stmts) {
                    if (auto* fn = dynamic_cast<const FuncDefNode*>(stmt.get())) {
                        Symbol s;
                        s.name = fn->name;
                        s.type = fn->returnType;
                        s.kind = SymbolKind::Function;
                        s.returnType = fn->returnType;
                        s.params = fn->params;
                        s.declRange = fn->range;
                        s.fileUri = fullPath;
                        rootScope->symbols[fn->name] = s;
                    }
                }
            } catch (...) {}
        }
    }
}

void SemanticAnalyzer::analyze(const BlockNode* root) {
    diagnostics.clear();
    symbolRefs.clear();
    documentSymbols.clear();

    if (!root) return;
    visitBlock(root);
}

void SemanticAnalyzer::visitNode(const Node* node) {
    if (!node) return;

    if (auto* blk = dynamic_cast<const BlockNode*>(node)) {
        visitBlock(blk);
    } else if (auto* fn = dynamic_cast<const FuncDefNode*>(node)) {
        visitFuncDef(fn);
    } else if (auto* vd = dynamic_cast<const VarDeclNode*>(node)) {
        visitVarDecl(vd);
    } else if (auto* gvd = dynamic_cast<const GlobalVarDeclNode*>(node)) {
        visitGlobalVarDecl(gvd);
    } else if (auto* va = dynamic_cast<const VarAssignNode*>(node)) {
        visitVarAssign(va);
    } else if (auto* fc = dynamic_cast<const FuncCallNode*>(node)) {
        visitFuncCall(fc);
    } else if (auto* acc = dynamic_cast<const VarAccessNode*>(node)) {
        visitVarAccess(acc);
    } else if (auto* ret = dynamic_cast<const ReturnNode*>(node)) {
        visitReturn(ret);
    } else if (auto* ifn = dynamic_cast<const IfNode*>(node)) {
        visitIf(ifn);
    } else if (auto* wh = dynamic_cast<const WhileNode*>(node)) {
        visitWhile(wh);
    } else if (auto* fr = dynamic_cast<const ForNode*>(node)) {
        visitFor(fr);
    } else if (auto* sw = dynamic_cast<const SwitchNode*>(node)) {
        visitSwitch(sw);
    } else if (auto* bop = dynamic_cast<const BinOpNode*>(node)) {
        visitBinOp(bop);
    } else if (auto* arr = dynamic_cast<const ArrayDeclNode*>(node)) {
        visitArrayDecl(arr);
    } else if (auto* usg = dynamic_cast<const UsingNode*>(node)) {
        visitUsing(usg);
    } else if (auto* inc = dynamic_cast<const IncludeNode*>(node)) {
        visitInclude(inc);
    }
}

void SemanticAnalyzer::visitBlock(const BlockNode* node) {
    for (const auto& stmt : node->stmts) {
        visitNode(stmt.get());
    }
}

void SemanticAnalyzer::visitFuncDef(const FuncDefNode* node) {
    Symbol fnSym;
    fnSym.name = node->name;
    fnSym.type = node->returnType;
    fnSym.kind = SymbolKind::Function;
    fnSym.returnType = node->returnType;
    fnSym.params = node->params;
    fnSym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    fnSym.fileUri = currentFile;

    std::string sig = node->returnType + " " + node->name + "(";
    for (size_t i = 0; i < node->params.size(); i++) {
        if (i > 0) sig += ", ";
        sig += node->params[i].type + " " + node->params[i].name;
    }
    sig += ")";
    fnSym.documentation = sig;

    rootScope->symbols[node->name] = fnSym;
    symbolRefs.push_back({fnSym.declRange, fnSym});

    DocumentSymbolInfo docSym;
    docSym.name = node->name;
    docSym.kind = "Function";
    docSym.range = node->range;
    docSym.selectionRange = fnSym.declRange;
    documentSymbols.push_back(docSym);

    enterScope();
    std::string oldReturn = currentFuncReturnType;
    currentFuncReturnType = node->returnType;

    for (const auto& param : node->params) {
        Symbol paramSym;
        paramSym.name = param.name;
        paramSym.type = param.type;
        paramSym.kind = SymbolKind::Parameter;
        paramSym.documentation = "parameter " + param.type + " " + param.name;
        paramSym.fileUri = currentFile;
        paramSym.declRange = node->range; // Encompassed in function signature
        currentScope->symbols[param.name] = paramSym;
    }

    if (node->body) {
        visitNode(node->body.get());
    }

    currentFuncReturnType = oldReturn;
    exitScope();
}

void SemanticAnalyzer::visitVarDecl(const VarDeclNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    if (currentScope->findCurrent(node->name)) {
        diagnostics.push_back({DiagnosticSeverity::Warning,
            "Redeclaration of variable '" + node->name + "' in the same scope",
            node->nameRange.start.line > 0 ? node->nameRange : node->range});
    }

    Symbol sym;
    sym.name = node->name;
    sym.type = node->type;
    sym.kind = (currentScope == rootScope.get()) ? SymbolKind::Variable : SymbolKind::Variable;
    sym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    sym.documentation = node->type + " " + node->name;
    sym.fileUri = currentFile;

    currentScope->symbols[node->name] = sym;
    symbolRefs.push_back({sym.declRange, sym});

    if (currentScope == rootScope.get()) {
        DocumentSymbolInfo docSym;
        docSym.name = node->name;
        docSym.kind = "Variable";
        docSym.range = node->range;
        docSym.selectionRange = sym.declRange;
        documentSymbols.push_back(docSym);
    }
}

void SemanticAnalyzer::visitGlobalVarDecl(const GlobalVarDeclNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    Symbol sym;
    sym.name = node->name;
    sym.type = node->type;
    sym.kind = SymbolKind::Variable;
    sym.declRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    sym.documentation = "global " + node->type + " " + node->name;
    sym.fileUri = currentFile;

    rootScope->symbols[node->name] = sym;
    symbolRefs.push_back({sym.declRange, sym});

    DocumentSymbolInfo docSym;
    docSym.name = node->name;
    docSym.kind = "Variable";
    docSym.range = node->range;
    docSym.selectionRange = sym.declRange;
    documentSymbols.push_back(docSym);
}

void SemanticAnalyzer::visitVarAssign(const VarAssignNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined variable '" + node->name + "'", targetRange});
    } else {
        symbolRefs.push_back({targetRange, *sym});
    }
}

void SemanticAnalyzer::visitVarAccess(const VarAccessNode* node) {
    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;
    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined variable '" + node->name + "'", targetRange});
    } else {
        symbolRefs.push_back({targetRange, *sym});
    }
}

void SemanticAnalyzer::visitFuncCall(const FuncCallNode* node) {
    for (const auto& arg : node->args) {
        visitNode(arg.get());
    }

    Symbol* sym = currentScope->find(node->name);
    SourceRange targetRange = node->nameRange.start.line > 0 ? node->nameRange : node->range;

    if (!sym) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Undefined function '" + node->name + "'", targetRange});
        return;
    }

    symbolRefs.push_back({targetRange, *sym});

    // Argument count check for non-varargs
    if (sym->kind == SymbolKind::Function && !sym->params.empty()) {
        if (node->args.size() != sym->params.size()) {
            diagnostics.push_back({DiagnosticSeverity::Error,
                "Function '" + node->name + "' expects " + std::to_string(sym->params.size()) +
                " arguments, but got " + std::to_string(node->args.size()),
                node->range});
        }
    }
}

void SemanticAnalyzer::visitReturn(const ReturnNode* node) {
    if (node->expr) {
        visitNode(node->expr.get());
    }

    if (currentFuncReturnType == "void" && node->expr != nullptr) {
        diagnostics.push_back({DiagnosticSeverity::Error, "Void function should not return a value", node->range});
    }
}

void SemanticAnalyzer::visitIf(const IfNode* node) {
    if (node->condition) visitNode(node->condition.get());
    if (node->thenB) {
        enterScope();
        visitNode(node->thenB.get());
        exitScope();
    }
    if (node->elseB) {
        enterScope();
        visitNode(node->elseB.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitWhile(const WhileNode* node) {
    if (node->condition) visitNode(node->condition.get());
    if (node->body) {
        enterScope();
        visitNode(node->body.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitFor(const ForNode* node) {
    enterScope();
    if (node->init) visitNode(node->init.get());
    if (node->condition) visitNode(node->condition.get());
    if (node->step) visitNode(node->step.get());
    if (node->body) visitNode(node->body.get());
    exitScope();
}

void SemanticAnalyzer::visitSwitch(const SwitchNode* node) {
    if (node->expr) visitNode(node->expr.get());
    for (const auto& c : node->cases) {
        enterScope();
        if (c.first) visitNode(c.first.get());
        if (c.second) visitNode(c.second.get());
        exitScope();
    }
    if (node->defaultCase) {
        enterScope();
        visitNode(node->defaultCase.get());
        exitScope();
    }
}

void SemanticAnalyzer::visitBinOp(const BinOpNode* node) {
    if (node->left) visitNode(node->left.get());
    if (node->right) visitNode(node->right.get());
}

void SemanticAnalyzer::visitArrayDecl(const ArrayDeclNode* node) {
    if (node->sizeNode) visitNode(node->sizeNode.get());
    Symbol sym;
    sym.name = node->name;
    sym.type = "array";
    sym.kind = SymbolKind::Variable;
    sym.declRange = node->range;
    sym.documentation = "array " + node->name;
    sym.fileUri = currentFile;
    currentScope->symbols[node->name] = sym;
    symbolRefs.push_back({node->range, sym});
}

void SemanticAnalyzer::visitUsing(const UsingNode* node) {
    loadModuleSymbols(node->libName, node->range);
    DocumentSymbolInfo docSym;
    docSym.name = "using " + node->libName;
    docSym.kind = "Module";
    docSym.range = node->range;
    docSym.selectionRange = node->range;
    documentSymbols.push_back(docSym);
}

void SemanticAnalyzer::visitInclude(const IncludeNode* node) {
    loadModuleSymbols(node->filename, node->range);
    DocumentSymbolInfo docSym;
    docSym.name = "include " + node->filename;
    docSym.kind = "Module";
    docSym.range = node->range;
    docSym.selectionRange = node->range;
    documentSymbols.push_back(docSym);
}

HoverInfo SemanticAnalyzer::getHover(int line, int col) const {
    const SymbolRef* best = nullptr;
    for (const auto& ref : symbolRefs) {
        if (ref.range.contains(line, col)) {
            // Find most specific (innermost) match
            if (!best || (ref.range.end.line - ref.range.start.line < best->range.end.line - best->range.start.line) ||
                (ref.range.end.column - ref.range.start.column < best->range.end.column - best->range.start.column)) {
                best = &ref;
            }
        }
    }

    if (!best) return {};

    HoverInfo info;
    info.found = true;
    info.range = best->range;
    std::ostringstream ss;
    ss << "```foxlang\n";
    if (best->symbol.kind == SymbolKind::Function || best->symbol.kind == SymbolKind::Builtin) {
        ss << best->symbol.returnType << " " << best->symbol.name << "(";
        for (size_t i = 0; i < best->symbol.params.size(); i++) {
            if (i > 0) ss << ", ";
            ss << best->symbol.params[i].type << " " << best->symbol.params[i].name;
        }
        ss << ")";
    } else {
        ss << best->symbol.type << " " << best->symbol.name;
    }
    ss << "\n```";
    if (!best->symbol.documentation.empty() && best->symbol.documentation != best->symbol.name) {
        ss << "\n\n" << best->symbol.documentation;
    }
    info.markdown = ss.str();
    return info;
}

DefinitionInfo SemanticAnalyzer::getDefinition(int line, int col) const {
    for (const auto& ref : symbolRefs) {
        if (ref.range.contains(line, col)) {
            if (ref.symbol.declRange.start.line > 0) {
                DefinitionInfo info;
                info.found = true;
                info.range = ref.symbol.declRange;
                info.fileUri = ref.symbol.fileUri.empty() ? currentFile : ref.symbol.fileUri;
                return info;
            }
        }
    }
    return {};
}

std::vector<CompletionItem> SemanticAnalyzer::getCompletions(int line, int col) const {
    (void)line;
    (void)col;
    std::vector<CompletionItem> items;
    std::unordered_set<std::string> seen;

    auto add = [&](std::string label, std::string kind, std::string detail, std::string doc) {
        if (seen.insert(label).second) {
            items.push_back({std::move(label), std::move(kind), std::move(detail), std::move(doc)});
        }
    };

    // 1. Language keywords
    static const std::vector<std::string> keywords = {
        "if", "else", "while", "for", "switch", "case", "default",
        "break", "continue", "return", "int", "float", "string", "bool", "void",
        "true", "false", "array", "set", "get", "size", "using", "include", "global"
    };
    for (const auto& kw : keywords) {
        add(kw, "Keyword", "FoxLang keyword", "");
    }

    // 2. All visible symbols from root scope and symbol refs
    std::vector<Symbol> allSymbols;
    rootScope->getAllSymbols(allSymbols);
    for (const auto& ref : symbolRefs) {
        allSymbols.push_back(ref.symbol);
    }

    for (const auto& sym : allSymbols) {
        std::string kindStr = (sym.kind == SymbolKind::Function || sym.kind == SymbolKind::Builtin) ? "Function" : "Variable";
        std::string detail = sym.type + " " + sym.name;
        if (sym.kind == SymbolKind::Function || sym.kind == SymbolKind::Builtin) {
            detail = sym.returnType + " " + sym.name + "(";
            for (size_t i = 0; i < sym.params.size(); i++) {
                if (i > 0) detail += ", ";
                detail += sym.params[i].type + " " + sym.params[i].name;
            }
            detail += ")";
        }
        add(sym.name, kindStr, detail, sym.documentation);
    }

    return items;
}

std::vector<DocumentSymbolInfo> SemanticAnalyzer::getDocumentSymbols() const {
    return documentSymbols;
}

} // namespace foxlang
