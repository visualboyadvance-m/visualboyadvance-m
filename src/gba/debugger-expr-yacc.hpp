/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_DEXP_DEBUGGER_EXPR_YACC_HPP_INCLUDED
# define YY_DEXP_DEBUGGER_EXPR_YACC_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int dexp_debug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    TOK_PLUS = 258,                /* TOK_PLUS  */
    TOK_MINUS = 259,               /* TOK_MINUS  */
    TOK_DIVIDE = 260,              /* TOK_DIVIDE  */
    TOK_MULTIPLY = 261,            /* TOK_MULTIPLY  */
    TOK_LSHIFT = 262,              /* TOK_LSHIFT  */
    TOK_RSHIFT = 263,              /* TOK_RSHIFT  */
    TOK_LPAREN = 264,              /* TOK_LPAREN  */
    TOK_RPAREN = 265,              /* TOK_RPAREN  */
    TOK_OR = 266,                  /* TOK_OR  */
    TOK_AND = 267,                 /* TOK_AND  */
    TOK_XOR = 268,                 /* TOK_XOR  */
    TOK_NEGATE = 269,              /* TOK_NEGATE  */
    TOK_BBRACKET = 270,            /* TOK_BBRACKET  */
    TOK_HBRACKET = 271,            /* TOK_HBRACKET  */
    TOK_WBRACKET = 272,            /* TOK_WBRACKET  */
    TOK_LBRACKET = 273,            /* TOK_LBRACKET  */
    TOK_RBRACKET = 274,            /* TOK_RBRACKET  */
    TOK_REGISTER = 275,            /* TOK_REGISTER  */
    TOK_NUMBER = 276,              /* TOK_NUMBER  */
    TOK_ID = 277                   /* TOK_ID  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 44 "debugger-expr.y"

  unsigned int number;
  char *string;

#line 91 "debugger-expr-yacc.hpp"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE dexp_lval;


int dexp_parse (void);


#endif /* !YY_DEXP_DEBUGGER_EXPR_YACC_HPP_INCLUDED  */
