%{
#include <stdio.h>
#include <string.h>
#include "debugger-expr-yacc.hpp"

#ifdef _MSC_VER
#define YY_NO_UNISTD_H
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

extern YYSTYPE dexp_lval;

char *dexprString;
int dexprCol;

#define YY_INPUT(buf,result,max_size) \
    { \
    char c = *dexprString++; \
    dexprCol++;\
    result = (c == 0) ? YY_NULL : (buf[0] = c, 1); \
    }

%}

%option nomain
%option noyywrap
%option noinput
%option nounput

HEX [0-9a-fA-F]

%%
r(0?[0-9]|1[0-5])        dexp_lval.number=atoi((char *)(dexp_text+1)); return TOK_REGISTER;
[sS][pP]            dexp_lval.number = 13; return TOK_REGISTER;
[lL][rR]            dexp_lval.number = 14; return TOK_REGISTER;
[pP][cC]            dexp_lval.number = 15; return TOK_REGISTER;
[bB]"["             return TOK_BBRACKET;
[hH]"["             return TOK_HBRACKET;
[wW]"["             return TOK_WBRACKET;
"]"                 return TOK_RBRACKET;
"["                 return TOK_LBRACKET;
[1-9][0-9]*         dexp_lval.number=atoi(dexp_text); return TOK_NUMBER;
0{HEX}*             sscanf(dexp_text, "%x", &dexp_lval.number); return TOK_NUMBER;
0[xX]{HEX}+         sscanf((char *)(dexp_text + 2), "%x", &dexp_lval.number); return TOK_NUMBER;
"$"{HEX}+           sscanf((char *)(dexp_text + 1), "%x", &dexp_lval.number); return TOK_NUMBER;
"+"                 return TOK_PLUS;
"-"                 return TOK_MINUS;
"/"                return TOK_DIVIDE;
"*"                 return TOK_MULTIPLY;
"<<"                return TOK_LSHIFT;
">>"                return TOK_RSHIFT;
"("                 return TOK_LPAREN;
")"                 return TOK_RPAREN;
"|"                 return TOK_OR;
"&"                 return TOK_AND;
"^"                 return TOK_XOR;
"!"                 return TOK_NEGATE;
"~"                 return TOK_NEGATE;
" "                 ;
.                   printf("Unrecognised token: %s\n", dexp_text);
[A-Za-z_][A-Za-z0-9_]* dexp_lval.string=dexp_text; return TOK_ID;

%%

void dexp_flush()
{
  dexp__flush_buffer(YY_CURRENT_BUFFER);
}
