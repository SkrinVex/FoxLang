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
    int line = 0;                     // Line of the statement being executed, for the error report,
    const std::string* file = nullptr; // and its source file (an interned name, never freed).
    int tryDepth = 0;                  // try blocks the running code is inside of
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

// A value as print() shows it: numbers and text as they are, arrays as [1, 2],
// maps as {key: value}, structs as Name{field: value}.
std::string display(const Value& value);
// The value a slot of this type starts with: 0, 0.0, "", false, an empty array or
// map, a struct with its defaults. Unknown types are an error.
Value zeroValue(const std::string& type, Context& ctx);
// A struct value built from positional arguments; missing trailing fields take their
// defaults.
Value construct(const StructType& type, std::vector<Value> args, Context& ctx);

// JSON navigation by a dotted path. Object keys and array indexes are both path
// segments: "message.chat.id", "items.0.name". The empty path is the document itself.
Value jsonGet(const std::string& json, const std::string& path);
Value jsonEscape(const std::string& text);
int jsonCount(const std::string& json, const std::string& path);
std::string jsonType(const std::string& json, const std::string& path);
// The value at a path as JSON text (a string keeps its quotes); false if absent.
bool jsonRaw(const std::string& json, const std::string& path, std::string& out);
// A copy of the document with raw JSON stored at the path. Missing object keys are
// created; an array index may name an element or the position right after the last.
std::string jsonSet(const std::string& json, const std::string& path, const std::string& raw);
bool jsonValid(const std::string& json);
// Members of an object (key, raw value) or elements of an array ("", raw value), in
// document order. Empty for anything else.
std::vector<std::pair<std::string, std::string>> jsonEntries(const std::string& json);

// Mustache-style HTML templates over a JSON document: {{path}} (HTML-escaped),
// {{{path}}} (raw), {{#each path}}...{{/each}}, {{#if path}}...{{else}}...{{/if}}.
std::string renderTemplate(const std::string& source, const std::string& json);

// A stable pointer for a source file name; AST blocks keep it to report errors.
const std::string* internFile(const std::string& name);
// A source identity as a person wants to read it: relative to the working directory
// when the file is inside it, and std/x.fox for the embedded standard library.
std::string displayPath(const std::string& identity);
// "file:line: message" for a runtime error at the statement last entered.
std::string locate(const std::string& message, const std::string& fallbackFile);

void loadDotEnv(const std::string& scriptPath);
std::string resolveFoxFile(const std::string& requested, const std::string& currentFile, const std::string& foxHome);

} // namespace runtime
} // namespace foxlang
