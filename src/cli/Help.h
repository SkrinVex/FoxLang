#pragma once
#include <ostream>
#include <string>

namespace foxlang::cli {

// foxlang help, foxlang --help and foxlang with no arguments: every command in brief.
void printHelp(std::ostream& out, const std::string& version);

// foxlang help <command> and foxlang <command> --help; false for an unknown command.
bool printCommandHelp(std::ostream& out, const std::string& command);

// The commands that have details, for the message about an unknown one.
std::string commandNames();

} // namespace foxlang::cli
