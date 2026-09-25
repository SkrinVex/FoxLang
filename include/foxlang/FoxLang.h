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
#include "foxlang/Builtins.h"
#include "foxlang/Platform.h"
#include "foxlang/Debug.h"

namespace foxlang {

struct RunResult {
    bool success = false;
    int exitCode = 0;
    std::string errorMessage;
};

struct InterpreterOptions {
    std::string foxHome;
    bool loadDotEnv = true;
    std::shared_ptr<const SourceProvider> sources;
    // Command-line arguments after the program name, returned by os_args().
    std::vector<std::string> arguments;
};

class Interpreter {
public:
    Interpreter();
    explicit Interpreter(InterpreterOptions options);
    ~Interpreter(); // frees the program's rings of containers too

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
    const std::vector<std::string>& getArguments() const { return options.arguments; }
    const SourceProvider& getSources() const { return *sources; }
    void executeInclude(const std::string& path, const std::string& currentFile, bool importOnly);
    // With an alias, `using math as m;` also defines m: a map of the module's functions
    // and variables, so m.sqrt(2) and m.PI work.
    void executeUsing(const std::string& libName, const std::string& currentFile, const std::string& alias = "");

    static std::string getVersion();

private:
    InterpreterOptions options;
    Context globalContext;
    std::set<std::string> loadedModules;
    // The top-level functions and variables each loaded module declares, for aliases.
    std::map<std::string, std::vector<std::string>> moduleFunctions, moduleVariables;
    std::shared_ptr<const SourceProvider> sources;
    void executeModule(const std::string& identity, bool importOnly);
};

} // namespace foxlang
