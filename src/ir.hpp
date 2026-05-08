#pragma once
#include "ast.hpp"
#include <string>
#include <vector>
#include <cstdio>

enum class Op {
    // Binary arithmetic / comparison / logical
    ADD, SUB, MUL, DIV, MOD,
    EQ, NEQ, LT, LE, GT, GE,
    AND, OR,
    // Unary
    NEG, NOT,
    // Move
    COPY,            // dst = src1
    // Memory (static arrays)
    LOAD,            // dst = src1[src2]      (src1 = array base var)
    STORE,           // dst[src1] = src2      (dst    = array base var)
    ADDR,            // dst = &src1           (src1 = array variable)
    // Control flow
    LABEL,           // src1 = label name
    JMP,             // src1 = label
    JZ,              // src1 = cond, src2 = label
    JNZ,             // src1 = cond, src2 = label
    // Calls
    PARAM,           // src1 = arg
    CALL,            // dst = call src1=func, src2=int_const(nargs)
    RET,             // src1 = value (or NONE for void)
};

struct Operand {
    enum Kind {
        NONE, TEMP, VAR, INT_CONST, FLOAT_CONST, STR_CONST, LABEL_REF, FUNC_REF
    };
    Kind        kind = NONE;
    long long   ival = 0;     // INT_CONST value, STR_CONST index
    double      fval = 0;     // FLOAT_CONST value
    std::string sval;          // VAR/TEMP/LABEL/FUNC name; STR_CONST raw text (informational)
    Type        type;

    static Operand none()                 { return {}; }
    static Operand intC(long long v)      { Operand o; o.kind=INT_CONST;   o.ival=v; o.type=tyInt();   return o; }
    static Operand floatC(double v)       { Operand o; o.kind=FLOAT_CONST; o.fval=v; o.type=tyFloat(); return o; }
    static Operand boolC(bool v)          { Operand o; o.kind=INT_CONST;   o.ival=v?1:0; o.type=tyBool(); return o; }
    static Operand strC(int idx, const std::string& s) {
        Operand o; o.kind=STR_CONST; o.ival=idx; o.sval=s; o.type=tyStr(); return o;
    }
    static Operand temp(const std::string& nm, Type t) {
        Operand o; o.kind=TEMP; o.sval=nm; o.type=t; return o;
    }
    static Operand var(const std::string& nm, Type t) {
        Operand o; o.kind=VAR; o.sval=nm; o.type=t; return o;
    }
    static Operand label(const std::string& nm) {
        Operand o; o.kind=LABEL_REF; o.sval=nm; return o;
    }
    static Operand func(const std::string& nm) {
        Operand o; o.kind=FUNC_REF; o.sval=nm; return o;
    }
    bool isNone() const { return kind == NONE; }
};

struct Instr {
    Op       op;
    Operand  dst;
    Operand  src1;
    Operand  src2;
    Loc      loc;
};

struct IRFunction {
    std::string             name;
    Type                    returnType;
    std::vector<Type>       paramTypes;
    std::vector<std::string> paramNames;
    std::vector<Symbol*>    paramSyms;
    std::vector<Symbol*>    locals;     // collected as DeclStmts are visited (for codegen frame layout)
    std::vector<Instr>      code;
    int                     nextTemp  = 0;
    int                     nextLabel = 0;

    Operand newTemp(Type t) {
        std::string nm = "t" + std::to_string(nextTemp++);
        return Operand::temp(nm, t);
    }
    std::string newLabel(const char* hint = "L") {
        return std::string(hint) + std::to_string(nextLabel++);
    }
};

struct IRProgram {
    std::vector<IRFunction>     funcs;
    std::vector<std::string>    stringConstants;   // emitted in .rodata
};

// Print IR to a stream in a human-readable form.
void printIR(const IRProgram& p, FILE* out);
