#ifndef PARSERAPI_H
#define PARSERAPI_H

#include "ast.h"
#include <limits.h>

typedef struct parser_t
{
  source_t* source;
  lexer_t* lexer;
  token_t* t;
  size_t optional;
} parser_t;

typedef int (*prec_t)( token_id id );

typedef ast_t* (*rule_t)( parser_t* );

ast_t* consume( parser_t* parser );

void insert( parser_t* parser, token_id id, ast_t* ast );

bool look( parser_t* parser, const token_id* id );

bool accept( parser_t* parser, const token_id* id, ast_t* ast );

ast_t* rulealt( parser_t* parser, const rule_t* alt, ast_t* ast );

ast_t* bindop( parser_t* parser, prec_t precedence, ast_t* ast, const rule_t* alt );

void syntax_error( parser_t* parser_t, const char* func, int line );

void scope( ast_t* ast );

#define TOKS(...) \
  static const token_id tok[] = \
  { \
    __VA_ARGS__, \
    TK_NONE \
  }

#define ALTS(...) \
  static const rule_t alt[] = \
  { \
    __VA_ARGS__, \
    NULL \
  }

#define PUSH() (parser->optional++)

#define POP() (parser->optional--)

#define SYNTAX_ERROR() \
  syntax_error( parser, __FUNCTION__, __LINE__ )

#define NEED(X) \
  if( !X ) \
  { \
    SYNTAX_ERROR(); \
    ast_free( ast ); \
    return NULL; \
  }

/* This is the only external API call */
ast_t* parse( source_t* source, rule_t start );

/* The API for parser rules starts here */

#define DECL(rule) \
  static ast_t* rule( parser_t* parser )

#define DEF(rule) \
  static ast_t* rule( parser_t* parser ) \
  { \
    ast_t* ast = NULL

#define CHECK(...) \
  NEED( LOOK( __VA_ARGS__ ) )

#define AST(ID) \
  ast = ast_new( ID, parser->t->line, parser->t->pos, NULL )

#define AST_TOKEN(...) \
  NEED( LOOK( __VA_ARGS__) ); \
  ast = consume( parser )

#define AST_RULE(...) \
  { \
    ALTS( __VA_ARGS__ ); \
    ast = rulealt( parser, alt, NULL ); \
    NEED( ast ); \
  }

#define INSERT(ID) insert( parser, ID, ast )

#define LOOK(...) \
  ({ \
    TOKS( __VA_ARGS__ ); \
    look( parser, tok ); \
  })

#define ACCEPT(...) \
  ({ \
    TOKS( __VA_ARGS__ ); \
    accept( parser, tok, ast ); \
  })

#define ACCEPT_DROP(...) \
  ({ \
    TOKS( __VA_ARGS__ ); \
    accept( parser, tok, NULL ); \
  })

#define OPTIONAL(...) \
  if( !ACCEPT( __VA_ARGS__ ) ) \
  { \
    INSERT( TK_NONE ); \
  }

#define EXPECT(...) \
  { \
    TOKS( __VA_ARGS__ ); \
    NEED( accept( parser, tok, ast ) ); \
  }

#define SKIP(...) \
  { \
    TOKS( __VA_ARGS__ ); \
    NEED( accept( parser, tok, NULL ) ); \
  }

#define RULE(...) \
  { \
    ALTS( __VA_ARGS__ ); \
    NEED( rulealt( parser, alt, ast ) ); \
  }

#define OPTRULE(...) \
  { \
    PUSH(); \
    ALTS( __VA_ARGS__ ); \
    if( !rulealt( parser, alt, ast ) ) \
    { \
      INSERT( TK_NONE ); \
    } \
    POP(); \
  }

#define IFRULE(X, ...) \
  if( ACCEPT_DROP( X ) ) \
  { \
    RULE( __VA_ARGS__ ); \
  } else { \
    INSERT( TK_NONE ); \
  }

#define IFTOKEN(X, ...) \
  if( ACCEPT_DROP( X ) ) \
  { \
    EXPECT( __VA_ARGS__ ); \
  } else { \
    INSERT( TK_NONE ); \
  }

#define WHILERULE(X, ...) \
  while( ACCEPT_DROP( X ) ) \
  { \
    RULE( __VA_ARGS__ ); \
  }

#define WHILETOKEN(X, ...) \
  while( ACCEPT_DROP( X ) ) \
  { \
    EXPECT( __VA_ARGS__ ); \
  }

#define SEQRULE(...) \
  { \
    PUSH(); \
    ALTS( __VA_ARGS__ ); \
    while( rulealt( parser, alt, ast ) ); \
    POP(); \
  }

#define BINDOP(...) \
  { \
    ALTS( __VA_ARGS__ ); \
    ast = bindop( parser, precedence, ast, alt ); \
  }

#define SCOPE() scope( ast )

#define DONE() \
    return ast; \
  }

#endif
