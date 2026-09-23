#include "foxlang/FoxLang.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <typeinfo>

namespace foxlang {

void executeIncludeHook(const std::string& path, Context& ctx, const std::string& currentFile, bool importOnly) {
    if (ctx.interpreter) {
        ctx.interpreter->executeInclude(path, currentFile, importOnly);
    } else {
        // Fallback standalone include execution
        std::string fullPath = runtime::resolveFoxFile(path, currentFile, "");
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            throw std::runtime_error("Module Error: Cannot open file '" + fullPath + "'");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();

        Lexer lexer(buffer.str());
        Parser parser(lexer.tokenize(), fullPath);
        auto program = parser.parseProgram();

        for (auto& stmt : program->stmts) {
            if (!stmt) continue;
            if (importOnly) {
                if (dynamic_cast<FuncDefNode*>(stmt.get()) ||
                    dynamic_cast<VarDeclNode*>(stmt.get()) ||
                    dynamic_cast<GlobalVarDeclNode*>(stmt.get()) ||
                    dynamic_cast<UsingNode*>(stmt.get()) ||
                    dynamic_cast<IncludeNode*>(stmt.get())) {
                    stmt->eval(ctx);
                }
            } else {
                stmt->eval(ctx);
            }
        }
    }
}

void executeUsingHook(const std::string& libName, Context& ctx, const std::string& currentFile) {
    if (ctx.interpreter) {
        ctx.interpreter->executeUsing(libName, currentFile);
    } else {
        std::string module = libName;
        if (module.size() < 4 || module.substr(module.size() - 4) != ".fox") {
            module += ".fox";
        }
        std::vector<std::string> candidates = {"std/" + module, module};
        std::string lastError;
        for (const auto& candidate : candidates) {
            try {
                executeIncludeHook(candidate, ctx, currentFile, true);
                return;
            } catch (const std::runtime_error& e) {
                lastError = e.what();
            }
        }
        throw std::runtime_error("Module Error: Module '" + libName + "' not found. " + lastError);
    }
}

Interpreter::Interpreter() {
    globalContext.interpreter = this;
}

Interpreter::Interpreter(InterpreterOptions opts) : options(std::move(opts)) {
    globalContext.interpreter = this;
}

std::string Interpreter::getVersion() {
    return "5.4.7";
}

Context& Interpreter::getContext() {
    return globalContext;
}

const Context& Interpreter::getContext() const {
    return globalContext;
}

void Interpreter::reset() {
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
    std::string fullPath = runtime::resolveFoxFile(path, currentFile, options.foxHome);
    if (loadedModules.count(fullPath)) return;
    loadedModules.insert(fullPath);

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        throw std::runtime_error("Module Error: Cannot open file '" + fullPath + "'");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    Lexer lexer(buffer.str());
    Parser parser(lexer.tokenize(), fullPath);
    auto program = parser.parseProgram();

    for (auto& stmt : program->stmts) {
        if (!stmt) continue;
        if (importOnly) {
            if (dynamic_cast<FuncDefNode*>(stmt.get()) ||
                dynamic_cast<VarDeclNode*>(stmt.get()) ||
                dynamic_cast<GlobalVarDeclNode*>(stmt.get()) ||
                dynamic_cast<UsingNode*>(stmt.get()) ||
                dynamic_cast<IncludeNode*>(stmt.get())) {
                stmt->eval(globalContext);
            }
        } else {
            stmt->eval(globalContext);
        }
    }
}

void Interpreter::executeUsing(const std::string& libName, const std::string& currentFile) {
    std::string module = libName;
    if (module.size() < 4 || module.substr(module.size() - 4) != ".fox") {
        module += ".fox";
    }

    std::vector<std::string> candidates = {"std/" + module, module};
    std::string lastError;
    for (const auto& candidate : candidates) {
        try {
            executeInclude(candidate, currentFile, true);
            return;
        } catch (const std::runtime_error& e) {
            lastError = e.what();
        }
    }
    throw std::runtime_error("Module Error: Module '" + libName + "' not found. " + lastError);
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
    return runSource(buffer.str(), filepath);
}

RunResult Interpreter::runSource(const std::string& source, const std::string& scriptPath) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize(), scriptPath);
        auto program = parser.parseProgram();

        program->eval(globalContext);
        return {true, 0, ""};
    } catch (const BreakException&) {
        return {false, 1, "Runtime Error: 'break' outside of loop in global scope"};
    } catch (const ContinueException&) {
        return {false, 1, "Runtime Error: 'continue' outside of loop in global scope"};
    } catch (const std::exception& e) {
        return {false, 1, e.what()};
    }
}

} // namespace foxlang
