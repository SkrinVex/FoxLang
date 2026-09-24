#pragma once
#include <string>

namespace foxlang {

struct Context;

// What a debugger learns from the interpreter. Without a debugger attached the
// interpreter pays one pointer test per statement and per call.
class DebugHook {
public:
    virtual ~DebugHook() = default;
    // Before a statement of a block runs; the debugger may stop here.
    virtual void statement(const std::string* file, int line) = 0;
    // A block begins and ends; after it the statement that contains it is current again.
    virtual void enterScope(Context& scope) = 0;
    virtual void leaveScope() = 0;
    virtual void enterFunction(const std::string& name, const std::string* file, int line, Context& scope) = 0;
    virtual void leaveFunction() = 0;
    // A runtime error is leaving the statement that raised it; the program is still intact.
    virtual void error(const std::string& message) = 0;
};

namespace runtime {
DebugHook* debugHook();
void setDebugHook(DebugHook* hook);
} // namespace runtime

} // namespace foxlang
