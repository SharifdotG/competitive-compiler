#pragma once
#include "ir.hpp"
#include <cstdio>

// Emit GAS Intel-syntax x86-64 assembly for an IR program.
// Output is intended to be assembled with `gcc -no-pie out.s runtime.o`.
void emitX86Asm(const IRProgram& p, FILE* out);
