#include "symtab.hpp"

SymbolTable::SymbolTable() {
    // No global scope by default; sema enters one before processing the program.
}

void SymbolTable::enterScope() {
    auto s = std::make_unique<Scope>();
    s->parent = stack_.empty() ? nullptr : stack_.back();
    Scope* raw = s.get();
    ownedScopes_.push_back(std::move(s));
    stack_.push_back(raw);
}

void SymbolTable::exitScope() {
    if (!stack_.empty()) stack_.pop_back();
    // Scope object kept alive in ownedScopes_ — symbols outlive parsing.
}

Symbol* SymbolTable::declare(const std::string& name, const Type& t, Loc loc, bool isParam) {
    Scope* cur = current();
    if (!cur) return nullptr;
    if (cur->table.count(name)) return nullptr;
    auto s = std::make_unique<Symbol>();
    s->name = name;
    s->type = t;
    s->declLoc = loc;
    s->isParam = isParam;
    Symbol* p = s.get();
    cur->table[name] = std::move(s);
    return p;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (Scope* s = current(); s; s = s->parent) {
        auto it = s->table.find(name);
        if (it != s->table.end()) return it->second.get();
    }
    return nullptr;
}

Symbol* SymbolTable::lookupInCurrentScope(const std::string& name) {
    Scope* s = current();
    if (!s) return nullptr;
    auto it = s->table.find(name);
    return it == s->table.end() ? nullptr : it->second.get();
}

FuncSymbol* SymbolTable::declareFunc(const std::string& name, Type ret,
                                     std::vector<Type> params, bool isBuiltin, Loc loc) {
    if (funcs_.count(name)) return nullptr;
    auto f = std::make_unique<FuncSymbol>();
    f->name = name;
    f->returnType = ret;
    f->paramTypes = std::move(params);
    f->isBuiltin = isBuiltin;
    f->declLoc = loc;
    FuncSymbol* p = f.get();
    funcs_[name] = std::move(f);
    return p;
}

FuncSymbol* SymbolTable::lookupFunc(const std::string& name) {
    auto it = funcs_.find(name);
    return it == funcs_.end() ? nullptr : it->second.get();
}
