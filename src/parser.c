#include "parser.h"
#include "parserapi.h"
#include "error.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

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
  // (LBRACE (ISO | TRN | VAR | VAL | BOX | TAG | THIS | ID) RBRACE)?
  // (ARROW (THIS | ID))? MULTIPLY?
  AST( TK_MODE );

  if( ACCEPT_DROP( TK_LBRACE ) )
  {
    EXPECT( TK_ISO, TK_TRN, TK_VAR, TK_VAL, TK_BOX, TK_TAG, TK_THIS, TK_ID );
    EXPECT_DROP( TK_RBRACE );
  }

  if( ACCEPT_DROP( TK_ARROW ) )
  {
    EXPECT( TK_THIS, TK_ID );
  } else {
    INSERT( TK_NONE );
  }

  if( ACCEPT_DROP( TK_MULTIPLY ) )
  {
    INSERT( TK_EPHEMERAL );
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
  // FUN mode QUESTION? LPAREN (type (COMMA type)*)? RPAREN oftype
  AST( TK_FUNTYPE );
  EXPECT_DROP( TK_FUN );
  RULE( mode );

  if( !ACCEPT( TK_QUESTION ) )
  {
    INSERT( TK_NONE );
  }

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

static ast_t* throwseq( parser_t* parser )
{
  // seq
  AST( TK_CANTHROW );
  RULE( seq );
  DONE();
}

static ast_t* throwexpr( parser_t* parser )
{
  // expr
  AST( TK_CANTHROW );
  RULE( expr );
  DONE();
}

static ast_t* reference( parser_t* parser )
{
  // ID
  AST( TK_REF );
  EXPECT( TK_ID );
  DONE();
}

static ast_t* primary( parser_t* parser )
{
  FORWARDALT(
    { TK_THIS, consume },
    { TK_INT, consume },
    { TK_FLOAT, consume },
    { TK_STRING, consume },
    { TK_ID, reference },
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
  AST( TK_LOCAL );
  EXPECT( TK_VAR, TK_VAL );
  EXPECT( TK_ID );
  RULE( oftype );
  EXPECT_DROP( TK_EQUALS );
  RULE( expr );
  DONE();
}

static ast_t* lambda( parser_t* parser )
{
  // FUN QUESTION? mode params oftype DBLARROW expr
  AST( TK_LAMBDA );
  SCOPE();
  EXPECT_DROP( TK_FUN );
  RULE( mode );

  bool throw = ACCEPT( TK_QUESTION );
  if( !throw ) { INSERT( TK_NONE ); }

  RULE( params );
  RULE( oftype );
  EXPECT_DROP( TK_DBLARROW );

  if( throw )
  {
    RULE( throwexpr );
  } else {
    RULE( expr );
  }

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
    // FIX: syntactic sugar for 'else None'
    EXPECT_DROP( TK_END );
    INSERT( TK_NONE );
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
  // PIPE binary? as (IF binary)? (DBLARROW seq)?
  AST( TK_CASE );
  EXPECT_DROP( TK_PIPE );

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

  if( ACCEPT_DROP( TK_DBLARROW ) )
  {
    RULE( seq );
    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

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

static ast_t* try( parser_t* parser )
{
  // TRY throwseq (ELSE seq)? (THEN seq)? END
  AST_TOKEN();
  RULE( throwseq );
  SCOPE();

  if( ACCEPT_DROP( TK_ELSE ) )
  {
    RULE( seq );
    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  if( ACCEPT_DROP( TK_THEN ) )
  {
    RULE( expr );
    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  EXPECT_DROP( TK_END );
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
  // BREAK | CONTINUE | RETURN | try | UNDEF | SEMI | command
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
    { TK_RETURN, consume },
    { TK_TRY, try },
    { TK_UNDEF, consume },
    { TK_NONE, command }
    );
}

static ast_t* function( parser_t* parser )
{
  // FUN mode QUESTION? ID typeparams params oftype (DBLARROW seq)?
  AST_TOKEN();
  SCOPE();
  RULE( mode );

  bool throw = ACCEPT( TK_QUESTION );
  if( !throw ) { INSERT( TK_NONE ); }

  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( params );
  RULE( oftype );

  if( ACCEPT_DROP( TK_DBLARROW ) )
  {
    if( throw )
    {
      RULE( throwseq );
    } else {
      RULE( seq );
    }

    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* behaviour( parser_t* parser )
{
  // BE ID typeparams params (DBLARROW seq)?
  AST_TOKEN();
  SCOPE();

  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( params );

  if( ACCEPT_DROP( TK_DBLARROW ) )
  {
    RULE( seq );
    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* constructor( parser_t* parser )
{
  // NEW QUESTION? ID typeparams params (DBLARROW seq)?
  AST_TOKEN();
  SCOPE();

  bool throw = ACCEPT( TK_QUESTION );
  if( !throw ) { INSERT( TK_NONE ); }

  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( params );

  if( ACCEPT_DROP( TK_DBLARROW ) )
  {
    if( throw )
    {
      RULE( throwseq );
    } else {
      RULE( seq );
    }

    SCOPE();
  } else {
    INSERT( TK_NONE );
  }

  DONE();
}

static ast_t* field( parser_t* parser )
{
  // (VAR | VAL) ID COLON type
  AST( TK_FIELD );
  EXPECT( TK_VAR, TK_VAL );
  EXPECT( TK_ID );
  EXPECT_DROP( TK_COLON );
  RULE( type );
  DONE();
}

static ast_t* members( parser_t* parser )
{
  // (field | function)*
  AST( TK_LIST );
  LIST(
    { TK_VAR, field },
    { TK_VAL, field },
    { TK_FUN, function },
    { TK_BE, behaviour },
    { TK_NEW, constructor }
    );
  DONE();
}

static ast_t* traits( parser_t* parser )
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
  // (TRAIT | CLASS | ACTOR) ID typeparams mode traits members
  AST_TOKEN();
  SCOPE();
  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( mode );
  RULE( traits );
  RULE( members );
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
  // (use | alias | trait | class | actor)*
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
