#include "sema.hpp"
#include "ast.hpp"
#include "symtab.hpp"
#include <cstdio>
#include <string>

namespace {

const char *binopName(BinOp::Op op) {
    static const char *n[] = {
        "+", "-", "*", "/", "%", "==", "!=", "<", "<=", ">", ">=", "&&", "||"};
    return n[op];
}

bool isLValueExpr(Expr *e) {
    return dynamic_cast<VarRef *>(e) || dynamic_cast<IndexExpr *>(e);
}

class SemaPass : public Visitor {
  public:
    SemaPass(SymbolTable &st) : symtab(st) {}

    int errors = 0;
    Type currentReturnType = tyError();
    int loopDepth = 0;
    bool inGlobalScope = true;

    void error(Loc loc, const std::string &msg) {
        ++errors;
        fprintf(stderr, "%d:%d: error: %s\n", loc.line, loc.col, msg.c_str());
    }

    // ---- Expressions ----

    void visit(IntLit &n) override { n.type = tyInt(); }
    void visit(FloatLit &n) override { n.type = tyFloat(); }
    void visit(StringLit &n) override { n.type = tyStr(); }
    void visit(BoolLit &n) override { n.type = tyBool(); }

    void visit(VarRef &n) override {
        Symbol *s = symtab.lookup(n.name);
        if (!s) {
            error(n.loc, "undeclared variable '" + n.name + "'");
            n.type = tyError();
            return;
        }
        n.sym = s;
        n.type = s->type;
    }

    void visit(IndexExpr &n) override {
        Symbol *s = symtab.lookup(n.name);
        if (!s) {
            error(n.loc, "undeclared variable '" + n.name + "'");
            n.type = tyError();
            n.idx->accept(*this);
            return;
        }
        n.sym = s;
        n.idx->accept(*this);
        if (s->type.kind != Type::ARRAY) {
            error(n.loc, "'" + n.name + "' is not an array (it is " +
                             typeName(s->type) + ")");
            n.type = tyError();
            return;
        }
        if (!n.idx->type.isError() && n.idx->type.kind != Type::INT) {
            error(n.idx->loc, std::string("array index must be int, got ") +
                                  typeName(n.idx->type));
        }
        Type elem;
        elem.kind = s->type.elem;
        n.type = elem;
    }

    void visit(CallExpr &n) override {
        FuncSymbol *fs = symtab.lookupFunc(n.callee);
        for (auto &a : n.args)
            a->accept(*this);

        if (!fs) {
            error(n.loc, "unknown function '" + n.callee + "'");
            n.type = tyError();
            return;
        }
        n.funcSym = fs;
        n.isBuiltin = fs->isBuiltin;

        if (fs->isBuiltin) {
            checkBuiltinCall(n);
            return;
        }

        // User function: standard arity + type check.
        if (fs->paramTypes.size() != n.args.size()) {
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "function '%s' expects %zu argument(s), got %zu",
                     n.callee.c_str(), fs->paramTypes.size(), n.args.size());
            error(n.loc, buf);
        } else {
            for (size_t i = 0; i < n.args.size(); ++i) {
                Type pt = fs->paramTypes[i];
                Type at = n.args[i]->type;
                if (at.isError())
                    continue;
                if (pt != at) {
                    char buf[160];
                    snprintf(buf, sizeof(buf),
                             "argument %zu of '%s': expected %s, got %s", i + 1,
                             n.callee.c_str(), typeName(pt), typeName(at));
                    error(n.args[i]->loc, buf);
                }
            }
        }
        n.type = fs->returnType;
    }

    void visit(BinOp &n) override {
        n.lhs->accept(*this);
        n.rhs->accept(*this);
        Type lt = n.lhs->type, rt = n.rhs->type;
        if (lt.isError() || rt.isError()) {
            n.type = tyError();
            return;
        }

        switch (n.op) {
        case BinOp::Add:
            if (lt.kind == Type::STRING && rt.kind == Type::STRING) {
                n.type = tyStr();
            } else if (lt.isNumeric() && rt.isNumeric() && lt == rt) {
                n.type = lt;
            } else {
                error(n.loc, std::string("operator + requires same numeric "
                                         "type or string, got ") +
                                 typeName(lt) + " and " + typeName(rt));
                n.type = tyError();
            }
            return;
        case BinOp::Sub:
        case BinOp::Mul:
        case BinOp::Div:
            if (lt.isNumeric() && rt.isNumeric() && lt == rt) {
                n.type = lt;
            } else {
                error(n.loc, std::string("operator ") + binopName(n.op) +
                                 " requires same numeric type, got " +
                                 typeName(lt) + " and " + typeName(rt));
                n.type = tyError();
            }
            return;
        case BinOp::Mod:
            if (lt.kind == Type::INT && rt.kind == Type::INT)
                n.type = tyInt();
            else {
                error(n.loc, "operator % requires int operands");
                n.type = tyError();
            }
            return;
        case BinOp::Eq:
        case BinOp::Neq:
            if (lt == rt)
                n.type = tyBool();
            else {
                error(n.loc, std::string("operator ") + binopName(n.op) +
                                 " requires same type, got " + typeName(lt) +
                                 " and " + typeName(rt));
                n.type = tyError();
            }
            return;
        case BinOp::Lt:
        case BinOp::Le:
        case BinOp::Gt:
        case BinOp::Ge:
            if (lt.isNumeric() && rt.isNumeric() && lt == rt)
                n.type = tyBool();
            else {
                error(n.loc, std::string("operator ") + binopName(n.op) +
                                 " requires same numeric type");
                n.type = tyError();
            }
            return;
        case BinOp::And:
        case BinOp::Or:
            if (lt.kind == Type::BOOL && rt.kind == Type::BOOL)
                n.type = tyBool();
            else {
                error(n.loc, std::string("operator ") + binopName(n.op) +
                                 " requires bool operands");
                n.type = tyError();
            }
            return;
        }
    }

    void visit(UnOp &n) override {
        n.operand->accept(*this);
        if (n.operand->type.isError()) {
            n.type = tyError();
            return;
        }
        if (n.op == UnOp::Neg) {
            if (n.operand->type.isNumeric())
                n.type = n.operand->type;
            else {
                error(n.loc, "unary - requires numeric operand");
                n.type = tyError();
            }
        } else { // Not
            if (n.operand->type.kind == Type::BOOL)
                n.type = tyBool();
            else {
                error(n.loc, "unary ! requires bool operand");
                n.type = tyError();
            }
        }
    }

    // ---- Statements ----

    void visit(Block &n) override {
        symtab.enterScope();
        for (auto &s : n.stmts)
            s->accept(*this);
        symtab.exitScope();
    }

    void visit(DeclStmt &n) override {
        if (symtab.lookupInCurrentScope(n.name)) {
            error(n.loc, "redeclaration of '" + n.name + "'");
            // Still process initializer to surface its errors.
            if (n.init)
                n.init->accept(*this);
            return;
        }
        Symbol *s = symtab.declare(n.name, n.declaredType, n.loc);
        n.sym = s;
        if (n.init) {
            n.init->accept(*this);
            if (n.declaredType.kind == Type::ARRAY) {
                error(n.loc, "array initializers not supported");
            } else if (!n.init->type.isError() &&
                       n.init->type != n.declaredType) {
                error(n.loc, std::string("initializer type ") +
                                 typeName(n.init->type) +
                                 " does not match declared type " +
                                 typeName(n.declaredType));
            }
        }
    }

    void visit(AssignStmt &n) override {
        n.value->accept(*this);
        n.target->accept(*this);
        if (n.target->type.isError() || n.value->type.isError())
            return;
        if (n.target->type.kind == Type::ARRAY) {
            error(n.loc, "cannot assign to entire array");
            return;
        }
        if (n.target->type != n.value->type) {
            error(n.loc,
                  std::string("type mismatch in assignment: target is ") +
                      typeName(n.target->type) + ", value is " +
                      typeName(n.value->type));
            return;
        }
        if (n.op != AssignStmt::Assign && !n.target->type.isNumeric()) {
            error(n.loc, "+= and -= require numeric operands");
        }
    }

    void visit(IfStmt &n) override {
        n.cond->accept(*this);
        if (!n.cond->type.isError() && n.cond->type.kind != Type::BOOL) {
            error(n.cond->loc, std::string("if condition must be bool, got ") +
                                   typeName(n.cond->type));
        }
        n.thenS->accept(*this);
        if (n.elseS)
            n.elseS->accept(*this);
    }

    void visit(WhileStmt &n) override {
        n.cond->accept(*this);
        if (!n.cond->type.isError() && n.cond->type.kind != Type::BOOL) {
            error(n.cond->loc,
                  std::string("while condition must be bool, got ") +
                      typeName(n.cond->type));
        }
        ++loopDepth;
        n.body->accept(*this);
        --loopDepth;
    }

    void visit(ForStmt &n) override {
        symtab.enterScope();
        if (n.init)
            n.init->accept(*this);
        n.cond->accept(*this);
        if (!n.cond->type.isError() && n.cond->type.kind != Type::BOOL) {
            error(n.cond->loc, "for condition must be bool");
        }
        n.step->accept(*this);
        ++loopDepth;
        n.body->accept(*this);
        --loopDepth;
        symtab.exitScope();
    }

    void visit(ReturnStmt &n) override {
        if (n.value) {
            n.value->accept(*this);
            if (!n.value->type.isError() &&
                n.value->type != currentReturnType) {
                error(n.loc, std::string("return type mismatch: expected ") +
                                 typeName(currentReturnType) + ", got " +
                                 typeName(n.value->type));
            }
        } else {
            if (currentReturnType.kind != Type::VOID_T) {
                error(n.loc, std::string("function must return ") +
                                 typeName(currentReturnType));
            }
        }
    }

    void visit(ExprStmt &n) override { n.expr->accept(*this); }

    void visit(BreakStmt &n) override {
        if (loopDepth == 0)
            error(n.loc, "break outside loop");
    }
    void visit(ContinueStmt &n) override {
        if (loopDepth == 0)
            error(n.loc, "continue outside loop");
    }

    void visit(FuncDecl &n) override {
        currentReturnType = n.returnType;
        symtab.enterScope();
        for (auto &p : n.params) {
            if (p.type.kind == Type::STRING) {
                error(
                    p.loc,
                    "string parameters are not supported in v1 (locals only)");
            }
            if (p.type.kind == Type::ARRAY) {
                error(p.loc,
                      "array parameters are not supported in v1 (locals only)");
            }
            if (symtab.lookupInCurrentScope(p.name)) {
                error(p.loc, "duplicate parameter '" + p.name + "'");
            } else {
                p.sym = symtab.declare(p.name, p.type, p.loc, /*isParam=*/true);
            }
        }
        if (n.body) {
            // Inline the body's statements at the function scope so params +
            // body share scope.
            for (auto &s : n.body->stmts)
                s->accept(*this);
        }
        symtab.exitScope();

        if (n.returnType.kind != Type::VOID_T && n.body &&
            !endsInReturn(n.body.get())) {
            error(n.loc,
                  "function '" + n.name + "' may not return on all paths");
        }
    }

    void visit(Program &n) override {
        installBuiltins();
        // Pre-declare user functions
        for (auto &f : n.funcs) {
            std::vector<Type> ptypes;
            for (auto &p : f->params)
                ptypes.push_back(p.type);
            FuncSymbol *fs = symtab.declareFunc(f->name, f->returnType, ptypes,
                                                /*isBuiltin=*/false, f->loc);
            if (!fs)
                error(f->loc, "duplicate function '" + f->name + "'");
            else
                f->sym = fs;
        }
        // Visit each function's body
        for (auto &f : n.funcs)
            f->accept(*this);
        if (!symtab.lookupFunc("main")) {
            fprintf(stderr, "error: no 'main' function defined\n");
            ++errors;
        }
    }

  private:
    SymbolTable &symtab;

    static bool endsInReturn(Stmt *s) {
        if (!s)
            return false;
        if (dynamic_cast<ReturnStmt *>(s))
            return true;
        if (auto *b = dynamic_cast<Block *>(s)) {
            if (b->stmts.empty())
                return false;
            return endsInReturn(b->stmts.back().get());
        }
        if (auto *i = dynamic_cast<IfStmt *>(s)) {
            return i->elseS && endsInReturn(i->thenS.get()) &&
                   endsInReturn(i->elseS.get());
        }
        return false;
    }

    void installBuiltins() {
        auto reg = [&](const char *name, Type ret, std::vector<Type> ps) {
            symtab.declareFunc(name, ret, std::move(ps), /*isBuiltin=*/true);
        };
        Type voidT;
        voidT.kind = Type::VOID_T;
        // print/read are dispatched specially — declare with empty params.
        reg("print", voidT, {});
        reg("read", voidT, {});
        // sort/reverse/binary_search dispatched specially (need array first
        // arg).
        reg("sort", voidT, {});
        reg("reverse", voidT, {});
        reg("binary_search", tyInt(), {});
        // Numeric utilities — overloaded; dispatched specially.
        reg("max", tyError(), {});
        reg("min", tyError(), {});
        reg("abs", tyError(), {});
        // swap — special (lvalues, same type).
        reg("swap", voidT, {});
        // len — special (array or string).
        reg("len", tyInt(), {});
        // gcd/lcm — fixed signatures.
        reg("gcd", tyInt(), {tyInt(), tyInt()});
        reg("lcm", tyInt(), {tyInt(), tyInt()});
    }

    void checkBuiltinCall(CallExpr &n) {
        const std::string &name = n.callee;

        if (name == "print") {
            if (n.args.size() != 1) {
                error(n.loc, "print() takes exactly 1 argument");
            } else {
                Type t = n.args[0]->type;
                if (!t.isError() && t.kind != Type::INT &&
                    t.kind != Type::FLOAT && t.kind != Type::STRING &&
                    t.kind != Type::BOOL) {
                    error(n.args[0]->loc,
                          std::string(
                              "print() supports int/float/string/bool, got ") +
                              typeName(t));
                }
            }
            n.type = tyVoid();
            return;
        }
        if (name == "read") {
            if (n.args.size() != 1) {
                error(n.loc, "read() takes exactly 1 argument");
            } else {
                Expr *a = n.args[0].get();
                if (!isLValueExpr(a)) {
                    error(
                        a->loc,
                        "read() argument must be a variable or array element");
                } else {
                    Type t = a->type;
                    if (!t.isError() && t.kind != Type::INT &&
                        t.kind != Type::FLOAT && t.kind != Type::STRING) {
                        error(a->loc,
                              std::string(
                                  "read() supports int/float/string, got ") +
                                  typeName(t));
                    }
                }
            }
            n.type = tyVoid();
            return;
        }
        if (name == "sort" || name == "reverse") {
            if (n.args.size() != 2) {
                error(n.loc, name + "() takes 2 arguments: array, length");
            } else {
                Type at = n.args[0]->type, lt = n.args[1]->type;
                if (!at.isError() && at.kind != Type::ARRAY) {
                    error(n.args[0]->loc,
                          name + "() first argument must be an array");
                } else if (!at.isError() && at.elem != Type::INT) {
                    error(n.args[0]->loc,
                          name + "() currently supports int arrays only");
                }
                if (!lt.isError() && lt.kind != Type::INT) {
                    error(n.args[1]->loc, name + "() length must be int");
                }
            }
            n.type = tyVoid();
            return;
        }
        if (name == "binary_search") {
            if (n.args.size() != 3) {
                error(n.loc,
                      "binary_search() takes 3 arguments: array, length, key");
            } else {
                Type at = n.args[0]->type, lt = n.args[1]->type,
                     kt = n.args[2]->type;
                if (!at.isError() &&
                    (at.kind != Type::ARRAY || at.elem != Type::INT)) {
                    error(n.args[0]->loc, "binary_search() requires int array");
                }
                if (!lt.isError() && lt.kind != Type::INT) {
                    error(n.args[1]->loc, "binary_search() length must be int");
                }
                if (!kt.isError() && kt.kind != Type::INT) {
                    error(n.args[2]->loc, "binary_search() key must be int");
                }
            }
            n.type = tyInt();
            return;
        }
        if (name == "max" || name == "min") {
            if (n.args.size() != 2) {
                error(n.loc, name + "() takes 2 arguments");
                n.type = tyError();
            } else {
                Type a = n.args[0]->type, b = n.args[1]->type;
                if (a.isError() || b.isError()) {
                    n.type = tyError();
                    return;
                }
                if (!a.isNumeric() || !b.isNumeric() || a != b) {
                    error(n.loc, name + "() requires two numeric operands of "
                                        "the same type");
                    n.type = tyError();
                } else {
                    n.type = a;
                }
            }
            return;
        }
        if (name == "abs") {
            if (n.args.size() != 1) {
                error(n.loc, "abs() takes 1 argument");
                n.type = tyError();
            } else {
                Type a = n.args[0]->type;
                if (a.isError()) {
                    n.type = tyError();
                    return;
                }
                if (!a.isNumeric()) {
                    error(n.loc, "abs() requires a numeric argument");
                    n.type = tyError();
                } else {
                    n.type = a;
                }
            }
            return;
        }
        if (name == "swap") {
            if (n.args.size() != 2) {
                error(n.loc, "swap() takes 2 arguments");
            } else {
                for (int i = 0; i < 2; ++i) {
                    if (!isLValueExpr(n.args[i].get())) {
                        error(n.args[i]->loc, "swap() argument must be a "
                                              "variable or array element");
                    }
                }
                if (!n.args[0]->type.isError() && !n.args[1]->type.isError() &&
                    n.args[0]->type != n.args[1]->type) {
                    error(
                        n.loc,
                        "swap() requires both arguments to have the same type");
                }
                if (!n.args[0]->type.isError() &&
                    n.args[0]->type.kind == Type::ARRAY) {
                    error(n.loc, "swap() does not support whole arrays");
                }
            }
            n.type = tyVoid();
            return;
        }
        if (name == "len") {
            if (n.args.size() != 1) {
                error(n.loc, "len() takes 1 argument");
            } else {
                Type a = n.args[0]->type;
                if (!a.isError() && a.kind != Type::ARRAY &&
                    a.kind != Type::STRING) {
                    error(n.args[0]->loc,
                          std::string("len() requires array or string, got ") +
                              typeName(a));
                }
            }
            n.type = tyInt();
            return;
        }
        if (name == "gcd" || name == "lcm") {
            // Fixed signature already checked above? No — we marked them as
            // builtin so we get here.
            if (n.args.size() != 2) {
                error(n.loc, name + "() takes 2 arguments");
            } else {
                for (int i = 0; i < 2; ++i) {
                    Type t = n.args[i]->type;
                    if (!t.isError() && t.kind != Type::INT) {
                        error(n.args[i]->loc,
                              name + "() requires int arguments");
                    }
                }
            }
            n.type = tyInt();
            return;
        }
        // Should not reach here.
        error(n.loc, "unhandled built-in '" + name + "'");
        n.type = tyError();
    }

    static Type tyVoid() {
        Type t;
        t.kind = Type::VOID_T;
        return t;
    }
};

} // namespace

int runSemanticAnalysis(Program &program, SymbolTable &symtab) {
    SemaPass pass(symtab);
    program.accept(pass);
    return pass.errors;
}
