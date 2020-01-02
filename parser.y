%{
    #include "node.h"
    NBlock *programBlock; // root node of final AST

    extern int yylex();
    void yyerror(const char *s) { printf("Error: %s\n", s); }
%}

%union {
    Node *node;
    NBlock *block;
    NExpression *expr;
    NStatement *stmt;
    NIdentifier *ident;
    NVariableDeclaration *var_decl;
    std::vector<NVariableDeclaration *> *varvec;
    std::vector<NExpression *> *exprvec;
    std::string *string;
    int token;
}

// Terminal symbols defined from token.l file.
%token <string> TIDENTIFIER TINTEGER TDOUBLE
%token <token> TCEQ TCNE TCLT TCLE TCGT TCGE TEQUAL TRETARM
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TCOMMA TDOT
%token <token> TPLUS TMINUS TMUL TDIV TCOLON TLET TFUNC
%token <token> TRETURN TEXTERNAL

// Define type each nonterminal represents.
%type <ident> ident
%type <expr> numeric expr sub_expr
%type <varvec> func_decl_args
%type <exprvec> call_args
%type <block> program stmts block
%type <stmt> stmt var_decl func_decl func_var_decl extern_decl
%type <token> comparison

// Operator precedence.
%left TPLUS TMINUS
%left TMUL TDIV

%start program

%%

program : stmts { programBlock = $1; }
        ;

stmts   : stmt { $$ = new NBlock(); $$->statements.push_back($<stmt>1); }
        | stmts stmt { $1->statements.push_back($<stmt>2); }
        ;

stmt    : var_decl 
        | func_decl
        | extern_decl
        | expr { $$ = new NExpressionStatement(*$1); }
        | TRETURN expr { $$ = new NReturnStatement(*$2); }
        ;

block   : TLBRACE stmts TRBRACE { $$ = $2; }
        | TLBRACE TRBRACE { $$ = new NBlock(); }
        ;

extern_decl : TEXTERNAL TFUNC ident TLPAREN func_decl_args TRPAREN TRETARM ident
                { $$ = new NExternDeclaration(*$8, *$3, *$5); delete $5; }
            ;

var_decl    : TLET ident TCOLON ident { $$ = new NVariableDeclaration(*$4, *$2); }
            | TLET ident TCOLON ident TEQUAL expr { $$ = new NVariableDeclaration(*$4, *$2, $6); }
            ;

func_var_decl   : ident TCOLON ident { $$ = new NVariableDeclaration(*$3, *$1); }
                ;

func_decl   : TFUNC ident TLPAREN func_decl_args TRPAREN TRETARM ident block
                { $$ = new NFunctionDeclaration(*$7, *$2, *$4, *$8); delete $4; }
            ;

func_decl_args  : /*blank*/ { $$ = new VariableList(); }
                | func_var_decl { $$ = new VariableList(); $$->push_back($<var_decl>1); }
                | func_decl_args TCOMMA func_var_decl { $1->push_back($<var_decl>3); }
                ;

ident   : TIDENTIFIER { $$ = new NIdentifier(*$1); delete $1; }
        ;

numeric : TINTEGER { $$ = new NInteger(atol($1->c_str())); delete $1; }
        | TDOUBLE { $$ = new NDouble(atof($1->c_str())); delete $1; }
        ;

sub_expr    : ident { $<ident>$ = $1; }
            | numeric
            | expr comparison expr { $$ = new NBinaryOperator(*$1, $2, *$3); }
            | TLPAREN expr TRPAREN { $$ = $2; }

expr    : ident TEQUAL expr { $$ = new NAssignment(*$<ident>1, *$3); }
        | ident TLPAREN call_args TRPAREN { $$ = new NMethodCall(*$1, *$3); delete $3; }
        | sub_expr { $$ = $1; }
        ;

call_args   : /*blank*/ { $$ = new ExpressionList(); }
            | sub_expr { $$ = new ExpressionList(); $$->push_back($1); }
            | call_args TCOMMA sub_expr { $1->push_back($3); }
            ;

comparison  : TCEQ | TCNE | TCLT | TCLE | TCGT | TCGE
            | TPLUS | TMINUS | TMUL | TDIV
            ;

%%