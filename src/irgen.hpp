#pragma once
#include "ast.hpp"
#include "ir.hpp"

// Walks the (already type-checked) AST and produces TAC IR.
// Caller owns the returned IRProgram.
IRProgram *generateIR(Program &ast);
