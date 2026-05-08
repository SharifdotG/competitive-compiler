#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

struct Symbol;
struct FuncSymbol;
struct Visitor;

struct Loc {
    int line = 0;
    int col  = 0;
};

struct Type {
    enum Kind { INT, FLOAT, STRING, BOOL, ARRAY, VOID_T, ERROR_T };
    Kind kind = ERROR_T;
    Kind elem = ERROR_T;     // valid when kind == ARRAY
    int  arrayLen = 0;       // valid when kind == ARRAY

    bool operator==(const Type& o) const {
        if (kind != o.kind) return false;
        if (kind == ARRAY) return elem == o.elem && arrayLen == o.arrayLen;
        return true;
    }
    bool operator!=(const Type& o) const { return !(*this == o); }
    bool isNumeric() const { return kind == INT || kind == FLOAT; }
    bool isError()   const { return kind == ERROR_T; }
};

inline Type tyInt()    { Type t; t.kind = Type::INT;    return t; }
inline Type tyFloat()  { Type t; t.kind = Type::FLOAT;  return t; }
inline Type tyStr()    { Type t; t.kind = Type::STRING; return t; }
inline Type tyBool()   { Type t; t.kind = Type::BOOL;   return t; }
inline Type tyError()  { Type t; t.kind = Type::ERROR_T;return t; }
inline Type tyArray(Type::Kind elem, int n) {
    Type t; t.kind = Type::ARRAY; t.elem = elem; t.arrayLen = n; return t;
}

const char* typeName(const Type& t);

// ---------- AST nodes ----------

struct Node {
    Loc loc;
    virtual ~Node() = default;
    virtual void accept(Visitor&) = 0;
};

struct Expr : Node {
    Type type;   // filled by sema
};

struct Stmt : Node {};

struct IntLit : Expr    { long long val = 0;   void accept(Visitor& v) override; };
struct FloatLit : Expr  { double val = 0;      void accept(Visitor& v) override; };
struct StringLit : Expr { std::string val;     void accept(Visitor& v) override; };
struct BoolLit : Expr   { bool val = false;    void accept(Visitor& v) override; };

struct VarRef : Expr {
    std::string name;
    Symbol* sym = nullptr;     // resolved by sema
    void accept(Visitor& v) override;
};

struct IndexExpr : Expr {
    std::string name;          // arr[idx]
    Symbol* sym = nullptr;
    std::unique_ptr<Expr> idx;
    void accept(Visitor& v) override;
};

struct CallExpr : Expr {
    std::string callee;
    FuncSymbol* funcSym = nullptr;
    std::vector<std::unique_ptr<Expr>> args;
    bool        isBuiltin = false;
    std::string builtinResolved;   // e.g. "__rt_print_int" — set by IRGen
    void accept(Visitor& v) override;
};

struct BinOp : Expr {
    enum Op { Add, Sub, Mul, Div, Mod, Eq, Neq, Lt, Le, Gt, Ge, And, Or };
    Op op = Add;
    std::unique_ptr<Expr> lhs, rhs;
    void accept(Visitor& v) override;
};

struct UnOp : Expr {
    enum Op { Neg, Not };
    Op op = Neg;
    std::unique_ptr<Expr> operand;
    void accept(Visitor& v) override;
};

struct Block : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
    void accept(Visitor& v) override;
};

struct DeclStmt : Stmt {
    std::string name;
    Type        declaredType;
    std::unique_ptr<Expr> init;       // may be null
    bool isArray  = false;
    int  arrayLen = 0;
    Symbol* sym   = nullptr;
    void accept(Visitor& v) override;
};

struct AssignStmt : Stmt {
    enum Op { Assign, PlusEq, MinusEq };
    Op op = Assign;
    std::unique_ptr<Expr> target;     // VarRef or IndexExpr
    std::unique_ptr<Expr> value;
    void accept(Visitor& v) override;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenS;
    std::unique_ptr<Stmt> elseS;       // may be null
    void accept(Visitor& v) override;
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> body;
    void accept(Visitor& v) override;
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt>       init;     // may be null
    std::unique_ptr<Expr>       cond;
    std::unique_ptr<AssignStmt> step;
    std::unique_ptr<Stmt>       body;
    void accept(Visitor& v) override;
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;          // may be null
    void accept(Visitor& v) override;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    void accept(Visitor& v) override;
};

struct BreakStmt    : Stmt { void accept(Visitor& v) override; };
struct ContinueStmt : Stmt { void accept(Visitor& v) override; };

struct Param {
    Type        type;
    std::string name;
    Loc         loc;
    Symbol*     sym = nullptr;
};

struct FuncDecl : Node {
    std::string name;
    Type        returnType;
    std::vector<Param> params;
    std::unique_ptr<Block> body;
    FuncSymbol* sym = nullptr;
    void accept(Visitor& v) override;
};

struct Program : Node {
    std::vector<std::unique_ptr<FuncDecl>> funcs;
    void accept(Visitor& v) override;
};

// ---------- Visitor ----------

struct Visitor {
    virtual ~Visitor() = default;
    virtual void visit(IntLit&)       {}
    virtual void visit(FloatLit&)     {}
    virtual void visit(StringLit&)    {}
    virtual void visit(BoolLit&)      {}
    virtual void visit(VarRef&)       {}
    virtual void visit(IndexExpr&)    {}
    virtual void visit(CallExpr&)     {}
    virtual void visit(BinOp&)        {}
    virtual void visit(UnOp&)         {}
    virtual void visit(Block&)        {}
    virtual void visit(DeclStmt&)     {}
    virtual void visit(AssignStmt&)   {}
    virtual void visit(IfStmt&)       {}
    virtual void visit(WhileStmt&)    {}
    virtual void visit(ForStmt&)      {}
    virtual void visit(ReturnStmt&)   {}
    virtual void visit(ExprStmt&)     {}
    virtual void visit(BreakStmt&)    {}
    virtual void visit(ContinueStmt&) {}
    virtual void visit(FuncDecl&)     {}
    virtual void visit(Program&)      {}
};

// ---------- Pretty-printer ----------

void printAst(Program& p, FILE* out);
