#include "ast.hpp"
#include <cstdio>

// ---------- accept dispatchers ----------

void IntLit::accept(Visitor &v) { v.visit(*this); }
void FloatLit::accept(Visitor &v) { v.visit(*this); }
void StringLit::accept(Visitor &v) { v.visit(*this); }
void BoolLit::accept(Visitor &v) { v.visit(*this); }
void VarRef::accept(Visitor &v) { v.visit(*this); }
void IndexExpr::accept(Visitor &v) { v.visit(*this); }
void CallExpr::accept(Visitor &v) { v.visit(*this); }
void BinOp::accept(Visitor &v) { v.visit(*this); }
void UnOp::accept(Visitor &v) { v.visit(*this); }
void Block::accept(Visitor &v) { v.visit(*this); }
void DeclStmt::accept(Visitor &v) { v.visit(*this); }
void AssignStmt::accept(Visitor &v) { v.visit(*this); }
void IfStmt::accept(Visitor &v) { v.visit(*this); }
void WhileStmt::accept(Visitor &v) { v.visit(*this); }
void ForStmt::accept(Visitor &v) { v.visit(*this); }
void ReturnStmt::accept(Visitor &v) { v.visit(*this); }
void ExprStmt::accept(Visitor &v) { v.visit(*this); }
void BreakStmt::accept(Visitor &v) { v.visit(*this); }
void ContinueStmt::accept(Visitor &v) { v.visit(*this); }
void FuncDecl::accept(Visitor &v) { v.visit(*this); }
void Program::accept(Visitor &v) { v.visit(*this); }

const char *typeName(const Type &t) {
    switch (t.kind) {
    case Type::INT:
        return "int";
    case Type::FLOAT:
        return "float";
    case Type::STRING:
        return "string";
    case Type::BOOL:
        return "bool";
    case Type::VOID_T:
        return "void";
    case Type::ERROR_T:
        return "<error>";
    case Type::ARRAY: {
        static thread_local char buf[64];
        const char *en = "?";
        switch (t.elem) {
        case Type::INT:
            en = "int";
            break;
        case Type::FLOAT:
            en = "float";
            break;
        case Type::STRING:
            en = "string";
            break;
        case Type::BOOL:
            en = "bool";
            break;
        default:
            break;
        }
        snprintf(buf, sizeof(buf), "%s[%d]", en, t.arrayLen);
        return buf;
    }
    }
    return "?";
}

// ---------- AST printer ----------

namespace {
struct AstPrinter : Visitor {
    FILE *out;
    int indent = 0;

    explicit AstPrinter(FILE *f) : out(f) {}

    void pad() {
        for (int i = 0; i < indent; ++i)
            fputs("  ", out);
    }

    void child(Node *n) {
        if (!n) {
            pad();
            fputs("<null>\n", out);
            return;
        }
        n->accept(*this);
    }

    void visit(IntLit &n) override {
        pad();
        fprintf(out, "IntLit %lld\n", n.val);
    }
    void visit(FloatLit &n) override {
        pad();
        fprintf(out, "FloatLit %g\n", n.val);
    }
    void visit(StringLit &n) override {
        pad();
        fprintf(out, "StringLit \"");
        for (char c : n.val) {
            switch (c) {
            case '\n':
                fputs("\\n", out);
                break;
            case '\t':
                fputs("\\t", out);
                break;
            case '\\':
                fputs("\\\\", out);
                break;
            case '"':
                fputs("\\\"", out);
                break;
            default:
                fputc(c, out);
                break;
            }
        }
        fputs("\"\n", out);
    }
    void visit(BoolLit &n) override {
        pad();
        fprintf(out, "BoolLit %s\n", n.val ? "true" : "false");
    }
    void visit(VarRef &n) override {
        pad();
        fprintf(out, "VarRef %s\n", n.name.c_str());
    }
    void visit(IndexExpr &n) override {
        pad();
        fprintf(out, "IndexExpr %s\n", n.name.c_str());
        ++indent;
        child(n.idx.get());
        --indent;
    }
    void visit(CallExpr &n) override {
        pad();
        fprintf(out, "CallExpr %s\n", n.callee.c_str());
        ++indent;
        for (auto &a : n.args)
            child(a.get());
        --indent;
    }
    void visit(BinOp &n) override {
        static const char *names[] = {"+", "-",  "*", "/",  "%",  "==", "!=",
                                      "<", "<=", ">", ">=", "&&", "||"};
        pad();
        fprintf(out, "BinOp %s\n", names[n.op]);
        ++indent;
        child(n.lhs.get());
        child(n.rhs.get());
        --indent;
    }
    void visit(UnOp &n) override {
        const char *name = (n.op == UnOp::Neg) ? "-" : "!";
        pad();
        fprintf(out, "UnOp %s\n", name);
        ++indent;
        child(n.operand.get());
        --indent;
    }
    void visit(Block &n) override {
        pad();
        fprintf(out, "Block\n");
        ++indent;
        for (auto &s : n.stmts)
            child(s.get());
        --indent;
    }
    void visit(DeclStmt &n) override {
        pad();
        if (n.isArray)
            fprintf(out, "DeclStmt %s : %s[%d]\n", n.name.c_str(),
                    n.declaredType.elem == Type::INT      ? "int"
                    : n.declaredType.elem == Type::FLOAT  ? "float"
                    : n.declaredType.elem == Type::STRING ? "string"
                    : n.declaredType.elem == Type::BOOL   ? "bool"
                                                          : "?",
                    n.arrayLen);
        else
            fprintf(out, "DeclStmt %s : %s\n", n.name.c_str(),
                    typeName(n.declaredType));
        if (n.init) {
            ++indent;
            child(n.init.get());
            --indent;
        }
    }
    void visit(AssignStmt &n) override {
        const char *op = n.op == AssignStmt::Assign   ? "="
                         : n.op == AssignStmt::PlusEq ? "+="
                                                      : "-=";
        pad();
        fprintf(out, "AssignStmt %s\n", op);
        ++indent;
        child(n.target.get());
        child(n.value.get());
        --indent;
    }
    void visit(IfStmt &n) override {
        pad();
        fprintf(out, "IfStmt\n");
        ++indent;
        pad();
        fprintf(out, "cond:\n");
        ++indent;
        child(n.cond.get());
        --indent;
        pad();
        fprintf(out, "then:\n");
        ++indent;
        child(n.thenS.get());
        --indent;
        if (n.elseS) {
            pad();
            fprintf(out, "else:\n");
            ++indent;
            child(n.elseS.get());
            --indent;
        }
        --indent;
    }
    void visit(WhileStmt &n) override {
        pad();
        fprintf(out, "WhileStmt\n");
        ++indent;
        pad();
        fprintf(out, "cond:\n");
        ++indent;
        child(n.cond.get());
        --indent;
        pad();
        fprintf(out, "body:\n");
        ++indent;
        child(n.body.get());
        --indent;
        --indent;
    }
    void visit(ForStmt &n) override {
        pad();
        fprintf(out, "ForStmt\n");
        ++indent;
        pad();
        fprintf(out, "init:\n");
        ++indent;
        if (n.init)
            child(n.init.get());
        else {
            pad();
            fputs("<empty>\n", out);
        }
        --indent;
        pad();
        fprintf(out, "cond:\n");
        ++indent;
        child(n.cond.get());
        --indent;
        pad();
        fprintf(out, "step:\n");
        ++indent;
        child(n.step.get());
        --indent;
        pad();
        fprintf(out, "body:\n");
        ++indent;
        child(n.body.get());
        --indent;
        --indent;
    }
    void visit(ReturnStmt &n) override {
        pad();
        fputs("ReturnStmt\n", out);
        if (n.value) {
            ++indent;
            child(n.value.get());
            --indent;
        }
    }
    void visit(ExprStmt &n) override {
        pad();
        fputs("ExprStmt\n", out);
        ++indent;
        child(n.expr.get());
        --indent;
    }
    void visit(BreakStmt &) override {
        pad();
        fputs("BreakStmt\n", out);
    }
    void visit(ContinueStmt &) override {
        pad();
        fputs("ContinueStmt\n", out);
    }
    void visit(FuncDecl &n) override {
        pad();
        fprintf(out, "FuncDecl %s -> %s\n", n.name.c_str(),
                typeName(n.returnType));
        ++indent;
        for (auto &p : n.params) {
            pad();
            fprintf(out, "Param %s : %s\n", p.name.c_str(), typeName(p.type));
        }
        if (n.body)
            child(n.body.get());
        --indent;
    }
    void visit(Program &n) override {
        pad();
        fputs("Program\n", out);
        ++indent;
        for (auto &f : n.funcs)
            child(f.get());
        --indent;
    }
};
} // namespace

void printAst(Program &p, FILE *out) {
    AstPrinter pr(out);
    p.accept(pr);
}
