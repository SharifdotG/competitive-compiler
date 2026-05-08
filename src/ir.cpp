#include "ir.hpp"
#include <cstdio>

static const char* opSym(Op op) {
    switch (op) {
        case Op::ADD: return "+";
        case Op::SUB: return "-";
        case Op::MUL: return "*";
        case Op::DIV: return "/";
        case Op::MOD: return "%";
        case Op::EQ:  return "==";
        case Op::NEQ: return "!=";
        case Op::LT:  return "<";
        case Op::LE:  return "<=";
        case Op::GT:  return ">";
        case Op::GE:  return ">=";
        case Op::AND: return "&&";
        case Op::OR:  return "||";
        default:      return "?";
    }
}

static void printOperand(FILE* out, const Operand& o) {
    switch (o.kind) {
        case Operand::NONE:        fputs("_", out); break;
        case Operand::TEMP:
        case Operand::VAR:         fputs(o.sval.c_str(), out); break;
        case Operand::INT_CONST:   fprintf(out, "%lld", o.ival); break;
        case Operand::FLOAT_CONST: fprintf(out, "%g", o.fval); break;
        case Operand::STR_CONST:   fprintf(out, "@str%lld", o.ival); break;
        case Operand::LABEL_REF:   fputs(o.sval.c_str(), out); break;
        case Operand::FUNC_REF:    fputs(o.sval.c_str(), out); break;
    }
}

static void printInstr(FILE* out, const Instr& i) {
    switch (i.op) {
        case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV: case Op::MOD:
        case Op::EQ:  case Op::NEQ: case Op::LT:  case Op::LE:  case Op::GT:  case Op::GE:
        case Op::AND: case Op::OR:
            fputs("    ", out);
            printOperand(out, i.dst); fputs(" = ", out);
            printOperand(out, i.src1); fprintf(out, " %s ", opSym(i.op));
            printOperand(out, i.src2); fputc('\n', out);
            return;
        case Op::NEG:
            fputs("    ", out); printOperand(out, i.dst); fputs(" = -", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::NOT:
            fputs("    ", out); printOperand(out, i.dst); fputs(" = !", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::COPY:
            fputs("    ", out); printOperand(out, i.dst); fputs(" = ", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::LOAD:
            fputs("    ", out); printOperand(out, i.dst);
            fputs(" = ", out); printOperand(out, i.src1);
            fputc('[', out);   printOperand(out, i.src2); fputc(']', out);
            fputc('\n', out);
            return;
        case Op::STORE:
            fputs("    ", out); printOperand(out, i.dst);
            fputc('[', out);   printOperand(out, i.src1); fputs("] = ", out);
            printOperand(out, i.src2);
            fputc('\n', out);
            return;
        case Op::ADDR:
            fputs("    ", out); printOperand(out, i.dst);
            fputs(" = &", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::LABEL:
            printOperand(out, i.src1); fputs(":\n", out);
            return;
        case Op::JMP:
            fputs("    jmp ", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::JZ:
            fputs("    jz ", out); printOperand(out, i.src1);
            fputs(", ", out);     printOperand(out, i.src2); fputc('\n', out);
            return;
        case Op::JNZ:
            fputs("    jnz ", out); printOperand(out, i.src1);
            fputs(", ", out);      printOperand(out, i.src2); fputc('\n', out);
            return;
        case Op::PARAM:
            fputs("    param ", out); printOperand(out, i.src1); fputc('\n', out);
            return;
        case Op::CALL:
            fputs("    ", out);
            if (!i.dst.isNone()) { printOperand(out, i.dst); fputs(" = ", out); }
            fputs("call ", out);
            printOperand(out, i.src1);
            fputs(", ", out);
            printOperand(out, i.src2);
            fputc('\n', out);
            return;
        case Op::RET:
            if (i.src1.isNone()) fputs("    ret\n", out);
            else { fputs("    ret ", out); printOperand(out, i.src1); fputc('\n', out); }
            return;
    }
}

static const char* typeStr(const Type& t) { return typeName(t); }

void printIR(const IRProgram& p, FILE* out) {
    if (!p.stringConstants.empty()) {
        fputs("string_constants:\n", out);
        for (size_t i = 0; i < p.stringConstants.size(); ++i) {
            fprintf(out, "  @str%zu = \"", i);
            for (char c : p.stringConstants[i]) {
                switch (c) {
                    case '\n': fputs("\\n", out); break;
                    case '\t': fputs("\\t", out); break;
                    case '\\': fputs("\\\\", out); break;
                    case '"':  fputs("\\\"", out); break;
                    default:   fputc(c, out); break;
                }
            }
            fputs("\"\n", out);
        }
        fputc('\n', out);
    }
    for (const auto& f : p.funcs) {
        fprintf(out, "function %s(", f.name.c_str());
        for (size_t i = 0; i < f.paramNames.size(); ++i) {
            if (i) fputs(", ", out);
            fprintf(out, "%s: %s", f.paramNames[i].c_str(), typeStr(f.paramTypes[i]));
        }
        fprintf(out, ") -> %s {\n", typeStr(f.returnType));
        for (const auto& i : f.code) printInstr(out, i);
        fputs("}\n\n", out);
    }
}
