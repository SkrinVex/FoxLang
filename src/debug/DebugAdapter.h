#pragma once
#include <string>
#include <vector>

namespace foxlang::debug {

// `foxlang debug-adapter [--connect host:port]`: a Debug Adapter Protocol server that
// runs the program named by the editor's launch request in this process. Without
// --connect the protocol uses stdin/stdout and the program's output becomes output
// events; with it the program keeps the terminal it was started in.
int runAdapter(const std::vector<std::string>& options);

} // namespace foxlang::debug
