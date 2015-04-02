/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TOK_PLUS = 258,
     TOK_MINUS = 259,
     TOK_DIVIDE = 260,
     TOK_MULTIPLY = 261,
     TOK_LSHIFT = 262,
     TOK_RSHIFT = 263,
     TOK_LPAREN = 264,
     TOK_RPAREN = 265,
     TOK_OR = 266,
     TOK_AND = 267,
     TOK_XOR = 268,
     TOK_NEGATE = 269,
     TOK_BBRACKET = 270,
     TOK_HBRACKET = 271,
     TOK_WBRACKET = 272,
     TOK_LBRACKET = 273,
     TOK_RBRACKET = 274,
     TOK_REGISTER = 275,
     TOK_NUMBER = 276,
     TOK_ID = 277
   };
#endif
/* Tokens.  */
#define TOK_PLUS 258
#define TOK_MINUS 259
#define TOK_DIVIDE 260
#define TOK_MULTIPLY 261
#define TOK_LSHIFT 262
#define TOK_RSHIFT 263
#define TOK_LPAREN 264
#define TOK_RPAREN 265
#define TOK_OR 266
#define TOK_AND 267
#define TOK_XOR 268
#define TOK_NEGATE 269
#define TOK_BBRACKET 270
#define TOK_HBRACKET 271
#define TOK_WBRACKET 272
#define TOK_LBRACKET 273
#define TOK_RBRACKET 274
#define TOK_REGISTER 275
#define TOK_NUMBER 276
#define TOK_ID 277




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 44 "src/sdl/debugger-expr.y"
{
  unsigned int number;
  char *string;
}
/* Line 1489 of yacc.c.  */
#line 98 "src/sdl/debugger-expr-yacc.hpp"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

