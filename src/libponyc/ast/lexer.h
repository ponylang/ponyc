#ifndef LEXER_H
#define LEXER_H

#include "error.h"
#include "source.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct lexer_t lexer_t;

typedef enum
{
  TK_NONE,

  // literals
  TK_STRING,
  TK_INT,
  TK_FLOAT,
  TK_ID,

  // symbols
  TK_LBRACE,
  TK_RBRACE,
  TK_LPAREN,
  TK_RPAREN,
  TK_LSQUARE,
  TK_RSQUARE,

  TK_COMMA,
  TK_ARROW,
  TK_DBLARROW,
  TK_DOT,
  TK_BANG,
  TK_COLON,
  TK_SEMI,
  TK_ASSIGN,

  TK_PLUS,
  TK_MINUS,
  TK_MULTIPLY,
  TK_DIVIDE,
  TK_MOD,
  TK_HAT,

  TK_LSHIFT,
  TK_RSHIFT,

  TK_LT,
  TK_LE,
  TK_GE,
  TK_GT,

  TK_EQ,
  TK_NE,

  TK_PIPE,
  TK_AMP,
  TK_QUESTION,

  TK_LBRACE_NEW,
  TK_LPAREN_NEW,
  TK_LSQUARE_NEW,
  TK_MINUS_NEW,

  // keywords
  TK_COMPILER_INTRINSIC,

  TK_USE,
  TK_TYPE,
  TK_TRAIT,
  TK_CLASS,
  TK_ACTOR,

  TK_IS,
  TK_ISNT,

  TK_VAR,
  TK_LET,
  TK_NEW,
  TK_FUN,
  TK_BE,

  TK_ISO,
  TK_TRN,
  TK_REF,
  TK_VAL,
  TK_BOX,
  TK_TAG,

  TK_THIS,
  TK_RETURN,
  TK_BREAK,
  TK_CONTINUE,
  TK_CONSUME,
  TK_RECOVER,

  TK_IF,
  TK_THEN,
  TK_ELSE,
  TK_ELSEIF,
  TK_END,
  TK_WHILE,
  TK_DO,
  TK_REPEAT,
  TK_UNTIL,
  TK_FOR,
  TK_IN,
  TK_MATCH,
  TK_AS,
  TK_WHERE,
  TK_TRY,
  TK_ERROR,

  TK_NOT,
  TK_AND,
  TK_OR,
  TK_XOR,

  // abstract
  TK_PROGRAM,
  TK_PACKAGE,
  TK_MODULE,

  TK_MEMBERS,
  TK_FVAR,
  TK_FLET,

  TK_TYPES,
  TK_UNIONTYPE,
  TK_ISECTTYPE,
  TK_TUPLETYPE,
  TK_NOMINAL,
  TK_STRUCTURAL,
  TK_THISTYPE,
  TK_FUNTYPE,

  TK_TYPEPARAMS,
  TK_TYPEPARAM,
  TK_PARAMS,
  TK_PARAM,
  TK_TYPEARGS,
  TK_POSITIONALARGS,
  TK_NAMEDARGS,
  TK_NAMEDARG,

  TK_SEQ,
  TK_IDSEQ,
  TK_QUALIFY,
  TK_CALL,
  TK_TUPLE,
  TK_ARRAY,
  TK_OBJECT,
  TK_CASES,
  TK_CASE,

  TK_REFERENCE,
  TK_PACKAGEREF,
  TK_TYPEREF,
  TK_FUNREF,
  TK_FVARREF,
  TK_FLETREF,
  TK_VARREF,
  TK_LETREF,
  TK_PARAMREF,

  TK_EOF,
  TK_LEX_ERROR
} token_id;

typedef struct token_t
{
  token_id id;
  bool newline;
  //bool error;
  source_t* source;
  size_t line;
  size_t pos;

  union
  {
    const char* string;
    double real;
    __uint128_t integer;
  };
} token_t;

lexer_t* lexer_open(source_t* source);
void lexer_close(lexer_t* lexer);
token_t* lexer_next(lexer_t* lexer);

token_t* token_blank(token_id id);
void token_copy_pos(token_t* src, token_t* dst);
token_t* token_from(token_t* token, token_id id);
token_t* token_from_string(token_t* token, const char* id);
token_t* token_dup(token_t* token);
const char* token_string(token_t* token);
double token_float(token_t* token);
size_t token_int(token_t* token);
void token_setid(token_t* token, token_id id);
void token_setstring(token_t* token, const char* s);
void token_free(token_t* token);

#endif
