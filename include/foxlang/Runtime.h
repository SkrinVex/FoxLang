#pragma once
#include <string>
#include <vector>
#include "foxlang/Context.h"
#include "foxlang/Builtins.h"

namespace foxlang {
namespace runtime {

// A builtin is its catalog entry plus the C++ that implements it.
struct Builtin;
const Builtin* findBuiltin(const std::string& name);
const BuiltinSpec& specOf(const Builtin& builtin);
// True when the arguments fit the builtin's parameter count and types. A FoxLang
// function with the same name is called instead when they do not.
bool acceptsArguments(const Builtin& builtin, const std::vector<Value>& args);
Value invoke(const Builtin& builtin, const std::vector<Value>& args, Context& ctx);

bool isBuiltin(const std::string& name);
Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& ctx);

int getLogLevelThreshold();
std::string formatNumber(double val);

// Recursion guard state. Kept in one translation unit rather than as inline
// thread_local data in a header, which compilers disagree about.
struct StackGuard {
    int depth = 0;
    int limit = 0;
    const char* origin = nullptr;
    size_t budget = 0;
    int line = 0; // Line of the statement being executed, for the error report.
};
StackGuard& stackGuard();

// Numeric access to stringly stored values. These report the offending text and
// the place that asked for it, instead of letting std::stoi/std::stod escape.
// Probes parse without building any message; the *what* overloads below describe
// the failure and are only reached when a value really is wrong.
bool tryNumber(const Value& value, double& out);
bool tryInt(const Value& value, long long& out);

double toNumber(const Value& value, const std::string& what);
int toInt(const Value& value, const std::string& what);
Text intText(const Value& value, const std::string& what);
Text intResult(long long result, const std::string& op);
Text realResult(double result);

// Converts a value for storage in a slot of the given type: a variable, a parameter
// or a return value. int and float convert both ways (float to int truncates, and
// must fit), any scalar becomes text in a string slot, everything else is an error.
void coerce(const std::string& type, Value& value, const std::string& what);

// JSON navigation by a dotted path. Object keys and array indexes are both path
// segments: "message.chat.id", "items.0.name". The empty path is the document itself.
Value jsonGet(const std::string& json, const std::string& path);
Value jsonEscape(const std::string& text);
int jsonCount(const std::string& json, const std::string& path);
std::string jsonType(const std::string& json, const std::string& path);

void loadDotEnv(const std::string& scriptPath);
std::string resolveFoxFile(const std::string& requested, const std::string& currentFile, const std::string& foxHome);

} // namespace runtime
} // namespace foxlang
