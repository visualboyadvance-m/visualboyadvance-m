%{
#include "expr.cpp.h"

#ifndef __GNUC__
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

char *exprString;
int exprCol;

#define YY_INPUT(buf,result,max_size) \
    { \
    int c = *exprString++; \
    exprCol++;\
    result = (c == 0) ? YY_NULL : (buf[0] = c, 1); \
    }
%}

%option nomain
%option noyywrap
%option nounput

SIZEOF "sizeof"
ID [a-zA-Z_][a-zA-Z0-9_]*
NUM [0-9]+
DOT "."
ARROW "->"
STAR "*"
ADDR "&"

%%

{SIZEOF} {
        return TOKEN_SIZEOF;
}

{ID} {
        return TOKEN_IDENTIFIER;
}

{NUM} {
        return TOKEN_NUMBER;
}

{DOT} {
        return TOKEN_DOT;
}       

{ARROW} {
        return TOKEN_ARROW;
}

{ADDR} {
        return TOKEN_ADDR;
}

{STAR} {
        return TOKEN_STAR;
}

[ \t\n]+

. return *yytext;

%%

void exprCleanBuffer()
{
  yy_delete_buffer(YY_CURRENT_BUFFER);
  yy_init = 1;
}
