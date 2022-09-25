%{
#include <stdio.h>
  
#include "../System.h"
#include "GBA.h"
#include "../common/Port.h"

#include <string>
#include <map>
#include <iostream>

unsigned int dexp_result = 0;
extern int dexp_error(const char *);
extern int dexp_lex();
extern char *dexp_text;

std::map<std::string, uint32_t> dexp_vars;

#define readWord(addr) \
  READ32LE((&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

#define readHalfWord(addr) \
  READ16LE((&map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]))

#define readByte(addr) \
  map[(addr)>>24].address[(addr) & map[(addr)>>24].mask]


%}

%token TOK_PLUS TOK_MINUS 
%token TOK_DIVIDE TOK_MULTIPLY TOK_LSHIFT TOK_RSHIFT
%token TOK_LPAREN TOK_RPAREN TOK_OR TOK_AND TOK_XOR TOK_NEGATE
%token TOK_BBRACKET TOK_HBRACKET TOK_WBRACKET 
%token TOK_LBRACKET TOK_RBRACKET
%left TOK_MINUS TOK_PLUS TOK_NEGATE
%left TOK_MULTIPLY TOK_DIVIDE
%left TOK_LSHIFT TOK_RSHIFT
%left TOK_OR
%left TOK_AND
%left TOK_XOR

%union
{
  unsigned int number;
  char *string;
}

%token <number> TOK_REGISTER
%token <number> TOK_NUMBER
%token <string> TOK_ID
%type <number> final
%type <number> exp
%%

final: exp {dexp_result = $1;}
;

exp: TOK_NUMBER           { $$ = $1; }
| TOK_ID { 
  std::string v($1); 
  if (dexp_vars.count(v) == 0) {
    printf("Variable %s not defined.\n", $1);
    YYABORT; 
  }
  $$ = dexp_vars[v]; 
}
| exp TOK_PLUS exp  { $$ = $1 + $3; }
| exp TOK_MINUS exp { $$ = $1 - $3;}
| TOK_MINUS exp { $$ = 0 - $2;}
| TOK_NEGATE exp { $$ = ~$2;}
| exp TOK_DIVIDE exp { $$ = $1 / $3;}
| exp TOK_MULTIPLY exp { $$ = $1 * $3;}
| exp TOK_LSHIFT exp { $$ = $1 << $3;}
| exp TOK_RSHIFT exp { $$ = $1 >> $3;}
| TOK_LPAREN exp TOK_RPAREN { $$=$2;}
| exp TOK_AND exp { $$ = $1 & $3;}
| exp TOK_OR exp { $$ = $1 | $3;}
| exp TOK_XOR exp { $$ = $1 ^ $3; }
| TOK_REGISTER { $$ = reg[$1].I; }
| TOK_BBRACKET exp TOK_RBRACKET { $$ = readByte($2); }
| TOK_HBRACKET exp TOK_RBRACKET { $$ = readHalfWord($2); }
| TOK_WBRACKET exp TOK_RBRACKET { $$ = readWord($2); }
| TOK_LBRACKET exp TOK_RBRACKET { $$ = readWord($2); }
;
%%

bool dexp_eval(char *expr, uint32_t *result)
{
  extern void dexp_flush();
  extern char *dexprString;
  extern int dexprCol;

  dexp_flush();
    
  
  dexprString = expr;
  dexprCol = 0;
    
  if(!dexp_parse()) {
    *result = dexp_result;
    return true;
  } else {
    return false;
  }    
}

int dexp_error(const char *)
{
  return 0;
}

void dexp_setVar(char *name, uint32_t value)
{
  std::string a(name);
  dexp_vars[a] = value;
}

void dexp_listVars()
{
  std::map<std::string, uint32_t>::iterator iter;

  for (iter = dexp_vars.begin(); iter != dexp_vars.end(); iter++) {
    printf("%s = %08X\n", iter->first.c_str(), iter->second);
  }
}

void dexp_saveVars(char *file)
{
  std::map<std::string, uint32_t>::iterator iter;

  FILE *f = fopen(file, "w");
  if (!f) {
    printf("Could not open file %s\n", file);
    return;
  }

  for (iter = dexp_vars.begin(); iter != dexp_vars.end(); iter++) {
    fprintf(f, "%s = %08X\n", iter->first.c_str(), iter->second);
  }
  fclose(f);
}

void dexp_loadVars(char *file)
{
  std::map<std::string, uint32_t>::iterator iter;
  char buffer[500];
  char name[500];
  uint32_t val;

  FILE *f = fopen(file, "r");
  if (!f) {
    printf("Could not open file %s\n", file);
    return;
  }

  while (fgets(buffer, 500, f) != NULL) {
    if (sscanf(buffer, "%s = %x",name,&val) == 2) {
      dexp_setVar(name, val);
    }
  }
}

  
