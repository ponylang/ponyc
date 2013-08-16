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

#define BAILOUT() \
  { \
    error_new( parser->errors, parser->t->line, parser->t->pos, \
      "Unexpected: %d (%s, %d)", current( parser ), __FUNCTION__, __LINE__ ); \
    ast_free( ast ); \
    return NULL; \
  }

#define NEED(X) if( !X ) BAILOUT()

/* The API for parser rules starts here */

#define AST(ID) ast_t* ast = ast_new( ID, parser->t->line, parser->t->pos )
#define AST_TOKEN() ast_t* ast = ast_token( parser )
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
    ast_t* binop = ast_new( X, parser->t->line, parser->t->pos ); \
    binop->child = ast; \
    ast = binop; \
  }

#define BINOP_TOKEN() \
  { \
    ast_t* binop = ast_token( parser ); \
    binop->child = ast; \
    ast = binop; \
  }

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

typedef enum
{
  BLOCK_OK,
  BLOCK_NOTPRESENT,
  BLOCK_EMPTY,
  BLOCK_ERROR
} block_t;

static token_id current( parser_t* parser )
{
  return parser->t->id;
}

static ast_t* ast_token( parser_t* parser )
{
  ast_t* ast = calloc( 1, sizeof(ast_t) );
  ast->t = parser->t;
  parser->t = lexer_next( parser->lexer );

  return ast;
}

static void ast_reverse( ast_t* ast )
{
  if( ast == NULL ) { return; }

  ast_t* cur = ast->child;
  ast_t* next;
  ast_t* last = NULL;

  while( cur != NULL )
  {
    ast_reverse( cur );

    next = cur->sibling;
    cur->sibling = last;
    last = cur;
    cur = next;
  }
}

static void insert( parser_t* parser, token_id id, ast_t* ast )
{
  ast_t* child = ast_new( id, parser->t->line, parser->t->pos );
  child->sibling = ast->child;
  ast->child = child;
}

static bool look( parser_t* parser, const token_id* id )
{
  while( *id != TK_NONE )
  {
    if( current( parser ) == *id )
    {
      return true;
    }

    id++;
  }

  return false;
}

static bool acceptalt( parser_t* parser, const token_id* id, ast_t* ast )
{
  if( !look( parser, id ) )
  {
    return false;
  }

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

static block_t block( parser_t* parser, const alt_t* alt, token_id name,
  token_id start, token_id stop, token_id sep, ast_t* ast )
{
  token_id tok_start[2] = { start, TK_NONE };
  token_id tok_stop[2] = { stop, TK_NONE };
  token_id tok_sep[2] = { sep, TK_NONE };

  ast_t* child;

  if( name != TK_NONE )
  {
    child = ast_new( name, parser->t->line, parser->t->pos );
    child->sibling = ast->child;
    ast->child = child;
  } else {
    child = ast;
  }

  if( (start != TK_NONE) && !acceptalt( parser, tok_start, NULL ) )
  {
    return BLOCK_NOTPRESENT;
  }

  bool have = false;

  while( true )
  {
    if( (stop != TK_NONE) && acceptalt( parser, tok_stop, NULL ) )
    {
      break;
    }

    if( !rulealt( parser, alt, child ) )
    {
      if( start != TK_NONE )
      {
        return BLOCK_ERROR;
      }
      break;
    }

    have = true;

    if( (sep != TK_NONE) && !acceptalt( parser, tok_sep, NULL ) )
    {
      if( (stop != TK_NONE) && !acceptalt( parser, tok_stop, NULL ) )
      {
        return BLOCK_ERROR;
      }
      break;
    }
  }

  return have ? BLOCK_OK : BLOCK_EMPTY;
}

static ast_t* forwardalt( parser_t* parser, const alt_t* alt )
{
  while( alt->f != NULL )
  {
    if( (alt->id == TK_NONE) || (current( parser ) == alt->id) )
    {
      return alt->f( parser );
    }

    alt++;
  }

  return NULL;
}

// forward declarations
static ast_t* oftype( parser_t* parser );
static ast_t* type( parser_t* parser );
static ast_t* expr( parser_t* parser );
static ast_t* unary( parser_t* parser );

// rules
static ast_t* assign( parser_t* parser )
{
  // (EQUALS expr)?
  if( ACCEPT_DROP( TK_EQUALS ) )
  {
    FORWARD( expr );
  } else {
    AST( TK_NONE );
    DONE();
  }
}

static ast_t* arg( parser_t* parser )
{
  // expr (ARROW ID)?
  AST( TK_ARROW );
  RULE( expr );

  if( ACCEPT_DROP( TK_ARROW ) )
  {
    EXPECT( TK_ID );
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* param( parser_t* parser )
{
  // ID oftype assign
  AST_TOKEN();
  RULE( oftype );
  RULE( assign );
  DONE();
}

static ast_t* mode( parser_t* parser )
{
  // basemode ::= ISO | VAR | VAL | TAG | ID
  // (LBRACE basemode (PIPE basemode)* RBRACE)? (ARROW (THIS | ID))?
  AST( TK_MODE );
  OPTBLOCK( TK_MODE, TK_LBRACE, TK_RBRACE, TK_PIPE,
    { TK_ISO, ast_token },
    { TK_VAR, ast_token },
    { TK_VAL, ast_token },
    { TK_TAG, ast_token },
    { TK_ID, ast_token }
    );

  if( ACCEPT_DROP( TK_ARROW ) )
  {
    EXPECT( TK_THIS, TK_ID );
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* params( parser_t* parser )
{
  // LPAREN (param (COMMA param)*)? RBRACKET
  AST( TK_PARAMS );
  BLOCK( TK_PARAMS, TK_LPAREN, TK_RPAREN, TK_COMMA,
    { TK_NONE, param }
    );
  DONE();
}

static ast_t* formalparams( parser_t* parser )
{
  // (LBRACKET param (COMMA param)* RBRACKET)?
  AST( TK_FORMALPARAMS );
  OPTBLOCK( TK_NONE, TK_LBRACKET, TK_RBRACKET, TK_COMMA,
    { TK_NONE, param }
    );
  DONE();
}

static ast_t* formalargs( parser_t* parser )
{
  // formalarg ::= EQUALS expr | type
  // (LBRACKET formalarg (COMMA formalarg)* RBRACKET)?
  AST( TK_FORMALARGS );
  OPTBLOCK( TK_NONE, TK_LBRACKET, TK_RBRACKET, TK_COMMA,
    { TK_EQUALS, assign },
    { TK_NONE, type }
    );
  DONE();
}

static ast_t* functiontype( parser_t* parser )
{
  // FUN mode LPAREN (type (COMMA type)*)? RPAREN oftype
  AST( TK_FUNCTIONTYPE );
  EXPECT_DROP( TK_FUN );
  RULE( mode );
  BLOCK( TK_PARAMS, TK_LPAREN, TK_RPAREN, TK_COMMA,
    { TK_NONE, type }
    );
  RULE( oftype );
  DONE();
}

static ast_t* objecttype( parser_t* parser )
{
  // ID (DOT ID)* formalargs mode
  AST( TK_OBJECTTYPE );
  BLOCK( TK_TYPEID, TK_NONE, TK_NONE, TK_DOT,
    { TK_ID, ast_token }
    );
  RULE( formalargs );
  RULE( mode );
  DONE();
}

static ast_t* partialtype( parser_t* parser )
{
  // BACKSLASH objecttype
  AST( TK_PARTIALTYPE );
  RULE( objecttype );
  DONE();
}

static ast_t* adttype( parser_t* parser )
{
  // LPAREN type (PIPE type)* RPAREN
  AST( TK_ADT );
  BLOCK( TK_NONE, TK_LPAREN, TK_RPAREN, TK_PIPE,
    { TK_NONE, type }
    );
  DONE();
}


static ast_t* type( parser_t* parser )
{
  // adttype | partialtype | functiontype | objecttype
  FORWARDALT(
    { TK_LPAREN, adttype },
    { TK_BACKSLASH, partialtype },
    { TK_FUN, functiontype },
    { TK_ID, objecttype }
    );
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
  // expr (SEMI expr)*
  AST( TK_SEQ );
  BLOCK( TK_NONE, TK_NONE, TK_NONE, TK_SEMI,
    { TK_NONE, expr }
    );
  DONE();
}

static ast_t* primary( parser_t* parser )
{
  FORWARDALT(
    { TK_THIS, ast_token },
    { TK_TRUE, ast_token },
    { TK_FALSE, ast_token },
    { TK_INT, ast_token },
    { TK_FLOAT, ast_token },
    { TK_STRING, ast_token },
    { TK_ID, ast_token },
    { TK_LPAREN, seq }
    );
}

static ast_t* postfix( parser_t* parser )
{
  // primary (DOT ID | formalargs | LPAREN (arg (COMMA arg)*)? RPAREN)*
  AST_RULE( primary );

  while( true )
  {
    if( ACCEPT_DROP( TK_DOT ) )
    {
      BINOP( TK_DOT );
      EXPECT( TK_ID );
    } else if( LOOK( TK_LBRACKET ) ) {
      BINOP( TK_CALL );
      RULE( formalargs );
    } else if( LOOK( TK_LPAREN ) ) {
      BINOP( TK_CALL );
      BLOCK( TK_ARGS, TK_LPAREN, TK_RPAREN, TK_COMMA,
        { TK_NONE, arg }
        );
    } else {
      break;
    }
  }

  DONE();
}

static ast_t* unop( parser_t* parser )
{
  AST_TOKEN();
  RULE( unary );
  DONE();
}

static ast_t* unary( parser_t* parser )
{
  FORWARDALT(
    { TK_NOT, unop },
    { TK_MINUS, unop },
    { TK_BACKSLASH, unop },
    { TK_NONE, postfix }
    );
}

static bool is_binary( parser_t* parser )
{
  // any unary, postfix or primary can start a binary expression
  return LOOK( TK_NOT, TK_MINUS, TK_BACKSLASH, TK_THIS, TK_TRUE, TK_FALSE,
    TK_INT, TK_FLOAT, TK_STRING, TK_ID, TK_LPAREN );
}

static ast_t* binary( parser_t* parser )
{
  // unary (binop unary)*
  AST_RULE( unary );

  while(
    LOOK( TK_AND, TK_OR, TK_XOR, TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE,
      TK_MOD, TK_LSHIFT, TK_RSHIFT, TK_EQ, TK_NEQ, TK_LT, TK_LE, TK_GE, TK_GT
      )
    )
  {
    BINOP_TOKEN();
    RULE( unary );
  }

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

  if( is_binary( parser ) )
  {
    RULE( binary );
  }

  RULE( as );

  if( ACCEPT_DROP( TK_IF ) )
  {
    RULE( binary );
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

static ast_t* command( parser_t* parser )
{
  // binary (EQUALS expr)?
  if( !is_binary( parser ) )
  {
    return NULL;
  }

  AST_RULE( binary );

  if( LOOK( TK_EQUALS ) )
  {
    BINOP_TOKEN();
    RULE( expr );
  }

  DONE();
}

static ast_t* expr( parser_t* parser )
{
  // local | lambda | conditional | match | whileloop | doloop | forloop |
  // BREAK | CONTINUE | RETURN | SEMI | command
  FORWARDALT(
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
    { TK_SEMI, ast_token },
    { TK_NONE, command }
    );
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
    INSERT( TK_NONE );
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
  OPTBLOCK( TK_NONE, TK_IS, TK_NONE, TK_COMMA,
    { TK_NONE, type }
    );
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
  BLOCK( TK_TYPEID, TK_NONE, TK_NONE, TK_DOT,
    { TK_ID, ast_token }
    );
  RULE( oftype );
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
  ast_reverse( parser->ast );

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
  ast_t* ast = parser->ast;
  parser->ast = NULL;
  return ast;
}

errorlist_t* parser_errors( parser_t* parser )
{
  errorlist_t* e = parser->errors;
  parser->errors = NULL;
  return e;
}

void parser_close( parser_t* parser )
{
  if( parser == NULL ) { return; }
  ast_free( parser->ast );
  errorlist_free( parser->errors );
  free( parser );
}
