#include "codegen.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include "symtab.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

const char *INT_REGS[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
const char *FLOAT_REGS[8] = {"xmm0", "xmm1", "xmm2", "xmm3",
                             "xmm4", "xmm5", "xmm6", "xmm7"};

bool isExternalSymbol(const std::string &name) {
    if (name.compare(0, 5, "__rt_") == 0)
        return true;
    static const std::unordered_set<std::string> libc = {
        "printf", "scanf",  "strcpy", "strcat", "strcmp",
        "strlen", "malloc", "free",   "qsort",  "puts"};
    return libc.count(name) > 0;
}

std::string offStr(int off) {
    char buf[24];
    if (off < 0)
        snprintf(buf, sizeof(buf), " - %d", -off);
    else
        snprintf(buf, sizeof(buf), " + %d", off);
    return buf;
}

struct FrameInfo {
    std::unordered_map<std::string, int>
        offsets; // VAR/TEMP name -> offset from rbp
    std::unordered_map<std::string, Type> tempTypes; // TEMP name -> type
    int frameSize = 0;
};

class Emitter {
  public:
    FILE *out;
    std::vector<double> floatPool;
    std::unordered_map<uint64_t, int> floatPoolIndex;

    void run(const IRProgram &p) {
        emitHeader();
        collectFloatConstants(p);
        emitConstantsSection(p);
        emitTextSection();
        for (const auto &f : p.funcs)
            emitFunction(f);
    }

  private:
    void emitHeader() {
        fprintf(out, ".intel_syntax noprefix\n");
        fprintf(out, ".section .note.GNU-stack,\"\",@progbits\n");
    }

    void collectFloatConstants(const IRProgram &p) {
        auto add = [&](const Operand &o) {
            if (o.kind != Operand::FLOAT_CONST)
                return;
            uint64_t bits;
            std::memcpy(&bits, &o.fval, 8);
            if (floatPoolIndex.count(bits))
                return;
            floatPoolIndex[bits] = (int)floatPool.size();
            floatPool.push_back(o.fval);
        };
        for (auto &f : p.funcs)
            for (auto &i : f.code) {
                add(i.dst);
                add(i.src1);
                add(i.src2);
            }
    }

    int floatPoolIdx(double v) const {
        uint64_t bits;
        std::memcpy(&bits, &v, 8);
        auto it = floatPoolIndex.find(bits);
        return it == floatPoolIndex.end() ? -1 : it->second;
    }

    void emitConstantsSection(const IRProgram &p) {
        if (p.stringConstants.empty() && floatPool.empty())
            return;
        fprintf(out, ".section .rodata\n");
        for (size_t i = 0; i < p.stringConstants.size(); ++i) {
            fprintf(out, ".LC%zu:\n    .asciz \"", i);
            for (char c : p.stringConstants[i]) {
                switch (c) {
                case '\n':
                    fputs("\\n", out);
                    break;
                case '\t':
                    fputs("\\t", out);
                    break;
                case '\r':
                    fputs("\\r", out);
                    break;
                case '\\':
                    fputs("\\\\", out);
                    break;
                case '"':
                    fputs("\\\"", out);
                    break;
                default:
                    if ((unsigned char)c < 32)
                        fprintf(out, "\\%03o", (unsigned char)c);
                    else
                        fputc(c, out);
                    break;
                }
            }
            fputs("\"\n", out);
        }
        for (size_t i = 0; i < floatPool.size(); ++i) {
            uint64_t bits;
            std::memcpy(&bits, &floatPool[i], 8);
            fprintf(out, ".LCF%zu:\n    .quad 0x%016lx   # %g\n", i,
                    (unsigned long)bits, floatPool[i]);
        }
    }

    void emitTextSection() { fprintf(out, ".text\n"); }

    // --------- Per-function emission ---------

    void emitFunction(const IRFunction &f) {
        FrameInfo fi;
        layoutFrame(f, fi);

        fprintf(out, ".globl %s\n", f.name.c_str());
        fprintf(out, "%s:\n", f.name.c_str());
        fprintf(out, "    push rbp\n");
        fprintf(out, "    mov rbp, rsp\n");
        if (fi.frameSize > 0)
            fprintf(out, "    sub rsp, %d\n", fi.frameSize);

        // Spill register-passed params into their stack slots
        int regI = 0, regF = 0;
        for (Symbol *p : f.paramSyms) {
            int off = fi.offsets[p->name];
            if (off >= 0)
                continue; // came on the stack; already in place
            if (p->type.kind == Type::FLOAT) {
                fprintf(out, "    movsd qword ptr [rbp%s], %s\n",
                        offStr(off).c_str(), FLOAT_REGS[regF]);
                ++regF;
            } else {
                fprintf(out, "    mov [rbp%s], %s\n", offStr(off).c_str(),
                        INT_REGS[regI]);
                ++regI;
            }
        }

        // Lower instructions
        std::vector<Operand> pendingParams;
        for (size_t k = 0; k < f.code.size(); ++k) {
            const Instr &i = f.code[k];
            if (i.op == Op::PARAM) {
                pendingParams.push_back(i.src1);
                continue;
            }
            if (i.op == Op::CALL) {
                lowerCall(i, pendingParams, fi);
                pendingParams.clear();
                continue;
            }
            lowerInstr(i, fi);
        }

        // Safety: if last instruction wasn't RET, append epilogue
        if (f.code.empty() || f.code.back().op != Op::RET) {
            fprintf(out, "    xor eax, eax\n");
            fprintf(out, "    mov rsp, rbp\n");
            fprintf(out, "    pop rbp\n");
            fprintf(out, "    ret\n");
        }
        fprintf(out, "\n");
    }

    void layoutFrame(const IRFunction &f, FrameInfo &fi) {
        int off = 0;
        auto alloc = [&](int bytes) -> int {
            off -= bytes;
            return off;
        };

        // Params: first 6 ints in rdi..r9, first 8 floats in xmm0..7, rest on
        // stack at [rbp+16+...]
        int regI = 0, regF = 0;
        int stackParam = 16;
        for (Symbol *p : f.paramSyms) {
            int slot;
            if (p->type.kind == Type::FLOAT) {
                if (regF < 8) {
                    slot = alloc(8);
                    ++regF;
                } else {
                    slot = stackParam;
                    stackParam += 8;
                }
            } else {
                if (regI < 6) {
                    slot = alloc(8);
                    ++regI;
                } else {
                    slot = stackParam;
                    stackParam += 8;
                }
            }
            fi.offsets[p->name] = slot;
        }

        // Locals
        for (Symbol *l : f.locals) {
            int sz = 8;
            if (l->type.kind == Type::ARRAY)
                sz = 8 * l->type.arrayLen;
            else if (l->type.kind == Type::STRING)
                sz = 256;
            fi.offsets[l->name] = alloc(sz);
        }

        // Temps
        for (auto &i : f.code) {
            auto note = [&](const Operand &o) {
                if (o.kind != Operand::TEMP)
                    return;
                if (fi.offsets.count(o.sval))
                    return;
                int sz = 8;
                if (o.type.kind == Type::STRING)
                    sz = 256;
                fi.offsets[o.sval] = alloc(sz);
                fi.tempTypes[o.sval] = o.type;
            };
            note(i.dst);
            note(i.src1);
            note(i.src2);
        }

        int total = -off;
        if (total % 16 != 0)
            total += 16 - (total % 16);
        fi.frameSize = total;
    }

    // --------- Operand load / store ---------

    // VAR/TEMP with kind STRING/ARRAY: those slots HOLD the buffer inline, so
    // loading the operand means loading its address (lea). Sema forbids
    // string/array params, so this rule applies uniformly.
    bool varIsByAddress(const Operand &o) {
        if (o.kind != Operand::VAR && o.kind != Operand::TEMP)
            return false;
        return o.type.kind == Type::ARRAY || o.type.kind == Type::STRING;
    }

    // Load operand into an int-class register (rax/rcx/rdx, or any of
    // INT_REGS).
    void loadInt(const Operand &o, const char *reg, FrameInfo &fi) {
        switch (o.kind) {
        case Operand::INT_CONST:
            fprintf(out, "    mov %s, %lld\n", reg, o.ival);
            return;
        case Operand::TEMP:
        case Operand::VAR: {
            int off = fi.offsets[o.sval];
            if (varIsByAddress(o)) {
                fprintf(out, "    lea %s, [rbp%s]\n", reg, offStr(off).c_str());
            } else {
                fprintf(out, "    mov %s, [rbp%s]\n", reg, offStr(off).c_str());
            }
            return;
        }
        case Operand::STR_CONST:
            fprintf(out, "    lea %s, [rip + .LC%lld]\n", reg, o.ival);
            return;
        case Operand::FLOAT_CONST: {
            int idx = floatPoolIdx(o.fval);
            fprintf(out, "    mov %s, [rip + .LCF%d]\n", reg, idx);
            return;
        }
        case Operand::FUNC_REF:
        case Operand::LABEL_REF:
        case Operand::NONE:
            return;
        }
    }

    // Load operand into an xmm register.
    void loadFloat(const Operand &o, const char *xmm, FrameInfo &fi) {
        switch (o.kind) {
        case Operand::TEMP:
        case Operand::VAR:
            fprintf(out, "    movsd %s, qword ptr [rbp%s]\n", xmm,
                    offStr(fi.offsets[o.sval]).c_str());
            return;
        case Operand::FLOAT_CONST: {
            int idx = floatPoolIdx(o.fval);
            fprintf(out, "    movsd %s, qword ptr [rip + .LCF%d]\n", xmm, idx);
            return;
        }
        case Operand::INT_CONST:
            // Materialize via memory? Rare; convert on the fly.
            fprintf(out, "    mov rax, %lld\n", o.ival);
            fprintf(out, "    cvtsi2sd %s, rax\n", xmm);
            return;
        default:
            return;
        }
    }

    void storeInt(const Operand &o, const char *reg, FrameInfo &fi) {
        if (o.kind != Operand::TEMP && o.kind != Operand::VAR)
            return;
        fprintf(out, "    mov [rbp%s], %s\n",
                offStr(fi.offsets[o.sval]).c_str(), reg);
    }
    void storeFloat(const Operand &o, const char *xmm, FrameInfo &fi) {
        if (o.kind != Operand::TEMP && o.kind != Operand::VAR)
            return;
        fprintf(out, "    movsd qword ptr [rbp%s], %s\n",
                offStr(fi.offsets[o.sval]).c_str(), xmm);
    }

    // --------- Lowering ---------

    bool isFloatOp(const Instr &i) {
        return i.dst.type.kind == Type::FLOAT ||
               i.src1.type.kind == Type::FLOAT ||
               i.src2.type.kind == Type::FLOAT;
    }

    void lowerInstr(const Instr &i, FrameInfo &fi) {
        switch (i.op) {
        case Op::ADD:
        case Op::SUB:
        case Op::MUL:
        case Op::DIV:
        case Op::MOD: {
            if (isFloatOp(i)) {
                loadFloat(i.src1, "xmm0", fi);
                loadFloat(i.src2, "xmm1", fi);
                const char *op = "addsd";
                if (i.op == Op::SUB)
                    op = "subsd";
                else if (i.op == Op::MUL)
                    op = "mulsd";
                else if (i.op == Op::DIV)
                    op = "divsd";
                else if (i.op ==
                         Op::MOD) { /* unsupported for floats; sema disallows */
                    return;
                }
                fprintf(out, "    %s xmm0, xmm1\n", op);
                storeFloat(i.dst, "xmm0", fi);
                return;
            }
            loadInt(i.src1, "rax", fi);
            loadInt(i.src2, "rcx", fi);
            switch (i.op) {
            case Op::ADD:
                fprintf(out, "    add rax, rcx\n");
                break;
            case Op::SUB:
                fprintf(out, "    sub rax, rcx\n");
                break;
            case Op::MUL:
                fprintf(out, "    imul rax, rcx\n");
                break;
            case Op::DIV:
                fprintf(out, "    cqo\n    idiv rcx\n");
                break;
            case Op::MOD:
                fprintf(out, "    cqo\n    idiv rcx\n    mov rax, rdx\n");
                break;
            default:
                break;
            }
            storeInt(i.dst, "rax", fi);
            return;
        }
        case Op::EQ:
        case Op::NEQ:
        case Op::LT:
        case Op::LE:
        case Op::GT:
        case Op::GE: {
            if (isFloatOp(i)) {
                loadFloat(i.src1, "xmm0", fi);
                loadFloat(i.src2, "xmm1", fi);
                fprintf(out, "    ucomisd xmm0, xmm1\n");
            } else {
                loadInt(i.src1, "rax", fi);
                loadInt(i.src2, "rcx", fi);
                fprintf(out, "    cmp rax, rcx\n");
            }
            const char *setOp = "sete";
            bool isFloat = isFloatOp(i);
            switch (i.op) {
            case Op::EQ:
                setOp = "sete";
                break;
            case Op::NEQ:
                setOp = "setne";
                break;
            case Op::LT:
                setOp = isFloat ? "setb" : "setl";
                break;
            case Op::LE:
                setOp = isFloat ? "setbe" : "setle";
                break;
            case Op::GT:
                setOp = isFloat ? "seta" : "setg";
                break;
            case Op::GE:
                setOp = isFloat ? "setae" : "setge";
                break;
            default:
                break;
            }
            fprintf(out, "    %s al\n    movzx rax, al\n", setOp);
            storeInt(i.dst, "rax", fi);
            return;
        }
        case Op::AND:
            loadInt(i.src1, "rax", fi);
            loadInt(i.src2, "rcx", fi);
            fprintf(out, "    and rax, rcx\n");
            storeInt(i.dst, "rax", fi);
            return;
        case Op::OR:
            loadInt(i.src1, "rax", fi);
            loadInt(i.src2, "rcx", fi);
            fprintf(out, "    or rax, rcx\n");
            storeInt(i.dst, "rax", fi);
            return;
        case Op::NEG:
            if (i.dst.type.kind == Type::FLOAT) {
                loadFloat(i.src1, "xmm1", fi);
                fprintf(out, "    xorpd xmm0, xmm0\n    subsd xmm0, xmm1\n");
                storeFloat(i.dst, "xmm0", fi);
            } else {
                loadInt(i.src1, "rax", fi);
                fprintf(out, "    neg rax\n");
                storeInt(i.dst, "rax", fi);
            }
            return;
        case Op::NOT:
            loadInt(i.src1, "rax", fi);
            fprintf(out, "    test rax, rax\n    sete al\n    movzx rax, al\n");
            storeInt(i.dst, "rax", fi);
            return;
        case Op::COPY:
            if (i.dst.type.kind == Type::STRING && varIsByAddress(i.dst)) {
                // Buffer-backed string: strcpy(&dst, src)
                fprintf(out, "    lea rdi, [rbp%s]\n",
                        offStr(fi.offsets[i.dst.sval]).c_str());
                loadInt(i.src1, "rsi", fi);
                fprintf(out, "    mov eax, 0\n    call strcpy@PLT\n");
                return;
            }
            if (i.dst.type.kind == Type::FLOAT) {
                loadFloat(i.src1, "xmm0", fi);
                storeFloat(i.dst, "xmm0", fi);
            } else {
                loadInt(i.src1, "rax", fi);
                storeInt(i.dst, "rax", fi);
            }
            return;
        case Op::LOAD: {
            // dst = src1[src2]   (8-byte elements)
            loadInt(i.src2, "rcx", fi); // index
            int baseOff = fi.offsets[i.src1.sval];
            fprintf(out, "    lea rax, [rbp%s]\n", offStr(baseOff).c_str());
            if (i.dst.type.kind == Type::FLOAT) {
                fprintf(out, "    movsd xmm0, qword ptr [rax + rcx*8]\n");
                storeFloat(i.dst, "xmm0", fi);
            } else {
                fprintf(out, "    mov rax, [rax + rcx*8]\n");
                storeInt(i.dst, "rax", fi);
            }
            return;
        }
        case Op::STORE: {
            // dst[src1] = src2
            loadInt(i.src1, "rcx", fi); // index
            int baseOff = fi.offsets[i.dst.sval];
            fprintf(out, "    lea rax, [rbp%s]\n", offStr(baseOff).c_str());
            if (i.src2.type.kind == Type::FLOAT) {
                loadFloat(i.src2, "xmm0", fi);
                fprintf(out, "    movsd qword ptr [rax + rcx*8], xmm0\n");
            } else {
                loadInt(i.src2, "rdx", fi);
                fprintf(out, "    mov [rax + rcx*8], rdx\n");
            }
            return;
        }
        case Op::ADDR: {
            int off = fi.offsets[i.src1.sval];
            fprintf(out, "    lea rax, [rbp%s]\n", offStr(off).c_str());
            storeInt(i.dst, "rax", fi);
            return;
        }
        case Op::LABEL:
            fprintf(out, ".%s:\n", i.src1.sval.c_str());
            return;
        case Op::JMP:
            fprintf(out, "    jmp .%s\n", i.src1.sval.c_str());
            return;
        case Op::JZ:
            loadInt(i.src1, "rax", fi);
            fprintf(out, "    test rax, rax\n    jz .%s\n",
                    i.src2.sval.c_str());
            return;
        case Op::JNZ:
            loadInt(i.src1, "rax", fi);
            fprintf(out, "    test rax, rax\n    jnz .%s\n",
                    i.src2.sval.c_str());
            return;
        case Op::RET:
            if (!i.src1.isNone()) {
                if (i.src1.type.kind == Type::FLOAT)
                    loadFloat(i.src1, "xmm0", fi);
                else
                    loadInt(i.src1, "rax", fi);
            }
            fprintf(out, "    mov rsp, rbp\n    pop rbp\n    ret\n");
            return;
        default:
            return;
        }
    }

    void lowerCall(const Instr &callInstr, std::vector<Operand> &params,
                   FrameInfo &fi) {
        // Classify each param: float -> xmm reg, int/ptr -> int reg, overflow
        // -> stack.
        std::vector<int> intRegArgs, floatRegArgs, stackArgs;
        for (size_t k = 0; k < params.size(); ++k) {
            Operand &p = params[k];
            if (p.type.kind == Type::FLOAT) {
                if (floatRegArgs.size() < 8)
                    floatRegArgs.push_back(k);
                else
                    stackArgs.push_back(k);
            } else {
                if (intRegArgs.size() < 6)
                    intRegArgs.push_back(k);
                else
                    stackArgs.push_back(k);
            }
        }

        // Stack args: pad if odd to keep 16-byte alignment.
        int padding = (stackArgs.size() % 2) ? 8 : 0;
        if (padding)
            fprintf(out, "    sub rsp, 8\n");
        for (int k = (int)stackArgs.size() - 1; k >= 0; --k) {
            Operand &p = params[stackArgs[k]];
            if (p.type.kind == Type::FLOAT) {
                loadFloat(p, "xmm0", fi);
                fprintf(out,
                        "    sub rsp, 8\n    movsd qword ptr [rsp], xmm0\n");
            } else {
                loadInt(p, "rax", fi);
                fprintf(out, "    push rax\n");
            }
        }

        // Load register args.
        for (size_t k = 0; k < intRegArgs.size(); ++k)
            loadInt(params[intRegArgs[k]], INT_REGS[k], fi);
        for (size_t k = 0; k < floatRegArgs.size(); ++k)
            loadFloat(params[floatRegArgs[k]], FLOAT_REGS[k], fi);

        // For variadic targets, al = number of xmm registers used. Set it
        // always — harmless otherwise.
        fprintf(out, "    mov eax, %zu\n", floatRegArgs.size());

        const std::string &fname = callInstr.src1.sval;
        if (isExternalSymbol(fname))
            fprintf(out, "    call %s@PLT\n", fname.c_str());
        else
            fprintf(out, "    call %s\n", fname.c_str());

        // Unwind stack args
        int unwind = (int)stackArgs.size() * 8 + padding;
        if (unwind > 0)
            fprintf(out, "    add rsp, %d\n", unwind);

        // Capture return value
        if (!callInstr.dst.isNone()) {
            if (callInstr.dst.type.kind == Type::FLOAT)
                storeFloat(callInstr.dst, "xmm0", fi);
            else
                storeInt(callInstr.dst, "rax", fi);
        }
    }
};

} // namespace

void emitX86Asm(const IRProgram &p, FILE *out) {
    Emitter e;
    e.out = out;
    e.run(p);
}
