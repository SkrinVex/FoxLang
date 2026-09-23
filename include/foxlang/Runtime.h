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

Value jsonGet(const std::string& json, const std::string& path);
Value jsonEscape(const std::string& text);

void loadDotEnv(const std::string& scriptPath);
std::string resolveFoxFile(const std::string& requested, const std::string& currentFile, const std::string& foxHome);

} // namespace runtime
} // namespace foxlang
