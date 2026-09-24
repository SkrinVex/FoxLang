#pragma once
#include <string>
#include <vector>
#include "foxlang/Context.h"

namespace foxlang {
namespace runtime {

bool isBuiltin(const std::string& name);
Value callBuiltin(const std::string& name, const std::vector<Value>& args, Context& ctx);

int getLogLevelThreshold();
std::string formatNumber(double val);

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

Value jsonGet(const std::string& json, const std::string& path);
Value jsonEscape(const std::string& text);

void loadDotEnv(const std::string& scriptPath);
std::string resolveFoxFile(const std::string& requested, const std::string& currentFile, const std::string& foxHome);

} // namespace runtime
} // namespace foxlang
