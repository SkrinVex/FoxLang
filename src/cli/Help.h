#pragma once
#include <ostream>
#include <string>

namespace skrumi::cli {

// skrumi help, skrumi --help and skrumi with no arguments: every command in brief.
void printHelp(std::ostream& out, const std::string& version);

// skrumi help <command> and skrumi <command> --help; false for an unknown command.
bool printCommandHelp(std::ostream& out, const std::string& command);

// The commands that have details, for the message about an unknown one.
std::string commandNames();

} // namespace skrumi::cli
