#include "opt.hpp"
#include "ir.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

namespace {

bool isIntC(const Operand& o)   { return o.kind == Operand::INT_CONST; }
bool isFloatC(const Operand& o) { return o.kind == Operand::FLOAT_CONST; }
bool sameTemp(const Operand& a, const Operand& b) {
    return a.kind == Operand::TEMP && b.kind == Operand::TEMP && a.sval == b.sval;
}
bool sameNamed(const Operand& a, const Operand& b) {
    if (a.kind != b.kind) return false;
    return (a.kind == Operand::TEMP || a.kind == Operand::VAR) && a.sval == b.sval;
}

void makeCopy(Instr& i, Operand v) {
    i.op = Op::COPY;
    i.src1 = v;
    i.src2 = Operand::none();
}

// ---- Pass 1: constant folding + algebraic identities ----
bool foldPass(IRFunction& f) {
    bool changed = false;
    for (auto& i : f.code) {
        // Both int constants → fold
        if (isIntC(i.src1) && isIntC(i.src2)) {
            long long a = i.src1.ival, b = i.src2.ival;
            switch (i.op) {
                case Op::ADD: makeCopy(i, Operand::intC(a + b)); changed = true; continue;
                case Op::SUB: makeCopy(i, Operand::intC(a - b)); changed = true; continue;
                case Op::MUL: makeCopy(i, Operand::intC(a * b)); changed = true; continue;
                case Op::DIV: if (b != 0) { makeCopy(i, Operand::intC(a / b)); changed = true; } continue;
                case Op::MOD: if (b != 0) { makeCopy(i, Operand::intC(a % b)); changed = true; } continue;
                case Op::EQ:  makeCopy(i, Operand::boolC(a == b)); changed = true; continue;
                case Op::NEQ: makeCopy(i, Operand::boolC(a != b)); changed = true; continue;
                case Op::LT:  makeCopy(i, Operand::boolC(a <  b)); changed = true; continue;
                case Op::LE:  makeCopy(i, Operand::boolC(a <= b)); changed = true; continue;
                case Op::GT:  makeCopy(i, Operand::boolC(a >  b)); changed = true; continue;
                case Op::GE:  makeCopy(i, Operand::boolC(a >= b)); changed = true; continue;
                case Op::AND: makeCopy(i, Operand::boolC(a && b)); changed = true; continue;
                case Op::OR:  makeCopy(i, Operand::boolC(a || b)); changed = true; continue;
                default: break;
            }
        }
        // Float folding
        if (isFloatC(i.src1) && isFloatC(i.src2)) {
            double a = i.src1.fval, b = i.src2.fval;
            switch (i.op) {
                case Op::ADD: makeCopy(i, Operand::floatC(a + b)); changed = true; continue;
                case Op::SUB: makeCopy(i, Operand::floatC(a - b)); changed = true; continue;
                case Op::MUL: makeCopy(i, Operand::floatC(a * b)); changed = true; continue;
                case Op::DIV: if (b != 0.0) { makeCopy(i, Operand::floatC(a / b)); changed = true; } continue;
                case Op::EQ:  makeCopy(i, Operand::boolC(a == b)); changed = true; continue;
                case Op::NEQ: makeCopy(i, Operand::boolC(a != b)); changed = true; continue;
                case Op::LT:  makeCopy(i, Operand::boolC(a <  b)); changed = true; continue;
                case Op::LE:  makeCopy(i, Operand::boolC(a <= b)); changed = true; continue;
                case Op::GT:  makeCopy(i, Operand::boolC(a >  b)); changed = true; continue;
                case Op::GE:  makeCopy(i, Operand::boolC(a >= b)); changed = true; continue;
                default: break;
            }
        }
        // Unary fold
        if (i.op == Op::NEG && isIntC(i.src1)) {
            makeCopy(i, Operand::intC(-i.src1.ival)); changed = true; continue;
        }
        if (i.op == Op::NEG && isFloatC(i.src1)) {
            makeCopy(i, Operand::floatC(-i.src1.fval)); changed = true; continue;
        }
        if (i.op == Op::NOT && isIntC(i.src1)) {
            makeCopy(i, Operand::boolC(!i.src1.ival)); changed = true; continue;
        }

        // Algebraic identities (int-only — float identities are skipped to preserve IEEE semantics)
        switch (i.op) {
            case Op::ADD:
                if (isIntC(i.src1) && i.src1.ival == 0) { makeCopy(i, i.src2); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 0) { makeCopy(i, i.src1); changed = true; }
                break;
            case Op::SUB:
                if (isIntC(i.src2) && i.src2.ival == 0) { makeCopy(i, i.src1); changed = true; }
                else if (sameNamed(i.src1, i.src2)) { makeCopy(i, Operand::intC(0)); changed = true; }
                break;
            case Op::MUL:
                if ((isIntC(i.src1) && i.src1.ival == 0) || (isIntC(i.src2) && i.src2.ival == 0)) {
                    makeCopy(i, Operand::intC(0)); changed = true;
                } else if (isIntC(i.src1) && i.src1.ival == 1) { makeCopy(i, i.src2); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 1) { makeCopy(i, i.src1); changed = true; }
                break;
            case Op::DIV:
                if (isIntC(i.src2) && i.src2.ival == 1) { makeCopy(i, i.src1); changed = true; }
                break;
            case Op::AND:
                if (isIntC(i.src1) && i.src1.ival == 0) { makeCopy(i, Operand::boolC(false)); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 0) { makeCopy(i, Operand::boolC(false)); changed = true; }
                else if (isIntC(i.src1) && i.src1.ival == 1) { makeCopy(i, i.src2); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 1) { makeCopy(i, i.src1); changed = true; }
                break;
            case Op::OR:
                if (isIntC(i.src1) && i.src1.ival == 1) { makeCopy(i, Operand::boolC(true)); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 1) { makeCopy(i, Operand::boolC(true)); changed = true; }
                else if (isIntC(i.src1) && i.src1.ival == 0) { makeCopy(i, i.src2); changed = true; }
                else if (isIntC(i.src2) && i.src2.ival == 0) { makeCopy(i, i.src1); changed = true; }
                break;
            default: break;
        }
    }
    return changed;
}

// ---- Pass 2: intra-block constant propagation (temp -> const) ----
bool propPass(IRFunction& f) {
    bool changed = false;
    std::unordered_map<std::string, Operand> tempConst;

    auto invalidate = [&](const std::string& name) { tempConst.erase(name); };
    auto sub = [&](Operand& o) {
        if (o.kind == Operand::TEMP) {
            auto it = tempConst.find(o.sval);
            if (it != tempConst.end()) { o = it->second; changed = true; }
        }
    };

    for (auto& i : f.code) {
        // Reset at each label boundary (join points)
        if (i.op == Op::LABEL) { tempConst.clear(); continue; }

        // Substitute uses
        sub(i.src1);
        sub(i.src2);

        // Update map for new defs
        if (i.dst.kind == Operand::TEMP) {
            invalidate(i.dst.sval);
            if (i.op == Op::COPY && (isIntC(i.src1) || isFloatC(i.src1))) {
                tempConst[i.dst.sval] = i.src1;
            }
        }
        // CALL doesn't modify our local temps (each temp written once typically); leave map.
    }
    return changed;
}

// ---- Pass 3: dead-code elimination (delete unused pure temp defs) ----
bool dcePass(IRFunction& f) {
    std::unordered_set<std::string> used;
    for (const auto& i : f.code) {
        if (i.src1.kind == Operand::TEMP) used.insert(i.src1.sval);
        if (i.src2.kind == Operand::TEMP) used.insert(i.src2.sval);
        // STORE writes to dst[idx] = val; the array variable is in dst — but STORE has side effects, treat dst as live
    }

    auto isPure = [](Op op) {
        switch (op) {
            case Op::CALL: case Op::STORE: case Op::PARAM:
            case Op::JMP:  case Op::JZ:    case Op::JNZ:
            case Op::LABEL: case Op::RET:
                return false;
            default:
                return true;
        }
    };

    std::vector<Instr> kept;
    bool changed = false;
    for (auto& i : f.code) {
        if (isPure(i.op) && i.dst.kind == Operand::TEMP && used.count(i.dst.sval) == 0) {
            changed = true;        // drop
        } else {
            kept.push_back(i);
        }
    }
    f.code = std::move(kept);
    return changed;
}

} // namespace

void optimizeIR(IRProgram& p) {
    for (auto& f : p.funcs) {
        bool changed = true;
        for (int iter = 0; changed && iter < 50; ++iter) {
            changed = false;
            changed |= foldPass(f);
            changed |= propPass(f);
            changed |= dcePass(f);
        }
    }
}
