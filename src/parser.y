%code requires {
    #include "ast.hpp"
    #include <vector>
    #include <memory>
    #include <string>

    using ExprListUP = std::vector<std::unique_ptr<Expr>>;
    using StmtListUP = std::vector<std::unique_ptr<Stmt>>;
    using FuncListUP = std::vector<std::unique_ptr<FuncDecl>>;
    using ParamListV = std::vector<Param>;
}

%{
#include "ast.hpp"
#include "parser.tab.hpp"
#include <cstdio>
#include <cstdlib>

extern int   yylex();
extern int   yylineno;
extern char* yytext;

void yyerror(const char* s);

Program* g_program = nullptr;
int      g_parse_errors = 0;

static Loc locOf(const YYLTYPE& l) { return Loc{l.first_line, l.first_column}; }
%}

%locations

%union {
    long long  ival;
    double     fval;
    char*      sval;

    Expr*       expr;
    Stmt*       stmt;
    Block*      block;
    DeclStmt*   decl;
    AssignStmt* assign;
    FuncDecl*   func;
    Program*    prog;

    ExprListUP* expr_list;
    StmtListUP* stmt_list;
    FuncListUP* func_list;
    ParamListV* param_list;

    int type_kind;   // Type::Kind cast to int (avoids include order issue in union)
}

%token <ival> INT_LIT
%token <fval> FLOAT_LIT
%token <sval> STRING_LIT IDENT

%token IF ELSE WHILE FOR RETURN BREAK CONTINUE
%token TRUE_KW FALSE_KW
%token INT_KW FLOAT_KW STRING_KW BOOL_KW

%token PLUS MINUS STAR SLASH PERCENT
%token ASSIGN PLUSEQ MINUSEQ
%token EQ NEQ LT LE GT GE
%token AND OR NOT
%token LPAREN RPAREN LBRACE RBRACE LBRACK RBRACK
%token COMMA SEMI

%left  OR
%left  AND
%left  EQ NEQ
%left  LT LE GT GE
%left  PLUS MINUS
%left  STAR SLASH PERCENT
%right NOT UMINUS
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <prog>       program
%type <func_list>  func_list
%type <func>       func_decl
%type <param_list> params_opt params
%type <type_kind>  type
%type <block>      block
%type <stmt_list>  stmt_list_opt stmt_list
%type <stmt>       stmt for_init
%type <stmt>       if_stmt while_stmt for_stmt return_stmt expr_stmt
%type <decl>       decl_stmt
%type <assign>     assign_stmt assign_no_semi
%type <expr>       expr
%type <expr_list>  args_opt args

%start program

%%

program
    : func_list
        {
            auto* p = new Program();
            for (auto& f : *$1) p->funcs.push_back(std::move(f));
            delete $1;
            g_program = p;
            $$ = p;
        }
    ;

func_list
    : func_decl
        { $$ = new FuncListUP();
          $$->emplace_back(std::unique_ptr<FuncDecl>($1)); }
    | func_list func_decl
        { $$ = $1; $$->emplace_back(std::unique_ptr<FuncDecl>($2)); }
    ;

func_decl
    : type IDENT LPAREN params_opt RPAREN block
        {
            auto* f = new FuncDecl();
            f->loc = locOf(@1);
            f->returnType.kind = (Type::Kind)$1;
            f->name = $2; free($2);
            if ($4) { f->params = std::move(*$4); delete $4; }
            f->body.reset($6);
            $$ = f;
        }
    ;

type
    : INT_KW    { $$ = (int)Type::INT; }
    | FLOAT_KW  { $$ = (int)Type::FLOAT; }
    | STRING_KW { $$ = (int)Type::STRING; }
    | BOOL_KW   { $$ = (int)Type::BOOL; }
    ;

params_opt
    : /* empty */     { $$ = nullptr; }
    | params          { $$ = $1; }
    ;

params
    : type IDENT
        {
            $$ = new ParamListV();
            Param p;
            p.type.kind = (Type::Kind)$1;
            p.name = $2; free($2);
            p.loc = locOf(@1);
            $$->push_back(std::move(p));
        }
    | params COMMA type IDENT
        {
            $$ = $1;
            Param p;
            p.type.kind = (Type::Kind)$3;
            p.name = $4; free($4);
            p.loc = locOf(@3);
            $$->push_back(std::move(p));
        }
    ;

block
    : LBRACE stmt_list_opt RBRACE
        {
            auto* b = new Block();
            b->loc = locOf(@1);
            if ($2) { b->stmts = std::move(*$2); delete $2; }
            $$ = b;
        }
    ;

stmt_list_opt
    : /* empty */     { $$ = nullptr; }
    | stmt_list       { $$ = $1; }
    ;

stmt_list
    : stmt
        { $$ = new StmtListUP();
          $$->emplace_back(std::unique_ptr<Stmt>($1)); }
    | stmt_list stmt
        { $$ = $1; $$->emplace_back(std::unique_ptr<Stmt>($2)); }
    ;

stmt
    : decl_stmt        { $$ = $1; }
    | assign_stmt      { $$ = $1; }
    | if_stmt          { $$ = $1; }
    | while_stmt       { $$ = $1; }
    | for_stmt         { $$ = $1; }
    | return_stmt      { $$ = $1; }
    | expr_stmt        { $$ = $1; }
    | block            { $$ = $1; }
    | BREAK SEMI
        { auto* s = new BreakStmt(); s->loc = locOf(@1); $$ = s; }
    | CONTINUE SEMI
        { auto* s = new ContinueStmt(); s->loc = locOf(@1); $$ = s; }
    ;

decl_stmt
    : type IDENT SEMI
        {
            auto* d = new DeclStmt();
            d->loc = locOf(@1);
            d->declaredType.kind = (Type::Kind)$1;
            d->name = $2; free($2);
            $$ = d;
        }
    | type IDENT ASSIGN expr SEMI
        {
            auto* d = new DeclStmt();
            d->loc = locOf(@1);
            d->declaredType.kind = (Type::Kind)$1;
            d->name = $2; free($2);
            d->init.reset($4);
            $$ = d;
        }
    | type IDENT LBRACK INT_LIT RBRACK SEMI
        {
            auto* d = new DeclStmt();
            d->loc = locOf(@1);
            d->declaredType.kind = Type::ARRAY;
            d->declaredType.elem = (Type::Kind)$1;
            d->declaredType.arrayLen = (int)$4;
            d->isArray  = true;
            d->arrayLen = (int)$4;
            d->name = $2; free($2);
            $$ = d;
        }
    ;

assign_stmt
    : assign_no_semi SEMI    { $$ = $1; }
    ;

assign_no_semi
    : IDENT ASSIGN expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::Assign;
            auto* v = new VarRef(); v->name = $1; free($1); v->loc = s->loc;
            s->target.reset(v);
            s->value.reset($3);
            $$ = s;
        }
    | IDENT LBRACK expr RBRACK ASSIGN expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::Assign;
            auto* ix = new IndexExpr(); ix->name = $1; free($1); ix->idx.reset($3); ix->loc = s->loc;
            s->target.reset(ix);
            s->value.reset($6);
            $$ = s;
        }
    | IDENT PLUSEQ expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::PlusEq;
            auto* v = new VarRef(); v->name = $1; free($1); v->loc = s->loc;
            s->target.reset(v);
            s->value.reset($3);
            $$ = s;
        }
    | IDENT LBRACK expr RBRACK PLUSEQ expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::PlusEq;
            auto* ix = new IndexExpr(); ix->name = $1; free($1); ix->idx.reset($3); ix->loc = s->loc;
            s->target.reset(ix);
            s->value.reset($6);
            $$ = s;
        }
    | IDENT MINUSEQ expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::MinusEq;
            auto* v = new VarRef(); v->name = $1; free($1); v->loc = s->loc;
            s->target.reset(v);
            s->value.reset($3);
            $$ = s;
        }
    | IDENT LBRACK expr RBRACK MINUSEQ expr
        {
            auto* s = new AssignStmt();
            s->loc = locOf(@1);
            s->op  = AssignStmt::MinusEq;
            auto* ix = new IndexExpr(); ix->name = $1; free($1); ix->idx.reset($3); ix->loc = s->loc;
            s->target.reset(ix);
            s->value.reset($6);
            $$ = s;
        }
    ;

if_stmt
    : IF LPAREN expr RPAREN stmt   %prec LOWER_THAN_ELSE
        {
            auto* s = new IfStmt();
            s->loc = locOf(@1);
            s->cond.reset($3);
            s->thenS.reset($5);
            $$ = s;
        }
    | IF LPAREN expr RPAREN stmt ELSE stmt
        {
            auto* s = new IfStmt();
            s->loc = locOf(@1);
            s->cond.reset($3);
            s->thenS.reset($5);
            s->elseS.reset($7);
            $$ = s;
        }
    ;

while_stmt
    : WHILE LPAREN expr RPAREN stmt
        {
            auto* s = new WhileStmt();
            s->loc = locOf(@1);
            s->cond.reset($3);
            s->body.reset($5);
            $$ = s;
        }
    ;

for_stmt
    : FOR LPAREN for_init expr SEMI assign_no_semi RPAREN stmt
        {
            auto* s = new ForStmt();
            s->loc = locOf(@1);
            s->init.reset($3);
            s->cond.reset($4);
            s->step.reset($6);
            s->body.reset($8);
            $$ = s;
        }
    ;

for_init
    : decl_stmt        { $$ = $1; }
    | assign_stmt      { $$ = $1; }
    | SEMI             { $$ = nullptr; }
    ;

return_stmt
    : RETURN SEMI
        { auto* r = new ReturnStmt(); r->loc = locOf(@1); $$ = r; }
    | RETURN expr SEMI
        { auto* r = new ReturnStmt(); r->loc = locOf(@1); r->value.reset($2); $$ = r; }
    ;

expr_stmt
    : expr SEMI
        { auto* s = new ExprStmt(); s->loc = locOf(@1); s->expr.reset($1); $$ = s; }
    ;

expr
    : INT_LIT
        { auto* e = new IntLit(); e->val = $1; e->loc = locOf(@1); $$ = e; }
    | FLOAT_LIT
        { auto* e = new FloatLit(); e->val = $1; e->loc = locOf(@1); $$ = e; }
    | STRING_LIT
        { auto* e = new StringLit(); e->val = $1; free($1); e->loc = locOf(@1); $$ = e; }
    | TRUE_KW
        { auto* e = new BoolLit(); e->val = true; e->loc = locOf(@1); $$ = e; }
    | FALSE_KW
        { auto* e = new BoolLit(); e->val = false; e->loc = locOf(@1); $$ = e; }
    | IDENT
        { auto* e = new VarRef(); e->name = $1; free($1); e->loc = locOf(@1); $$ = e; }
    | IDENT LBRACK expr RBRACK
        { auto* e = new IndexExpr(); e->name = $1; free($1); e->idx.reset($3); e->loc = locOf(@1); $$ = e; }
    | IDENT LPAREN args_opt RPAREN
        {
            auto* e = new CallExpr();
            e->callee = $1; free($1);
            if ($3) { e->args = std::move(*$3); delete $3; }
            e->loc = locOf(@1);
            $$ = e;
        }
    | LPAREN expr RPAREN
        { $$ = $2; }
    | expr PLUS expr
        { auto* e = new BinOp(); e->op = BinOp::Add; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr MINUS expr
        { auto* e = new BinOp(); e->op = BinOp::Sub; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr STAR expr
        { auto* e = new BinOp(); e->op = BinOp::Mul; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr SLASH expr
        { auto* e = new BinOp(); e->op = BinOp::Div; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr PERCENT expr
        { auto* e = new BinOp(); e->op = BinOp::Mod; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr EQ expr
        { auto* e = new BinOp(); e->op = BinOp::Eq;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr NEQ expr
        { auto* e = new BinOp(); e->op = BinOp::Neq; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr LT expr
        { auto* e = new BinOp(); e->op = BinOp::Lt;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr LE expr
        { auto* e = new BinOp(); e->op = BinOp::Le;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr GT expr
        { auto* e = new BinOp(); e->op = BinOp::Gt;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr GE expr
        { auto* e = new BinOp(); e->op = BinOp::Ge;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr AND expr
        { auto* e = new BinOp(); e->op = BinOp::And; e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | expr OR expr
        { auto* e = new BinOp(); e->op = BinOp::Or;  e->lhs.reset($1); e->rhs.reset($3); e->loc = locOf(@1); $$ = e; }
    | NOT expr
        { auto* e = new UnOp(); e->op = UnOp::Not; e->operand.reset($2); e->loc = locOf(@1); $$ = e; }
    | MINUS expr  %prec UMINUS
        { auto* e = new UnOp(); e->op = UnOp::Neg; e->operand.reset($2); e->loc = locOf(@1); $$ = e; }
    ;

args_opt
    : /* empty */    { $$ = nullptr; }
    | args           { $$ = $1; }
    ;

args
    : expr
        { $$ = new ExprListUP(); $$->emplace_back(std::unique_ptr<Expr>($1)); }
    | args COMMA expr
        { $$ = $1; $$->emplace_back(std::unique_ptr<Expr>($3)); }
    ;

%%

void yyerror(const char* s) {
    g_parse_errors++;
    fprintf(stderr, "%d:%d: parse error: %s (near '%s')\n",
            yylloc.first_line, yylloc.first_column, s, yytext);
}
