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
  size_t optional;
} parser_t;

typedef ast_t* (*rule_t)( parser_t* );

ast_t* consume( parser_t* parser );

void insert( parser_t* parser, token_id id, ast_t* ast );

bool look( parser_t* parser, const token_id* id );

bool accept( parser_t* parser, const token_id* id, ast_t* ast );

bool rulealt( parser_t* parser, const rule_t* alt, ast_t* ast );

ast_t* forwardalt( parser_t* parser, const rule_t* alt );

#define TOK(...) \
  static const token_id tok[] = \
  { \
    __VA_ARGS__, \
    TK_NONE \
  }

#define ALT(...) \
  static const rule_t alt[] = \
  { \
    __VA_ARGS__, \
    NULL \
  }

#define PUSH() (parser->optional++)

#define POP() (parser->optional--)

#define NEED(X) \
  if( !X ) \
  { \
    if( parser->optional == 0 ) \
    { \
      error( parser->source, parser->t->line, parser->t->pos, \
        "syntax error (%s, %d)", __FUNCTION__, __LINE__ ); \
    } \
    ast_free( ast ); \
    return NULL; \
  }

/* The API for parser rules starts here */

#define AST(ID) \
  ast_t* ast = ast_new( ID, parser->t->line, parser->t->pos, NULL )

#define AST_TOKEN(...) \
  NEED( LOOK( __VA_ARGS__) ); \
  ast_t* ast = consume( parser )

#define AST_RULE(X) ast_t* ast = X( parser ); NEED( ast )

#define INSERT(ID) insert( parser, ID, ast )

#define LOOK(...) \
  ({ \
    TOK( __VA_ARGS__ ); \
    look( parser, tok ); \
  })

#define ACCEPT(...) \
  ({ \
    TOK( __VA_ARGS__ ); \
    accept( parser, tok, ast ); \
  })

#define ACCEPT_DROP(...) \
  ({ \
    TOK( __VA_ARGS__ ); \
    accept( parser, tok, NULL ); \
  })

#define EXPECT(...) \
  { \
    TOK( __VA_ARGS__ ); \
    NEED( accept( parser, tok, ast ) ); \
  }

#define EXPECT_DROP(...) \
  { \
    TOK( __VA_ARGS__ ); \
    NEED( accept( parser, tok, NULL ) ); \
  }

#define RULE(...) \
  { \
    ALT( __VA_ARGS__ ); \
    NEED( rulealt( parser, alt, ast ) ); \
  }

#define OPTRULE(...) \
  { \
    PUSH(); \
    ALT( __VA_ARGS__ ); \
    if( !rulealt( parser, alt, ast ) ) \
    { \
      INSERT( TK_NONE );
    } \
    POP(); \
  )

#define IFRULE(X, ...) \
  if( ACCEPT_DROP( X ) ) \
  { \
    RULE( __VA_ARGS__ ); \
  } else { \
    INSERT( TK_NONE ); \
  }

#define WHILERULE(X, ...) \
  while( ACCEPT_DROP( X ) ) \
  { \
    RULE( __VA_ARGS__ ); \
  }

#define SEQRULE(...) \
  { \
    PUSH(); \
    ALT( __VA_ARGS__ ); \
    while( rulealt( parser, alt, ast ) ); \
    POP(); \
  }

#define FORWARD(...) \
  { \
    ALT( __VA_ARGS__ ); \
    return forwardalt( parser, alt ); \
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
