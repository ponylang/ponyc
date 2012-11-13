#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef struct lexer_t lexer_t;

typedef enum
{
  // primitives
  TK_STRING,
  TK_INT,
  TK_FLOAT,
  TK_ID,
  TK_TYPEID,

  // symbols
  TK_LBRACE,
  TK_RBRACE,
  TK_LPAREN,
  TK_RPAREN,
  TK_LBRACKET,
  TK_RBRACKET,
  TK_COMMA,
  TK_RESULTS,

  TK_CALL,
  TK_PACKAGE,
  TK_OFTYPE,
  TK_PARTIAL,
  TK_ASSIGN,
  TK_LIST,
  TK_BANG,

  TK_PLUS,
  TK_MINUS,
  TK_MULTIPLY,
  TK_DIVIDE,
  TK_MOD,

  TK_LSHIFT,
  TK_RSHIFT,

  TK_LT,
  TK_LE,
  TK_GE,
  TK_GT,

  TK_EQ,
  TK_NOTEQ,
  TK_STEQ,
  TK_NSTEQ,

  TK_OR,
  TK_AND,
  TK_XOR,

  TK_UNIQ,
  TK_READONLY,
  TK_RECEIVER,

  // keywords
  TK_USE,
  TK_DECLARE,
  TK_TYPE,
  TK_LAMBDA,
  TK_TRAIT,
  TK_OBJECT,
  TK_ACTOR,
  TK_IS,
  TK_VAR,
  TK_DELEGATE,
  TK_NEW,
  TK_AMBIENT,
  TK_FUNCTION,
  TK_MESSAGE,
  TK_THROWS,
  TK_THROW,
  TK_RETURN,
  TK_BREAK,
  TK_CONTINUE,
  TK_IF,
  TK_ELSE,
  TK_FOR,
  TK_IN,
  TK_WHILE,
  TK_DO,
  TK_MATCH,
  TK_CASE,
  TK_AS,
  TK_CATCH,
  TK_ALWAYS,
  TK_THIS,
  TK_TRUE,
  TK_FALSE,

  // abstract
  TK_MODULE,
  TK_DECLAREMAP,
  TK_MAP,
  TK_TYPEBODY,
  TK_TYPECLASS,
  TK_ANNOTATION,
  TK_FORMALARGS,
  TK_FIELD,
  TK_ARG,
  TK_ARGS,
  TK_BLOCK,
  TK_CASEVAR,

  TK_EOF
} token_id;

typedef struct token_t
{
  token_id id;
  size_t line;
  size_t pos;

  union
  {
    char* string;
    double flt;
    size_t integer;
  };

  struct token_t* next;
} token_t;

lexer_t* lexer_open( const char* file );
void lexer_close( lexer_t* lexer );

token_t* lexer_next( lexer_t* lexer );
void token_free( token_t* token );

#endif
