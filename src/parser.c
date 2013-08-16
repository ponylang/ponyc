#include "parser.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

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

#define NEED(X) \
  if( !X ) \
  { \
    error_new( parser->errors, parser->t->line, parser->t->pos, \
      "Unexpected: %d", current( parser ) ); \
    ast_free( ast ); \
    return NULL; \
  }

/* The API for parser rules starts here */

#define AST(ID) ast_t* ast = ast_new( parser, ID )
#define AST_TOKEN() ast_t* ast = ast_token( parser )
#define INSERT(ID) insert( parser, ID, ast )

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
    rulealtlist( parser, alt, TK_NONE, ast ); \
  }

#define DELIM(SEP, ...) \
  { \
    ALT( __VA_ARGS__ ); \
    NEED( rulealtlist( parser, alt, SEP, ast ) ); \
  }

#define OPTDELIM(SEP, ...) \
  { \
    ALT( __VA_ARGS__ ); \
    rulealtlist( parser, alt, SEP, ast ); \
  }

#define FORWARD(X) return X( parser )
#define DONE() return ast

/* The API for parser rules ends here */

struct parser_t
{
  lexer_t* lexer;
  token_t* t;
  ast_t* ast;
  errorlist_t* errors;
};

typedef ast_t* (*rule_t)( parser_t* );

typedef struct alt_t
{
  token_id id;
  rule_t f;
} alt_t;

static token_id current( parser_t* parser )
{
  return parser->t->id;
}

static ast_t* ast_new( parser_t* parser, token_id id )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = calloc( 1, sizeof(token_t) );
  ast->t->id = id;
  ast->t->line = parser->t->line;
  ast->t->pos = parser->t->pos;

  return ast;
}

static ast_t* ast_token( parser_t* parser )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = parser->t;
  parser->t = lexer_next( parser->lexer );

  return ast;
}

static void ast_free( ast_t* ast )
{
  if( ast == NULL ) { return; }
  if( ast->t != NULL ) { token_free( ast->t ); }

  ast_free( ast->child );
  ast_free( ast->sibling );
}

static void insert( parser_t* parser, token_id id, ast_t* ast )
{
  ast_t* child = ast_new( parser, id );
  child->sibling = ast->child;
  ast->child = child;
}

static bool acceptalt( parser_t* parser, const token_id* id, ast_t* ast )
{
  while( *id != TK_NONE )
  {
    if( current( parser ) == *id )
    {
      if( ast != NULL )
      {
        ast_t* child = ast_token( parser );
        child->sibling = ast->child;
        ast->child = child;
      } else {
        token_free( parser->t );
        parser->t = lexer_next( parser->lexer );
      }

      return true;
    }

    id++;
  }

  return false;
}

static bool rulealt( parser_t* parser, const alt_t* alt, ast_t* ast )
{
  while( alt->f != NULL )
  {
    if( (alt->id == TK_NONE) || (current( parser ) == alt->id) )
    {
      ast_t* child = alt->f( parser );

      if( child == NULL )
      {
        return false;
      }

      child->sibling = ast->child;
      ast->child = child;
      return true;
    }

    alt++;
  }

  return false;
}

static bool rulealtlist( parser_t* parser, const alt_t* alt, token_id sep,
  ast_t* ast )
{
  token_id tok[2] = { sep, TK_NONE };
  bool have = false;

  while( true )
  {
    if( !rulealt( parser, alt, ast ) )
    {
      break;
    }

    have = true;

    if( (sep != TK_NONE) && !acceptalt( parser, tok, NULL ) )
    {
      break;
    }
  }

  return have;
}

// forward declarations
static ast_t* oftype( parser_t* parser );
static ast_t* type( parser_t* parser );
static ast_t* expr( parser_t* parser );

// rules
static ast_t* assign( parser_t* parser )
{
  // (EQUALS expr)?
  if( ACCEPT_DROP( TK_EQUALS ) )
  {
    AST_TOKEN();
    RULE( expr );
    DONE();
  } else {
    AST( TK_NONE );
    DONE();
  }
}

static ast_t* param( parser_t* parser )
{
  // ID oftype assign
  AST_TOKEN();
  RULE( oftype );
  RULE( assign );
  DONE();
}

static ast_t* params( parser_t* parser )
{
  // LPAREN (param (COMMA param)*)? RPAREN
  AST( TK_PARAMS );
  EXPECT_DROP( TK_LPAREN );
  OPTDELIM( TK_COMMA, { TK_NONE, param } );
  EXPECT_DROP( TK_RPAREN );
  DONE();
}

static ast_t* basemode( parser_t* parser )
{
  // basemode ::= ISO | VAR | VAL | TAG | ID
  // (basemode (PIPE basemode)*)?
  AST( TK_MODE );
  OPTDELIM( TK_PIPE,
    { TK_ISO, ast_token },
    { TK_VAR, ast_token },
    { TK_VAL, ast_token },
    { TK_TAG, ast_token }
    );
  DONE();
}

static ast_t* mode( parser_t* parser )
{
  // (LBRACE basemode (ARROW (THIS | ID))? RBRACE)?
  AST( TK_MODE );

  if( ACCEPT_DROP( TK_LBRACE ) )
  {
    RULE( basemode );

    if( ACCEPT_DROP( TK_ARROW ) )
    {
      EXPECT( TK_THIS, TK_ID );
    } else {
      INSERT( TK_NONE );
    }

    EXPECT_DROP( TK_RBRACE );
  } else {
    INSERT( TK_MODE );
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* formalparams( parser_t* parser )
{
  // formalparam ::= EQUALS expr | type
  // (LBRACKET formalparam (COMMA formalparam)* RBRACKET)?
  AST( TK_FORMALPARAMS );

  if( ACCEPT_DROP( TK_LBRACKET ) )
  {
    DELIM( TK_COMMA,
      { TK_EQUALS, assign },
      { TK_NONE, type }
      );

    EXPECT_DROP( TK_RBRACKET );
  }

  DONE();
}

static ast_t* type( parser_t* parser )
{
  // FIX:
  return NULL;
}

static ast_t* oftype( parser_t* parser )
{
  // (COLON type)?
  if( ACCEPT_DROP( TK_COLON ) )
  {
    FORWARD( type );
  } else {
    AST( TK_INFER );
    DONE();
  }
}

static ast_t* seq( parser_t* parser )
{
  AST( TK_SEQ );
  DELIM( TK_SEMI, { TK_NONE, expr } );
  DONE();
}

static ast_t* local( parser_t* parser )
{
  // (VAR | VAL) ID oftype EQUALS expr
  AST_TOKEN();
  EXPECT( TK_ID );
  RULE( oftype );
  EXPECT_DROP( TK_EQUALS );
  RULE( expr );
  DONE();
}

static ast_t* lambda( parser_t* parser )
{
  // FUN mode params oftype EQUALS expr
  AST_TOKEN();
  RULE( mode );
  RULE( params );
  RULE( oftype );
  EXPECT_DROP( TK_EQUALS );
  RULE( expr );
  DONE();
}

static ast_t* conditional( parser_t* parser )
{
  // IF seq THEN expr (ELSE expr | END)
  AST_TOKEN();
  RULE( seq );
  EXPECT_DROP( TK_THEN );
  RULE( expr );

  if( ACCEPT_DROP( TK_ELSE ) )
  {
    RULE( expr );
  } else {
    EXPECT( TK_END );
  }

  DONE();
}

static ast_t* as( parser_t* parser )
{
  // (AS ID COLON type)?
  AST( TK_AS );

  if( ACCEPT_DROP( TK_AS ) )
  {
    EXPECT( TK_ID );
    EXPECT_DROP( TK_COLON );
    RULE( type );
  }

  DONE();
}

static ast_t* caseexpr( parser_t* parser )
{
  // PIPE binary? as (IF binary)? EQUALS SEQ
  AST_TOKEN();

  // FIX: binary

  RULE( as );

  if( ACCEPT_DROP( TK_IF ) )
  {
    // FIX: binary
  } else {
    INSERT( TK_IF );
  }

  EXPECT_DROP( TK_EQUALS );
  RULE( seq );
  DONE();
}

static ast_t* match( parser_t* parser )
{
  // MATCH seq caseexpr* END
  AST_TOKEN();
  RULE( seq );
  LIST( { TK_PIPE, caseexpr } );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* whileloop( parser_t* parser )
{
  // WHILE seq DO expr
  AST_TOKEN();
  RULE( seq );
  EXPECT_DROP( TK_DO );
  RULE( expr );
  DONE();
}

static ast_t* doloop( parser_t* parser )
{
  // DO expr WHILE expr
  AST_TOKEN();
  RULE( expr );
  EXPECT_DROP( TK_WHILE );
  RULE( expr );
  DONE();
}

static ast_t* forloop( parser_t* parser )
{
  // FOR ID oftype IN expr DO expr
  AST_TOKEN();
  EXPECT( TK_ID );
  RULE( oftype );
  EXPECT_DROP( TK_IN );
  RULE( expr );
  EXPECT_DROP( TK_DO );
  RULE( expr );
  DONE();
}

static ast_t* returnexpr( parser_t* parser )
{
  // RETURN expr
  AST_TOKEN();
  RULE( expr );
  DONE();
}

static ast_t* expr( parser_t* parser )
{
  // local | lambda | conditional | match | whileloop | doloop | forloop |
  // BREAK | CONTINUE | RETURN | SEMI | binary (EQUALS expr)?
  AST( TK_EXPR );
  RULEALT(
    { TK_VAR, local },
    { TK_VAL, local },
    { TK_FUN, lambda },
    { TK_IF, conditional },
    { TK_MATCH, match },
    { TK_WHILE, whileloop },
    { TK_DO, doloop },
    { TK_FOR, forloop },
    { TK_BREAK, ast_token },
    { TK_CONTINUE, ast_token },
    { TK_RETURN, returnexpr },
    { TK_SEMI, ast_token }
    );

  // FIX: binary (EQUALS expr)?

  DONE();
}

static ast_t* function( parser_t* parser )
{
  // (FUN | MSG) mode ID formalparams params oftype protection (EQUALS seq)?
  AST_TOKEN();
  RULE( mode );
  EXPECT( TK_ID );
  RULE( formalparams );
  RULE( params );
  RULE( oftype );

  if( !ACCEPT( TK_PRIVATE, TK_PACKAGE ) )
  {
    INSERT( TK_NONE );
  }

  if( ACCEPT_DROP( TK_EQUALS ) )
  {
    RULE( seq );
  } else {
    INSERT( TK_SEQ );
  }

  DONE();
}

static ast_t* field( parser_t* parser )
{
  // (VAR | VAL) ID COLON type
  AST_TOKEN();
  EXPECT( TK_ID );
  EXPECT( TK_COLON );
  RULE( type );
  DONE();
}

static ast_t* is( parser_t* parser )
{
  // (IS type (COMMA type)*)?
  AST( TK_IS );

  if( ACCEPT_DROP( TK_IS ) )
  {
    DELIM( TK_COMMA, { TK_NONE, type } );
  }

  DONE();
}

static ast_t* class( parser_t* parser )
{
  // (TRAIT | CLASS | ACTOR) ID formalparams mode protection is (field | function)*
  AST_TOKEN();
  EXPECT( TK_ID );
  RULE( formalparams );
  RULE( mode );

  if( !ACCEPT( TK_PRIVATE, TK_PACKAGE, TK_INFER ) )
  {
    INSERT( TK_NONE );
  }

  RULE( is );
  LIST(
    { TK_VAR, field },
    { TK_VAL, field },
    { TK_FUN, function },
    { TK_MSG, function }
    );
  DONE();
}

static ast_t* typedecl( parser_t* parser )
{
  // FIX: name remapping?
  // TYPE ID (DOT ID)* (COLON type)? is
  AST_TOKEN();
  DELIM( TK_DOT, { TK_ID, ast_token } );

  if( ACCEPT_DROP( TK_COLON ) )
  {
    RULE( type );
  }

  RULE( is );
  DONE();
}

static ast_t* use( parser_t* parser )
{
  // USE STRING (AS ID)?
  AST_TOKEN();
  EXPECT( TK_STRING );

  if( ACCEPT_DROP( TK_AS ) )
  {
    EXPECT( TK_ID );
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* module( parser_t* parser )
{
  // (use | type | trait | class | actor)*
  AST( TK_MODULE );
  LIST(
    { TK_USE, use },
    { TK_TYPE, typedecl },
    { TK_TRAIT, class },
    { TK_CLASS, class },
    { TK_ACTOR, class }
    );
  EXPECT_DROP( TK_EOF );
  DONE();
}

// external API
parser_t* parser_open( const char* file )
{
  // open the lexer
  lexer_t* lexer = lexer_open( file );
  if( lexer == NULL ) { return NULL; }

  // create a parser and attach the lexer
  parser_t* parser = calloc( 1, sizeof(parser_t) );
  parser->lexer = lexer;
  parser->t = lexer_next( lexer );
  parser->errors = errorlist_new();
  parser->ast = module( parser );

  errorlist_t* e = lexer_errors( lexer );

  if( e->count > 0 )
  {
    parser->errors->count += e->count;
    e->tail->next = parser->errors->head;
    parser->errors->head = e->head;
    e->head = NULL;
    e->tail = NULL;
  }

  errorlist_free( e );
  lexer_close( lexer );
  parser->lexer = NULL;

  return parser;
}

ast_t* parser_ast( parser_t* parser )
{
  return parser->ast;
}

errorlist_t* parser_errors( parser_t* parser )
{
  return parser->errors;
}

void parser_close( parser_t* parser )
{
  if( parser == NULL ) { return; }
  ast_free( parser->ast );
  errorlist_free( parser->errors );
  free( parser );
}
