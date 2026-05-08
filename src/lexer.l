%{
#include "parser.tab.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

int yycolumn = 1;

#define YY_USER_ACTION                                  \
    yylloc.first_line = yylloc.last_line = yylineno;    \
    yylloc.first_column = yycolumn;                     \
    yylloc.last_column = yycolumn + yyleng - 1;         \
    yycolumn += yyleng;

static char* dup_string_lit(const char* text, int len);
%}

%option noyywrap yylineno never-interactive

%%

[ \t\r]+        { /* skip horizontal whitespace */ }
\n              { yycolumn = 1; }

"//"[^\n]*      { /* line comment */ }
"/*"([^*]|\*+[^*/])*\*+"/"  {
    /* block comment — flex auto-bumps yylineno; fix yycolumn */
    int last_nl = -1;
    for (int i = 0; i < yyleng; ++i) {
        if (yytext[i] == '\n') last_nl = i;
    }
    if (last_nl >= 0) {
        yycolumn = yyleng - last_nl;
    }
}

"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"for"       { return FOR; }
"return"    { return RETURN; }
"break"     { return BREAK; }
"continue"  { return CONTINUE; }
"true"      { return TRUE_KW; }
"false"     { return FALSE_KW; }
"int"       { return INT_KW; }
"float"     { return FLOAT_KW; }
"string"    { return STRING_KW; }
"bool"      { return BOOL_KW; }

"+="        { return PLUSEQ; }
"-="        { return MINUSEQ; }
"=="        { return EQ; }
"!="        { return NEQ; }
"<="        { return LE; }
">="        { return GE; }
"&&"        { return AND; }
"||"        { return OR; }

"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return STAR; }
"/"         { return SLASH; }
"%"         { return PERCENT; }
"="         { return ASSIGN; }
"<"         { return LT; }
">"         { return GT; }
"!"         { return NOT; }
"("         { return LPAREN; }
")"         { return RPAREN; }
"{"         { return LBRACE; }
"}"         { return RBRACE; }
"["         { return LBRACK; }
"]"         { return RBRACK; }
","         { return COMMA; }
";"         { return SEMI; }

[0-9]+"."[0-9]+   { yylval.fval = atof(yytext); return FLOAT_LIT; }
[0-9]+            { yylval.ival = atoll(yytext); return INT_LIT; }

\"([^"\\\n]|\\.)*\"  {
    yylval.sval = dup_string_lit(yytext, yyleng);
    return STRING_LIT;
}

[A-Za-z_][A-Za-z0-9_]* {
    yylval.sval = strdup(yytext);
    return IDENT;
}

. {
    fprintf(stderr, "%d:%d: error: unexpected character '%s'\n",
            yylineno, yycolumn - yyleng, yytext);
}

%%

/* Strip outer quotes; unescape \n \t \\ \" \0 inside the literal. */
static char* dup_string_lit(const char* text, int len) {
    char* out = (char*)malloc(len);  /* upper bound: same length minus the two quotes */
    int o = 0;
    for (int i = 1; i < len - 1; ++i) {
        char c = text[i];
        if (c == '\\' && i + 1 < len - 1) {
            char nx = text[++i];
            switch (nx) {
                case 'n':  out[o++] = '\n'; break;
                case 't':  out[o++] = '\t'; break;
                case 'r':  out[o++] = '\r'; break;
                case '\\': out[o++] = '\\'; break;
                case '"':  out[o++] = '"';  break;
                case '0':  out[o++] = '\0'; break;
                default:   out[o++] = nx;   break;
            }
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
    return out;
}
