#pragma once
#include "ir.hpp"

// Runs a small fixed-point optimization pipeline on each IR function:
//  - Constant folding + algebraic identities
//  - Intra-block constant propagation (temp -> const)
//  - Dead-code elimination of unused temp definitions
void optimizeIR(IRProgram& p);
