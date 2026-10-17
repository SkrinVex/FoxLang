#pragma once
#include "skrumi/Skrumi.h"
#include <string>
#include <vector>

namespace skrumi::cli {
enum class ConsoleMode { Default, Console, Hidden };
struct LaunchOptions {
    InterpreterOptions interpreter;
    ConsoleMode console = ConsoleMode::Default;
};
// Only the reserved --skrumi-* namespace is consumed. -- protects all remaining args.
LaunchOptions parseLaunch(const std::vector<std::string>& arguments);
void configureConsole(ConsoleMode mode);
void reportError(const std::string& message);
} // namespace skrumi::cli
