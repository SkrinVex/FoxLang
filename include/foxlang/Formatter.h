#pragma once
#include <string>

namespace foxlang {

// The canonical layout of FoxLang source: four spaces per level of {}, [] and (),
// `case` and `default` one level above the statements they select, no trailing
// whitespace, no tabs in indentation, at most one blank line in a row, none at the
// start of a block or of the file, LF line ends and a final newline. The code
// itself is never rewritten, so formatting cannot change what a program does.
std::string formatSource(const std::string& source);

} // namespace foxlang
