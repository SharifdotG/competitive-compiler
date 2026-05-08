#include "ast.hpp"
#include "codegen.hpp"
#include "ir.hpp"
#include "irgen.hpp"
#include "opt.hpp"
#include "parser.tab.hpp"
#include "sema.hpp"
#include "symtab.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libgen.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern FILE *yyin;
extern int yylex();
extern int yyparse();
extern int yylineno;
extern int yycolumn;
extern char *yytext;
extern YYSTYPE yylval;
extern YYLTYPE yylloc;
extern Program *g_program;
extern int g_parse_errors;

static const char *tok_name(int t) {
    switch (t) {
    case INT_LIT:
        return "INT_LIT";
    case FLOAT_LIT:
        return "FLOAT_LIT";
    case STRING_LIT:
        return "STRING_LIT";
    case IDENT:
        return "IDENT";
    case IF:
        return "IF";
    case ELSE:
        return "ELSE";
    case WHILE:
        return "WHILE";
    case FOR:
        return "FOR";
    case RETURN:
        return "RETURN";
    case BREAK:
        return "BREAK";
    case CONTINUE:
        return "CONTINUE";
    case TRUE_KW:
        return "TRUE";
    case FALSE_KW:
        return "FALSE";
    case INT_KW:
        return "INT";
    case FLOAT_KW:
        return "FLOAT";
    case STRING_KW:
        return "STRING";
    case BOOL_KW:
        return "BOOL";
    case PLUS:
        return "PLUS";
    case MINUS:
        return "MINUS";
    case STAR:
        return "STAR";
    case SLASH:
        return "SLASH";
    case PERCENT:
        return "PERCENT";
    case ASSIGN:
        return "ASSIGN";
    case PLUSEQ:
        return "PLUSEQ";
    case MINUSEQ:
        return "MINUSEQ";
    case EQ:
        return "EQ";
    case NEQ:
        return "NEQ";
    case LT:
        return "LT";
    case LE:
        return "LE";
    case GT:
        return "GT";
    case GE:
        return "GE";
    case AND:
        return "AND";
    case OR:
        return "OR";
    case NOT:
        return "NOT";
    case LPAREN:
        return "LPAREN";
    case RPAREN:
        return "RPAREN";
    case LBRACE:
        return "LBRACE";
    case RBRACE:
        return "RBRACE";
    case LBRACK:
        return "LBRACK";
    case RBRACK:
        return "RBRACK";
    case COMMA:
        return "COMMA";
    case SEMI:
        return "SEMI";
    default:
        return "UNKNOWN";
    }
}

static int dump_tokens(const char *path) {
    yyin = fopen(path, "r");
    if (!yyin) {
        fprintf(stderr, "cpc: cannot open %s\n", path);
        return 1;
    }
    int t;
    while ((t = yylex()) != 0) {
        printf("%3d:%-3d  %-12s  ", yylloc.first_line, yylloc.first_column,
               tok_name(t));
        if (t == STRING_LIT)
            printf("\"%s\"\n", yylval.sval);
        else if (t == INT_LIT)
            printf("%lld\n", yylval.ival);
        else if (t == FLOAT_LIT)
            printf("%g\n", yylval.fval);
        else if (t == IDENT)
            printf("%s\n", yylval.sval);
        else
            printf("%s\n", yytext);
    }
    fclose(yyin);
    return 0;
}

static void usage() {
    fprintf(stderr, "usage: cpc [flags] FILE.cl\n"
                    "  --tokens     dump tokens and exit\n"
                    "  --ast        dump AST and exit\n"
                    "  --ir         dump IR (three-address code) and exit\n"
                    "  --opt-ir     dump optimized IR and exit\n"
                    "  -S           emit assembly to FILE.s\n"
                    "  -o NAME      output executable name (default a.out)\n"
                    "  -h, --help   show this message\n");
}

int main(int argc, char **argv) {
    enum class Mode {
        Compile,
        Tokens,
        Ast,
        Ir,
        OptIr,
        Asm
    } mode = Mode::Compile;
    const char *file = nullptr;
    const char *outname = "a.out";
    (void)outname; // unused until M7

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tokens")
            mode = Mode::Tokens;
        else if (a == "--ast")
            mode = Mode::Ast;
        else if (a == "--ir")
            mode = Mode::Ir;
        else if (a == "--opt-ir")
            mode = Mode::OptIr;
        else if (a == "-S")
            mode = Mode::Asm;
        else if (a == "-o" && i + 1 < argc)
            outname = argv[++i];
        else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (!a.empty() && a[0] == '-') {
            fprintf(stderr, "cpc: unknown flag '%s'\n", a.c_str());
            usage();
            return 1;
        } else
            file = argv[i];
    }

    if (!file) {
        usage();
        return 1;
    }

    if (mode == Mode::Tokens) {
        return dump_tokens(file);
    }

    // Parse
    yyin = fopen(file, "r");
    if (!yyin) {
        fprintf(stderr, "cpc: cannot open %s\n", file);
        return 1;
    }
    yylineno = 1;
    yycolumn = 1;
    int parseRC = yyparse();
    fclose(yyin);

    if (parseRC != 0 || g_parse_errors > 0 || !g_program) {
        fprintf(stderr, "cpc: %d parse error(s)\n", g_parse_errors);
        return 1;
    }

    if (mode == Mode::Ast) {
        printAst(*g_program, stdout);
        return 0;
    }

    // Semantic analysis required for everything past parse + AST
    SymbolTable symtab;
    int semaErrors = runSemanticAnalysis(*g_program, symtab);
    if (semaErrors > 0) {
        fprintf(stderr, "cpc: %d semantic error(s)\n", semaErrors);
        return 1;
    }

    // IR generation required for everything past sema
    IRProgram *ir = generateIR(*g_program);

    if (mode == Mode::Ir) {
        printIR(*ir, stdout);
        delete ir;
        return 0;
    }
    if (mode == Mode::OptIr) {
        optimizeIR(*ir);
        printIR(*ir, stdout);
        delete ir;
        return 0;
    }

    // Always optimize before codegen (the optimizer is conservative and only
    // improves things)
    optimizeIR(*ir);

    if (mode == Mode::Asm) {
        // Output to outname (defaults to <basename>.s if not overridden)
        std::string asmPath;
        if (std::string(outname) != "a.out") {
            asmPath = outname;
        } else {
            std::string in = file;
            size_t slash = in.find_last_of('/');
            std::string base =
                (slash == std::string::npos) ? in : in.substr(slash + 1);
            size_t dot = base.find_last_of('.');
            if (dot != std::string::npos)
                base = base.substr(0, dot);
            asmPath = base + ".s";
        }
        FILE *f = fopen(asmPath.c_str(), "w");
        if (!f) {
            perror(asmPath.c_str());
            delete ir;
            return 1;
        }
        emitX86Asm(*ir, f);
        fclose(f);
        delete ir;
        return 0;
    }

    // Full compile: emit asm to a temp file, then invoke gcc to assemble +
    // link.
    char tmpl[] = "/tmp/cpcXXXXXX.s";
    int fd = mkstemps(tmpl, 2);
    if (fd < 0) {
        perror("mkstemps");
        delete ir;
        return 1;
    }
    FILE *f = fdopen(fd, "w");
    emitX86Asm(*ir, f);
    fclose(f);

    std::string cmd = "gcc -no-pie -o ";
    cmd += outname;
    cmd += " ";
    cmd += tmpl;
    cmd += " runtime.o";
    int rc = std::system(cmd.c_str());
    unlink(tmpl);
    delete ir;
    if (rc != 0) {
        fprintf(stderr, "cpc: gcc failed (exit %d)\n", rc);
        return 1;
    }
    return 0;
}
