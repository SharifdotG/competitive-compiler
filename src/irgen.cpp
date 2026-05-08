#include "irgen.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include "symtab.hpp"
#include <cassert>
#include <cstdio>
#include <stack>
#include <string>
#include <vector>

namespace {

class IRGen : public Visitor {
  public:
    IRProgram *program = nullptr;
    IRFunction *curFunc = nullptr;

    Operand resultExpr; // set by expression visitors

    std::stack<std::string> breakStack;
    std::stack<std::string> contStack;

    // ----- helpers -----

    void emit(Op op, Operand dst, Operand a = Operand::none(),
              Operand b = Operand::none(), Loc loc = {}) {
        curFunc->code.push_back({op, dst, a, b, loc});
    }

    int addStringConst(const std::string &s) {
        program->stringConstants.push_back(s);
        return (int)program->stringConstants.size() - 1;
    }

    Operand genExpr(Expr *e) {
        e->accept(*this);
        return resultExpr;
    }

    static Operand operandOf(Symbol *s) {
        return Operand::var(s->name, s->type);
    }

    // ----- expressions -----

    void visit(IntLit &n) override { resultExpr = Operand::intC(n.val); }
    void visit(FloatLit &n) override { resultExpr = Operand::floatC(n.val); }
    void visit(BoolLit &n) override { resultExpr = Operand::boolC(n.val); }
    void visit(StringLit &n) override {
        int idx = addStringConst(n.val);
        resultExpr = Operand::strC(idx, n.val);
    }
    void visit(VarRef &n) override { resultExpr = operandOf(n.sym); }

    void visit(IndexExpr &n) override {
        Operand idx = genExpr(n.idx.get());
        Type elemT;
        elemT.kind = n.sym->type.elem;
        Operand t = curFunc->newTemp(elemT);
        emit(Op::LOAD, t, operandOf(n.sym), idx, n.loc);
        resultExpr = t;
    }

    void visit(BinOp &n) override {
        // Short-circuit && and ||
        if (n.op == BinOp::And || n.op == BinOp::Or) {
            genShortCircuit(n);
            return;
        }
        // String == / != / + via runtime
        if (n.lhs->type.kind == Type::STRING) {
            if (n.op == BinOp::Eq || n.op == BinOp::Neq) {
                Operand a = genExpr(n.lhs.get());
                Operand b = genExpr(n.rhs.get());
                emit(Op::PARAM, Operand::none(), a, Operand::none(), n.loc);
                emit(Op::PARAM, Operand::none(), b, Operand::none(), n.loc);
                Operand t = curFunc->newTemp(tyBool());
                emit(Op::CALL, t, Operand::func("__rt_streq"), Operand::intC(2),
                     n.loc);
                if (n.op == BinOp::Neq) {
                    Operand t2 = curFunc->newTemp(tyBool());
                    emit(Op::NOT, t2, t, Operand::none(), n.loc);
                    resultExpr = t2;
                } else {
                    resultExpr = t;
                }
                return;
            }
            if (n.op == BinOp::Add) {
                Operand t = curFunc->newTemp(
                    tyStr()); // codegen allocates 256-byte buffer
                Operand addrT = curFunc->newTemp(tyInt());
                emit(Op::ADDR, addrT, t, Operand::none(), n.loc);
                Operand a = genExpr(n.lhs.get());
                Operand b = genExpr(n.rhs.get());
                emit(Op::PARAM, Operand::none(), addrT, Operand::none(), n.loc);
                emit(Op::PARAM, Operand::none(), a, Operand::none(), n.loc);
                emit(Op::PARAM, Operand::none(), b, Operand::none(), n.loc);
                emit(Op::CALL, Operand::none(), Operand::func("__rt_strconcat"),
                     Operand::intC(3), n.loc);
                resultExpr = t;
                return;
            }
        }
        Operand l = genExpr(n.lhs.get());
        Operand r = genExpr(n.rhs.get());
        Op op;
        switch (n.op) {
        case BinOp::Add:
            op = Op::ADD;
            break;
        case BinOp::Sub:
            op = Op::SUB;
            break;
        case BinOp::Mul:
            op = Op::MUL;
            break;
        case BinOp::Div:
            op = Op::DIV;
            break;
        case BinOp::Mod:
            op = Op::MOD;
            break;
        case BinOp::Eq:
            op = Op::EQ;
            break;
        case BinOp::Neq:
            op = Op::NEQ;
            break;
        case BinOp::Lt:
            op = Op::LT;
            break;
        case BinOp::Le:
            op = Op::LE;
            break;
        case BinOp::Gt:
            op = Op::GT;
            break;
        case BinOp::Ge:
            op = Op::GE;
            break;
        default:
            op = Op::ADD;
            break; // unreachable
        }
        Operand t = curFunc->newTemp(n.type);
        emit(op, t, l, r, n.loc);
        resultExpr = t;
    }

    void genShortCircuit(BinOp &n) {
        // For &&: result = lhs ? rhs : false
        // For ||: result = lhs ? true  : rhs
        Operand t = curFunc->newTemp(tyBool());
        Operand l = genExpr(n.lhs.get());
        std::string Lend = curFunc->newLabel("Lsc");
        if (n.op == BinOp::And) {
            // t = l; if (!t) goto end; t = rhs; end:
            emit(Op::COPY, t, l, Operand::none(), n.loc);
            emit(Op::JZ, Operand::none(), t, Operand::label(Lend), n.loc);
            Operand r = genExpr(n.rhs.get());
            emit(Op::COPY, t, r, Operand::none(), n.loc);
            emit(Op::LABEL, Operand::none(), Operand::label(Lend),
                 Operand::none(), n.loc);
        } else {
            // t = l; if (t) goto end; t = rhs; end:
            emit(Op::COPY, t, l, Operand::none(), n.loc);
            emit(Op::JNZ, Operand::none(), t, Operand::label(Lend), n.loc);
            Operand r = genExpr(n.rhs.get());
            emit(Op::COPY, t, r, Operand::none(), n.loc);
            emit(Op::LABEL, Operand::none(), Operand::label(Lend),
                 Operand::none(), n.loc);
        }
        resultExpr = t;
    }

    void visit(UnOp &n) override {
        Operand x = genExpr(n.operand.get());
        Operand t = curFunc->newTemp(n.type);
        emit(n.op == UnOp::Neg ? Op::NEG : Op::NOT, t, x, Operand::none(),
             n.loc);
        resultExpr = t;
    }

    void visit(CallExpr &n) override {
        if (n.isBuiltin) {
            genBuiltinCall(n);
            return;
        }
        // Plain user function: pass each arg by value, then CALL.
        std::vector<Operand> args;
        args.reserve(n.args.size());
        for (auto &a : n.args)
            args.push_back(genExpr(a.get()));
        for (auto &a : args)
            emit(Op::PARAM, Operand::none(), a, Operand::none(), n.loc);
        Operand dst = Operand::none();
        if (n.type.kind != Type::VOID_T && !n.type.isError()) {
            dst = curFunc->newTemp(n.type);
        }
        emit(Op::CALL, dst, Operand::func(n.callee),
             Operand::intC((long long)args.size()), n.loc);
        resultExpr = dst;
    }

    // ----- built-in dispatch -----

    void genBuiltinCall(CallExpr &n) {
        const std::string &name = n.callee;

        // len(arr) is a compile-time constant for static arrays
        if (name == "len" && !n.args.empty() &&
            n.args[0]->type.kind == Type::ARRAY) {
            resultExpr = Operand::intC(n.args[0]->type.arrayLen);
            return;
        }

        // read(int_or_float_var) — runtime returns the value, then we copy into
        // the variable
        if (name == "read" && !n.args.empty()) {
            Type t = n.args[0]->type;
            if (t.kind == Type::INT || t.kind == Type::FLOAT) {
                std::string rt =
                    (t.kind == Type::INT) ? "__rt_read_int" : "__rt_read_float";
                Operand val = curFunc->newTemp(t);
                emit(Op::CALL, val, Operand::func(rt), Operand::intC(0), n.loc);
                storeIntoLValue(n.args[0].get(), val, n.loc);
                resultExpr = Operand::none();
                return;
            }
            // string read: pass buffer (handled below as by-ref)
        }

        // Determine which arguments need ADDR (by-ref).
        std::vector<bool> byRef(n.args.size(), false);
        if (name == "swap") {
            byRef[0] = byRef[1] = true;
        } else if (name == "sort" || name == "reverse")
            byRef[0] = true;
        else if (name == "binary_search")
            byRef[0] = true;
        else if (name == "read")
            byRef[0] = true; // string buffer

        std::vector<Operand> args;
        for (size_t i = 0; i < n.args.size(); ++i) {
            if (byRef[i]) {
                args.push_back(genAddrOf(n.args[i].get(), n.loc));
            } else {
                args.push_back(genExpr(n.args[i].get()));
            }
        }

        std::string rt = mangleBuiltin(name, n.args);
        n.builtinResolved = rt;

        for (auto &a : args)
            emit(Op::PARAM, Operand::none(), a, Operand::none(), n.loc);

        Operand dst = Operand::none();
        if (n.type.kind != Type::VOID_T && !n.type.isError()) {
            dst = curFunc->newTemp(n.type);
        }
        emit(Op::CALL, dst, Operand::func(rt),
             Operand::intC((long long)args.size()), n.loc);
        resultExpr = dst;
    }

    Operand genAddrOf(Expr *e, Loc loc) {
        Operand addr = curFunc->newTemp(
            tyInt()); // pointer = 8 bytes; encode as int operand
        if (auto *vr = dynamic_cast<VarRef *>(e)) {
            emit(Op::ADDR, addr, operandOf(vr->sym), Operand::none(), loc);
        } else if (auto *ix = dynamic_cast<IndexExpr *>(e)) {
            // &arr[idx] — emit base+idx*8 via ADDR + offset; codegen lowers via
            // lea. For simplicity here: spill arr base into a temp, then
            // ADDR-with-offset is a codegen detail. We model this as: ADDR
            // base; then ADD addr, addr, idx*sizeof(elem)
            emit(Op::ADDR, addr, operandOf(ix->sym), Operand::none(), loc);
            Operand idx = genExpr(ix->idx.get());
            // Multiply idx by element size (assume 8 bytes for v1)
            Operand scaled = curFunc->newTemp(tyInt());
            emit(Op::MUL, scaled, idx, Operand::intC(8), loc);
            Operand summed = curFunc->newTemp(tyInt());
            emit(Op::ADD, summed, addr, scaled, loc);
            return summed;
        } else {
            fprintf(stderr, "irgen: address-of an unsupported expression\n");
        }
        return addr;
    }

    void storeIntoLValue(Expr *lvalue, Operand value, Loc loc) {
        if (auto *vr = dynamic_cast<VarRef *>(lvalue)) {
            emit(Op::COPY, operandOf(vr->sym), value, Operand::none(), loc);
        } else if (auto *ix = dynamic_cast<IndexExpr *>(lvalue)) {
            Operand idx = genExpr(ix->idx.get());
            emit(Op::STORE, operandOf(ix->sym), idx, value, loc);
        }
    }

    static std::string
    mangleBuiltin(const std::string &name,
                  const std::vector<std::unique_ptr<Expr>> &args) {
        if (name == "print") {
            Type t = args[0]->type;
            if (t.kind == Type::INT)
                return "__rt_print_int";
            if (t.kind == Type::FLOAT)
                return "__rt_print_float";
            if (t.kind == Type::STRING)
                return "__rt_print_str";
            if (t.kind == Type::BOOL)
                return "__rt_print_bool";
        }
        if (name == "read") {
            // int/float handled before this; remaining = string
            return "__rt_read_str";
        }
        if (name == "sort")
            return "__rt_sort_int";
        if (name == "reverse")
            return "__rt_reverse_int";
        if (name == "binary_search")
            return "__rt_binary_search_int";
        if (name == "gcd")
            return "__rt_gcd";
        if (name == "lcm")
            return "__rt_lcm";
        if (name == "max" || name == "min" || name == "abs") {
            Type t = args[0]->type;
            const char *suf = (t.kind == Type::INT) ? "int" : "float";
            return "__rt_" + name + "_" + suf;
        }
        if (name == "swap")
            return "__rt_swap_i64";
        if (name == "len")
            return "__rt_strlen"; // string only by this point
        return name;
    }

    // ----- statements -----

    void visit(Block &n) override {
        for (auto &s : n.stmts)
            s->accept(*this);
    }

    void visit(DeclStmt &n) override {
        if (n.sym)
            curFunc->locals.push_back(n.sym);
        if (n.init) {
            Operand v = genExpr(n.init.get());
            emit(Op::COPY, operandOf(n.sym), v, Operand::none(), n.loc);
        }
    }

    void visit(AssignStmt &n) override {
        if (n.op == AssignStmt::Assign) {
            Operand v = genExpr(n.value.get());
            storeIntoLValue(n.target.get(), v, n.loc);
        } else {
            // x op= v   =>   x = x op v
            Op op = (n.op == AssignStmt::PlusEq) ? Op::ADD : Op::SUB;
            Operand cur = genExpr(n.target.get()); // load current value
            Operand rhs = genExpr(n.value.get());
            Operand t = curFunc->newTemp(n.target->type);
            emit(op, t, cur, rhs, n.loc);
            storeIntoLValue(n.target.get(), t, n.loc);
        }
    }

    void visit(IfStmt &n) override {
        Operand cond = genExpr(n.cond.get());
        if (n.elseS) {
            std::string Lelse = curFunc->newLabel("Lelse");
            std::string Lend = curFunc->newLabel("Lend");
            emit(Op::JZ, Operand::none(), cond, Operand::label(Lelse), n.loc);
            n.thenS->accept(*this);
            emit(Op::JMP, Operand::none(), Operand::label(Lend),
                 Operand::none(), n.loc);
            emit(Op::LABEL, Operand::none(), Operand::label(Lelse),
                 Operand::none(), n.loc);
            n.elseS->accept(*this);
            emit(Op::LABEL, Operand::none(), Operand::label(Lend),
                 Operand::none(), n.loc);
        } else {
            std::string Lend = curFunc->newLabel("Lend");
            emit(Op::JZ, Operand::none(), cond, Operand::label(Lend), n.loc);
            n.thenS->accept(*this);
            emit(Op::LABEL, Operand::none(), Operand::label(Lend),
                 Operand::none(), n.loc);
        }
    }

    void visit(WhileStmt &n) override {
        std::string Ltop = curFunc->newLabel("Ltop");
        std::string Lend = curFunc->newLabel("Lend");
        breakStack.push(Lend);
        contStack.push(Ltop);
        emit(Op::LABEL, Operand::none(), Operand::label(Ltop), Operand::none(),
             n.loc);
        Operand cond = genExpr(n.cond.get());
        emit(Op::JZ, Operand::none(), cond, Operand::label(Lend), n.loc);
        n.body->accept(*this);
        emit(Op::JMP, Operand::none(), Operand::label(Ltop), Operand::none(),
             n.loc);
        emit(Op::LABEL, Operand::none(), Operand::label(Lend), Operand::none(),
             n.loc);
        breakStack.pop();
        contStack.pop();
    }

    void visit(ForStmt &n) override {
        if (n.init)
            n.init->accept(*this);
        std::string Ltop = curFunc->newLabel("Ltop");
        std::string Lstep = curFunc->newLabel("Lstep");
        std::string Lend = curFunc->newLabel("Lend");
        breakStack.push(Lend);
        contStack.push(Lstep);
        emit(Op::LABEL, Operand::none(), Operand::label(Ltop), Operand::none(),
             n.loc);
        Operand cond = genExpr(n.cond.get());
        emit(Op::JZ, Operand::none(), cond, Operand::label(Lend), n.loc);
        n.body->accept(*this);
        emit(Op::LABEL, Operand::none(), Operand::label(Lstep), Operand::none(),
             n.loc);
        n.step->accept(*this);
        emit(Op::JMP, Operand::none(), Operand::label(Ltop), Operand::none(),
             n.loc);
        emit(Op::LABEL, Operand::none(), Operand::label(Lend), Operand::none(),
             n.loc);
        breakStack.pop();
        contStack.pop();
    }

    void visit(ReturnStmt &n) override {
        if (n.value) {
            Operand v = genExpr(n.value.get());
            emit(Op::RET, Operand::none(), v, Operand::none(), n.loc);
        } else {
            emit(Op::RET, Operand::none(), Operand::none(), Operand::none(),
                 n.loc);
        }
    }

    void visit(ExprStmt &n) override {
        // Discard result.
        (void)genExpr(n.expr.get());
    }

    void visit(BreakStmt &n) override {
        if (breakStack.empty())
            return;
        emit(Op::JMP, Operand::none(), Operand::label(breakStack.top()),
             Operand::none(), n.loc);
    }
    void visit(ContinueStmt &n) override {
        if (contStack.empty())
            return;
        emit(Op::JMP, Operand::none(), Operand::label(contStack.top()),
             Operand::none(), n.loc);
    }

    void visit(FuncDecl &n) override {
        IRFunction f;
        f.name = n.name;
        f.returnType = n.returnType;
        for (auto &p : n.params) {
            f.paramTypes.push_back(p.type);
            f.paramNames.push_back(p.name);
            f.paramSyms.push_back(p.sym);
        }
        program->funcs.push_back(std::move(f));
        curFunc = &program->funcs.back();

        if (n.body)
            n.body->accept(*this);

        // Ensure void functions end with ret
        if (n.returnType.kind == Type::VOID_T) {
            emit(Op::RET, Operand::none(), Operand::none(), Operand::none(),
                 n.loc);
        }
        curFunc = nullptr;
    }

    void visit(Program &n) override {
        for (auto &f : n.funcs)
            f->accept(*this);
    }
};

} // namespace

IRProgram *generateIR(Program &ast) {
    IRGen g;
    g.program = new IRProgram();
    ast.accept(g);
    return g.program;
}
