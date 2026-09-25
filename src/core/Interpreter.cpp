#include "foxlang/FoxLang.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace foxlang {

namespace {

bool isDeclaration(const Node* stmt) {
    return dynamic_cast<const FuncDefNode*>(stmt) || dynamic_cast<const VarDeclNode*>(stmt) ||
           dynamic_cast<const ArrayDeclNode*>(stmt) || dynamic_cast<const UsingNode*>(stmt) ||
           dynamic_cast<const IncludeNode*>(stmt);
}

// Imports bring in declarations; calls on a module's top level are its own demo code.
void runModule(BlockNode& program, Context& ctx, bool importOnly) {
    runtime::StackGuard& guard = runtime::stackGuard();
    for (auto& stmt : program.stmts) {
        if (!stmt || (importOnly && !isDeclaration(stmt.get()))) continue;
        guard.line = stmt->range.start.line;
        guard.file = program.file;
        stmt->eval(ctx);
    }
}

// Parses a source, naming the file in a syntax error that the lexer could not name.
std::unique_ptr<BlockNode> parseSource(const std::string& text, const std::string& identity) {
    try {
        Lexer lexer(text);
        Parser parser(lexer.tokenize(), identity);
        return parser.parseProgram();
    } catch (SyntaxError& error) {
        if (error.file().empty()) error.setFile(runtime::displayPath(identity));
        throw;
    }
}

Interpreter& interpreterOf(Context& ctx) {
    if (!ctx.interpreter) throw std::runtime_error("Module Error: imports need an Interpreter");
    return *ctx.interpreter;
}

} // namespace

void executeIncludeHook(const std::string& path, Context& ctx, const std::string& currentFile, bool importOnly) {
    interpreterOf(ctx).executeInclude(path, currentFile, importOnly);
}

void executeUsingHook(const std::string& libName, Context& ctx, const std::string& currentFile) {
    interpreterOf(ctx).executeUsing(libName, currentFile);
}

Interpreter::Interpreter() : Interpreter(InterpreterOptions{}) {}

Interpreter::Interpreter(InterpreterOptions opts) : options(std::move(opts)) {
    globalContext.interpreter = this;
    sources = options.sources ? options.sources : filesystemSources(options.foxHome);
}

std::string Interpreter::getVersion() {
    return FOXLANG_VERSION;
}

Context& Interpreter::getContext() {
    return globalContext;
}

const Context& Interpreter::getContext() const {
    return globalContext;
}

void Interpreter::reset() {
    globalContext.graphics.reset();
    globalContext.server.reset();
    globalContext.variables.clear();
    globalContext.functions.clear();
    globalContext.structs.clear();
    loadedModules.clear();
}

void Interpreter::setGlobal(const std::string& name, const std::string& type, const std::string& value) {
    globalContext.defineVar(name, type, {type, value});
}

Value Interpreter::getGlobal(const std::string& name) const {
    return globalContext.getVar(name);
}

const std::set<std::string>& Interpreter::getLoadedModules() const {
    return loadedModules;
}

void Interpreter::executeInclude(const std::string& path, const std::string& currentFile, bool importOnly) {
    executeModule(sources->resolve({path, false}, currentFile), importOnly);
}

void Interpreter::executeModule(const std::string& fullPath, bool importOnly) {
    if (loadedModules.count(fullPath)) return;
    auto program = parseSource(sources->read(fullPath), fullPath);
    loadedModules.insert(fullPath);
    // The importing statement resumes after the module, so its location is restored.
    runtime::StackGuard& guard = runtime::stackGuard();
    int line = guard.line;
    const std::string* file = guard.file;
    runModule(*program, globalContext, importOnly);
    guard.line = line;
    guard.file = file;
}

void Interpreter::executeUsing(const std::string& libName, const std::string& currentFile) {
    executeModule(sources->resolve({libName, true}, currentFile), true);
}

RunResult Interpreter::runFile(const std::string& filepath) {
    if (options.loadDotEnv) {
        runtime::loadDotEnv(filepath);
    }

    std::ifstream file(platform::pathFromUtf8(filepath), std::ios::binary);
    if (!file.is_open()) {
        return {false, 1, "could not open file '" + filepath + "'"};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::error_code ec;
    auto identity = platform::pathToUtf8(std::filesystem::weakly_canonical(platform::pathFromUtf8(filepath), ec));
    return runSource(buffer.str(), ec ? filepath : identity);
}

RunResult Interpreter::runSource(const std::string& source, const std::string& scriptPath) {
    runtime::StackGuard& guard = runtime::stackGuard();
    guard.line = 0;
    guard.file = nullptr;
    auto located = [&](const std::string& message) { return runtime::locate(message, scriptPath); };
    try {
        auto program = parseSource(source, scriptPath);
        loadedModules.insert(scriptPath);
        program->eval(globalContext);
        return {true, 0, ""};
    } catch (const ExitRequest& request) {
        return {request.code == 0, request.code, ""};
    } catch (const SyntaxError& error) {
        return {false, 1, error.what()};
    } catch (const ReturnValue&) {
        return {false, 1, located("Runtime Error: 'return' outside of a function")};
    } catch (const BreakException&) {
        return {false, 1, located("Runtime Error: 'break' outside of loop in global scope")};
    } catch (const ContinueException&) {
        return {false, 1, located("Runtime Error: 'continue' outside of loop in global scope")};
    } catch (const std::exception& e) {
        return {false, 1, located(e.what())};
    }
}

} // namespace foxlang
