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
    for (auto& stmt : program.stmts) {
        if (stmt && (!importOnly || isDeclaration(stmt.get()))) stmt->eval(ctx);
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
    globalContext.releaseArrays();
    globalContext.variables.clear();
    globalContext.functions.clear();
    globalContext.arrays.clear();
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
    Lexer lexer(sources->read(fullPath));
    Parser parser(lexer.tokenize(), fullPath);
    auto program = parser.parseProgram();
    loadedModules.insert(fullPath);
    runModule(*program, globalContext, importOnly);
}

void Interpreter::executeUsing(const std::string& libName, const std::string& currentFile) {
    executeModule(sources->resolve({libName, true}, currentFile), true);
}

RunResult Interpreter::runFile(const std::string& filepath) {
    if (options.loadDotEnv) {
        runtime::loadDotEnv(filepath);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, 1, "could not open file '" + filepath + "'"};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::error_code ec;
    auto identity = std::filesystem::weakly_canonical(filepath, ec).string();
    return runSource(buffer.str(), ec ? filepath : identity);
}

// Reports the line the interpreter last entered, unless the message already names one.
static std::string withLine(const std::string& message) {
    int line = runtime::stackGuard().line;
    if (line <= 0 || message.find(" [line ") != std::string::npos) return message;
    return message + " [line " + std::to_string(line) + "]";
}

RunResult Interpreter::runSource(const std::string& source, const std::string& scriptPath) {
    runtime::stackGuard().line = 0;
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize(), scriptPath);
        auto program = parser.parseProgram();
        loadedModules.insert(scriptPath);
        program->eval(globalContext);
        return {true, 0, ""};
    } catch (const ExitRequest& request) {
        return {request.code == 0, request.code, ""};
    } catch (const ReturnValue&) {
        return {false, 1, withLine("Runtime Error: 'return' outside of a function")};
    } catch (const BreakException&) {
        return {false, 1, "Runtime Error: 'break' outside of loop in global scope"};
    } catch (const ContinueException&) {
        return {false, 1, "Runtime Error: 'continue' outside of loop in global scope"};
    } catch (const std::exception& e) {
        return {false, 1, withLine(e.what())};
    }
}

} // namespace foxlang
