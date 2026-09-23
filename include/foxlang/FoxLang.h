#pragma once
#include <string>
#include <vector>
#include <set>
#include "foxlang/Token.h"
#include "foxlang/AST.h"
#include "foxlang/Lexer.h"
#include "foxlang/Parser.h"
#include "foxlang/Context.h"
#include "foxlang/Runtime.h"
#include "foxlang/Platform.h"

namespace foxlang {

struct RunResult {
    bool success = false;
    int exitCode = 0;
    std::string errorMessage;
};

struct InterpreterOptions {
    std::string foxHome;
    bool loadDotEnv = true;
    std::string workingDir = ".";
    std::shared_ptr<const SourceProvider> sources;
};

class Interpreter {
public:
    Interpreter();
    explicit Interpreter(InterpreterOptions options);
    ~Interpreter() = default;

    // Run program from file
    RunResult runFile(const std::string& filepath);

    // Run program directly from source string
    RunResult runSource(const std::string& source, const std::string& scriptPath = "<eval>");

    Context& getContext();
    const Context& getContext() const;
    void reset();

    void setGlobal(const std::string& name, const std::string& type, const std::string& value);
    Value getGlobal(const std::string& name) const;

    const std::set<std::string>& getLoadedModules() const;
    void executeInclude(const std::string& path, const std::string& currentFile, bool importOnly);
    void executeUsing(const std::string& libName, const std::string& currentFile);

    static std::string getVersion();

private:
    InterpreterOptions options;
    Context globalContext;
    std::set<std::string> loadedModules;
    std::shared_ptr<const SourceProvider> sources;
    void executeModule(const std::string& identity, bool importOnly);
};

} // namespace foxlang

using foxlang::RunResult;
using foxlang::InterpreterOptions;
using foxlang::Interpreter;
