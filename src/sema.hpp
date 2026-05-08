#pragma once
#include "ast.hpp"
#include "symtab.hpp"

// Runs semantic analysis: scope resolution, type checking, return-path check.
// Returns the number of errors found (0 = success).
// Mutates AST nodes — fills in Expr::type, VarRef::sym, IndexExpr::sym,
// CallExpr::funcSym/isBuiltin, DeclStmt::sym, Param::sym, FuncDecl::sym.
int runSemanticAnalysis(Program &program, SymbolTable &symtab);
