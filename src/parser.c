#include "parser.h"
#include "error.h"
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

/* The API for parser rules ends here */

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

typedef enum
{
  BLOCK_OK,
  BLOCK_NOTPRESENT,
  BLOCK_EMPTY,
  BLOCK_ERROR
} block_t;

static ast_t* consume( parser_t* parser )
{
  ast_t* ast = ast_token( parser->t );
  parser->t = lexer_next( parser->lexer );
  return ast;
}

static token_id current( parser_t* parser )
{
  return parser->t->id;
}

static void insert( parser_t* parser, token_id id, ast_t* ast )
{
  ast_t* child = ast_new( id, parser->t->line, parser->t->pos, NULL );
  ast_add( ast, child );
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
    ast_t* child = ast_token( parser->t );
    ast_add( ast, child );
  } else {
    token_free( parser->t );
  }

  parser->t = lexer_next( parser->lexer );
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

      ast_add( ast, child );
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
    child = ast_new( name, parser->t->line, parser->t->pos, NULL );
    ast_add( ast, child );
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
    AST( TK_INFER );
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
  AST( TK_PARAM );
  EXPECT( TK_ID );
  RULE( oftype );
  RULE( assign );
  DONE();
}

static ast_t* typeparam( parser_t* parser )
{
  // ID oftype assign
  AST( TK_TYPEPARAM );
  EXPECT( TK_ID );
  RULE( oftype );
  RULE( assign );
  DONE();
}

static ast_t* mode( parser_t* parser )
{
  // basemode ::= ISO | VAR | VAL | TAG | THIS | ID
  // (LBRACE basemode (PIPE basemode)* RBRACE)? (ARROW (THIS | ID))?
  AST( TK_MODE );
  OPTBLOCK( TK_LIST, TK_LBRACE, TK_RBRACE, TK_PIPE,
    { TK_ISO, consume },
    { TK_VAR, consume },
    { TK_VAL, consume },
    { TK_TAG, consume },
    { TK_THIS, consume },
    { TK_ID, consume }
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
  AST( TK_LIST );
  BLOCK( TK_NONE, TK_LPAREN, TK_RPAREN, TK_COMMA,
    { TK_NONE, param }
    );
  DONE();
}

static ast_t* typeparams( parser_t* parser )
{
  // (LBRACKET typeparam (COMMA typeparam)* RBRACKET)?
  AST( TK_LIST );
  OPTBLOCK( TK_NONE, TK_LBRACKET, TK_RBRACKET, TK_COMMA,
    { TK_NONE, typeparam }
    );
  DONE();
}

static ast_t* typeargs( parser_t* parser )
{
  // (LBRACKET type (COMMA type)* RBRACKET)?
  AST( TK_LIST );
  OPTBLOCK( TK_NONE, TK_LBRACKET, TK_RBRACKET, TK_COMMA,
    { TK_NONE, type }
    );
  DONE();
}

static ast_t* funtype( parser_t* parser )
{
  // FUN THROW? mode LPAREN (type (COMMA type)*)? RPAREN oftype
  AST( TK_FUNTYPE );
  EXPECT_DROP( TK_FUN );

  if( !ACCEPT( TK_THROW ) )
  {
    INSERT( TK_NONE );
  }

  RULE( mode );
  BLOCK( TK_LIST, TK_LPAREN, TK_RPAREN, TK_COMMA,
    { TK_NONE, type }
    );
  RULE( oftype );
  DONE();
}

static ast_t* objtype( parser_t* parser )
{
  // ID (DOT ID)? typeargs mode
  AST( TK_OBJTYPE );
  EXPECT( TK_ID );

  if( ACCEPT_DROP( TK_DOT ) )
  {
    EXPECT( TK_ID );
  } else {
    INSERT( TK_NONE );
  }

  RULE( typeargs );
  RULE( mode );
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
  // adttype | funtype | objtype
  FORWARDALT(
    { TK_LPAREN, adttype },
    { TK_FUN, funtype },
    { TK_ID, objtype }
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

static ast_t* parenseq( parser_t* parser )
{
  // LPAREN seq RPAREN
  ACCEPT_DROP( TK_LPAREN );
  AST_RULE( seq );
  SCOPE();
  EXPECT_DROP( TK_RPAREN );
  DONE();
}

static ast_t* primary( parser_t* parser )
{
  FORWARDALT(
    { TK_THIS, consume },
    { TK_INT, consume },
    { TK_FLOAT, consume },
    { TK_STRING, consume },
    { TK_ID, consume },
    { TK_LPAREN, parenseq }
    );
}

static ast_t* postfix( parser_t* parser )
{
  // primary (DOT ID | typeargs | LPAREN (arg (COMMA arg)*)? RPAREN)*
  AST_RULE( primary );

  while( true )
  {
    if( ACCEPT_DROP( TK_DOT ) )
    {
      BINOP( TK_DOT );
      EXPECT( TK_ID );
    } else if( LOOK( TK_LBRACKET ) ) {
      BINOP( TK_TYPEARGS );
      RULE( typeargs );
    } else if( LOOK( TK_LPAREN ) ) {
      BINOP( TK_CALL );
      BLOCK( TK_LIST, TK_LPAREN, TK_RPAREN, TK_COMMA,
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
    { TK_NONE, postfix }
    );
}

static bool is_binary( parser_t* parser )
{
  // any unary, postfix or primary can start a binary expression
  return LOOK( TK_NOT, TK_MINUS, TK_THIS, TK_INT, TK_FLOAT, TK_STRING, TK_ID,
    TK_LPAREN );
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
  // FUN THROW? mode params oftype EQUALS expr
  AST_TOKEN();
  SCOPE();

  if( !ACCEPT( TK_THROW ) )
  {
    INSERT( TK_NONE );
  }

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
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_THEN );
  RULE( expr );
  SCOPE();

  if( ACCEPT_DROP( TK_ELSE ) )
  {
    RULE( expr );
    SCOPE();
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
  // PIPE binary? as (IF binary)? EQUALS seq
  AST_TOKEN();

  if( is_binary( parser ) )
  {
    RULE( binary );
  } else {
    INSERT( TK_NONE );
  }

  RULE( as );

  if( ACCEPT_DROP( TK_IF ) )
  {
    RULE( binary );
  } else {
    INSERT( TK_NONE );
  }

  EXPECT_DROP( TK_EQUALS );
  RULE( seq );
  SCOPE();
  DONE();
}

static ast_t* match( parser_t* parser )
{
  // MATCH seq caseexpr* END
  AST_TOKEN();
  SCOPE();
  RULE( seq );
  LIST( { TK_PIPE, caseexpr } );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* whileloop( parser_t* parser )
{
  // WHILE seq DO expr
  AST_TOKEN();
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_DO );
  RULE( expr );
  DONE();
}

static ast_t* doloop( parser_t* parser )
{
  // DO seq WHILE expr
  AST_TOKEN();
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_WHILE );
  RULE( expr );
  DONE();
}

static ast_t* forloop( parser_t* parser )
{
  // FOR ID oftype IN seq DO expr
  // FIX: scope, or ast transformation?
  AST_TOKEN();
  EXPECT( TK_ID );
  RULE( oftype );
  EXPECT_DROP( TK_IN );
  RULE( seq );
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

static ast_t* try( parser_t* parser )
{
  // TRY seq (ELSE seq)? (THEN expr | END)
  AST_TOKEN();
  RULE( seq );
  SCOPE();

  if( ACCEPT_DROP( TK_ELSE ) )
  {
    RULE( seq );
    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  if( ACCEPT_DROP( TK_ELSE ) )
  {
    RULE( expr );
    SCOPE();
  } else {
    EXPECT( TK_END );
  }

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
    { TK_BREAK, consume },
    { TK_CONTINUE, consume },
    { TK_RETURN, returnexpr },
    { TK_TRY, try },
    { TK_THROW, consume },
    { TK_NONE, command }
    );
}

static ast_t* function( parser_t* parser )
{
  // (FUN | MSG) PRIVATE? THROW? ID typeparams mode params oftype (EQUALS seq)?
  AST_TOKEN();
  SCOPE();

  if( !ACCEPT( TK_PRIVATE ) )
  {
    INSERT( TK_NONE );
  }

  if( !ACCEPT( TK_THROW ) )
  {
    INSERT( TK_NONE );
  }

  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( mode );
  RULE( params );
  RULE( oftype );

  if( ACCEPT_DROP( TK_EQUALS ) )
  {
    RULE( seq );
    SCOPE();
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
  EXPECT_DROP( TK_COLON );
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
  // (TRAIT | CLASS | ACTOR) ID typeparams mode is (PRIVATE|INFER)? (field | function)*
  AST_TOKEN();
  SCOPE();
  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( mode );
  RULE( is );

  if( !ACCEPT( TK_PRIVATE, TK_INFER ) )
  {
    INSERT( TK_NONE );
  }

  LIST(
    { TK_VAR, field },
    { TK_VAL, field },
    { TK_FUN, function },
    { TK_MSG, function }
    );
  DONE();
}

static ast_t* alias( parser_t* parser )
{
  // ALIAS ID typeparams COLON type
  AST_TOKEN();
  SCOPE();
  EXPECT( TK_ID );
  RULE( typeparams );
  EXPECT_DROP( TK_COLON );
  RULE( type );
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
  SCOPE();
  LIST(
    { TK_USE, use },
    { TK_ALIAS, alias },
    { TK_TRAIT, class },
    { TK_CLASS, class },
    { TK_ACTOR, class }
    );
  EXPECT_DROP( TK_EOF );
  DONE();
}

// external API
ast_t* parse( source_t* source )
{
  // open the lexer
  lexer_t* lexer = lexer_open( source );
  if( lexer == NULL ) { return NULL; }

  // create a parser and attach the lexer
  parser_t* parser = calloc( 1, sizeof(parser_t) );
  parser->source = source;
  parser->lexer = lexer;
  parser->t = lexer_next( lexer );

  ast_t* ast = module( parser );

  if( ast != NULL )
  {
    ast_reverse( ast );
    ast_attach( ast, source );
  }

  lexer_close( lexer );
  token_free( parser->t );
  free( parser );

  return ast;
}
