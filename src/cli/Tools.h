#pragma once
#include <string>
#include <vector>

namespace foxlang::cli {

// foxlang fmt [--check] [paths...]: lays out .fox files (see formatSource); --check
// only lists the files that would change and fails when there are any.
int format(const std::vector<std::string>& arguments);

// foxlang test [--filter text] [paths...]: runs the parameterless test_ functions of
// every *_test.fox file, each in a fresh program; fails when any test fails.
int test(const std::vector<std::string>& arguments);

// foxlang disasm <file.fox>: prints the bytecode of the program and of every function
// defined in it, in the order they appear in the file.
int disassemble(const std::vector<std::string>& arguments);

} // namespace foxlang::cli
