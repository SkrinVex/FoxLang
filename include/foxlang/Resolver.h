#pragma once

namespace foxlang {

struct FuncDefNode;
struct BlockNode;
struct Node;

// Numbers the local variables of a function body (parameters first) once, before its
// first call; the layout is kept on the body, shared by every copy of the function.
void resolveFunction(FuncDefNode& function);
// Numbers the variables of the blocks inside a program; its top level stays global.
void resolveProgram(BlockNode& program);
// Resolves the lambdas in an expression that runs outside any function.
void resolveExpression(Node& expression);

} // namespace foxlang
