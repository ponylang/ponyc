#ifndef PARSERAPI_H
#define PARSERAPI_H

#include "ast.h"

typedef enum
{
  BLOCK_OK,
  BLOCK_NOTPRESENT,
  BLOCK_EMPTY,
  BLOCK_ERROR
} block_t;

typedef struct parser_t
{
  source_t* source;
  lexer_t* lexer;
  token_t* t;
} parser_t;

typedef ast_t* (*rule_t)( parser_t* );

typedef struct alt_t
{
  token_id id;
  rule_t f;
} alt_t;

ast_t* consume( parser_t* parser );

void insert( parser_t* parser, token_id id, ast_t* ast );

bool look( parser_t* parser, const token_id* id );

bool acceptalt( parser_t* parser, const token_id* id, ast_t* ast );

bool rulealt( parser_t* parser, const alt_t* alt, ast_t* ast );

block_t block( parser_t* parser, const alt_t* alt, token_id name,
  token_id start, token_id stop, token_id sep, ast_t* ast );

ast_t* forwardalt( parser_t* parser, const alt_t* alt );

#define TOK(...) \
  static const token_id tok[] = \
  { \
    __VA_ARGS__, \
    TK_NONE \
  }

#define ALT(...) \
  static const alt_t alt[] = \
  { \
    __VA_ARGS__, \
    { 0, NULL } \
  }

#define BAILOUT() \
  { \
    error( parser->source, parser->t->line, parser->t->pos, \
      "syntax error (%s, %d)", __FUNCTION__, __LINE__ ); \
    ast_free( ast ); \
    return NULL; \
  }

#define NEED(X) if( !X ) BAILOUT()

/* The API for parser rules starts here */

#define AST(ID) ast_t* ast = ast_new( ID, parser->t->line, parser->t->pos, NULL )
#define AST_TOKEN() ast_t* ast = consume( parser )
#define AST_RULE(X) ast_t* ast = X( parser )
#define INSERT(ID) insert( parser, ID, ast )

#define LOOK(...) \
  ({\
    TOK( __VA_ARGS__ ); \
    look( parser, tok ); \
  })

#define ACCEPT(...) \
  ({\
    TOK( __VA_ARGS__ ); \
    acceptalt( parser, tok, ast ); \
  })

#define ACCEPT_DROP(...) \
  ({\
    TOK( __VA_ARGS__ ); \
    acceptalt( parser, tok, NULL ); \
  })

#define EXPECT(...) \
  {\
    TOK( __VA_ARGS__ ); \
    NEED( acceptalt( parser, tok, ast ) ); \
  }

#define EXPECT_DROP(...) \
  {\
    TOK( __VA_ARGS__ ); \
    NEED( acceptalt( parser, tok, NULL ) ); \
  }

#define RULE(X) \
  {\
    ALT( { TK_NONE, X } ); \
    NEED( rulealt( parser, alt, ast ) ); \
  }

#define RULEALT(...) \
  {\
    ALT( __VA_ARGS__ ); \
    NEED( rulealt( parser, alt, ast ) ); \
  }

#define LIST(...) \
  { \
    ALT( __VA_ARGS__ ); \
    block( parser, alt, TK_NONE, TK_NONE, TK_NONE, TK_NONE, ast ); \
  }

#define BLOCK(NAME, START, STOP, SEP, ...) \
  { \
    ALT( __VA_ARGS__ ); \
    switch( block( parser, alt, NAME, START, STOP, SEP, ast ) ) \
    { \
      case BLOCK_OK: \
      case BLOCK_EMPTY: \
        break; \
      case BLOCK_NOTPRESENT: \
      case BLOCK_ERROR: \
        BAILOUT(); \
        break; \
    } \
  }

#define OPTBLOCK(NAME, START, STOP, SEP, ...) \
  { \
    ALT( __VA_ARGS__ ); \
    switch( block( parser, alt, NAME, START, STOP, SEP, ast ) ) \
    { \
      case BLOCK_OK: \
      case BLOCK_EMPTY: \
      case BLOCK_NOTPRESENT: \
        break; \
      case BLOCK_ERROR: \
        BAILOUT(); \
        break; \
    } \
  }

#define FORWARD(X) return X( parser )

#define FORWARDALT(...) \
  { \
    ALT( __VA_ARGS__ ); \
    return forwardalt( parser, alt ); \
  }

#define BINOP(X) \
  { \
    ast_t* binop = ast_new( X, parser->t->line, parser->t->pos, NULL ); \
    ast_add( binop, ast ); \
    ast = binop; \
  }

#define BINOP_TOKEN() \
  { \
    ast_t* binop = consume( parser ); \
    ast_add( binop, ast ); \
    ast = binop; \
  }

#define SCOPE() \
  { \
    ast_t* child = ast_child( ast ); \
    if( child == NULL ) \
    { \
      ast_scope( ast ); \
    } else { \
      ast_t* next = ast_sibling( child ); \
      while( next != NULL ) \
      { \
        child = next; \
        next = ast_sibling( child ); \
      } \
      ast_scope( child ); \
    } \
  }

#define DONE() return ast;

#endif
