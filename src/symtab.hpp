#pragma once
#include "ast.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

struct Symbol {
    std::string name;
    Type        type;
    Loc         declLoc;
    bool        isParam = false;
    int         stackOffset = 0;   // filled by codegen (negative offset from rbp)
};

struct FuncSymbol {
    std::string       name;
    std::vector<Type> paramTypes;
    Type              returnType;
    bool              isBuiltin = false;
    Loc               declLoc;
};

struct Scope {
    std::unordered_map<std::string, std::unique_ptr<Symbol>> table;
    Scope* parent = nullptr;
};

class SymbolTable {
public:
    SymbolTable();
    ~SymbolTable() = default;

    void enterScope();
    void exitScope();

    // Returns nullptr if name already declared in current scope.
    Symbol* declare(const std::string& name, const Type& t, Loc loc, bool isParam = false);

    // Search up the scope chain. nullptr if not found.
    Symbol* lookup(const std::string& name);

    // Search only current (innermost) scope.
    Symbol* lookupInCurrentScope(const std::string& name);

    // Functions live in a flat global namespace.
    FuncSymbol* declareFunc(const std::string& name, Type ret,
                            std::vector<Type> params, bool isBuiltin = false,
                            Loc loc = {});
    FuncSymbol* lookupFunc(const std::string& name);

    Scope* current() { return stack_.empty() ? nullptr : stack_.back(); }

private:
    std::vector<std::unique_ptr<Scope>> ownedScopes_;  // alive for whole compilation
    std::vector<Scope*>                 stack_;        // current path
    std::unordered_map<std::string, std::unique_ptr<FuncSymbol>> funcs_;
};
