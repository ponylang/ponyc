#include "parser.h"
#include "parserapi.h"
#include "error.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

// forward declarations
static ast_t* type( parser_t* parser );
static ast_t* term( parser_t* parser );
static ast_t* members( parser_t* parser );
static ast_t* traits( parser_t* parser );

// helpers
static bool is_binop( parser_t* parser )
{
  return LOOK(
    TK_AND, TK_OR, TK_XOR, // logic
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD, // arithmetic
    TK_LSHIFT, TK_RSHIFT, // shift
    TK_IS, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT, // comparison
    TK_ASSIGN // assignment
    );
}

// rules
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
  // (LBRACE
  //  (ISO | TRN | VAR | VAL | BOX | TAG | THIS | ID)
  //  (ARROW (THIS | ID))? MULTIPLY?
  // RBRACE)?
  AST( TK_MODE );

  if( ACCEPT_DROP( TK_LBRACE ) )
  {
    EXPECT( TK_ISO, TK_TRN, TK_VAR, TK_VAL, TK_BOX, TK_TAG, TK_THIS, TK_ID );

    if( ACCEPT_DROP( TK_ARROW ) )
    {
      EXPECT( TK_THIS, TK_ID );
    } else {
      INSERT( TK_NONE );
    }

    if( !ACCEPT( TK_HAT ) )
    {
      INSERT( TK_NONE );
    }

    EXPECT_DROP( TK_RBRACE );
  }

  DONE();
}

static ast_t* params( parser_t* parser )
{
  // LPAREN (param (COMMA param)*)? RBRACKET
  AST( TK_PARAMS );
  BLOCK( TK_NONE, TK_LPAREN, TK_RPAREN, TK_COMMA,
    { TK_NONE, param }
    );
  DONE();
}

static ast_t* typeparams( parser_t* parser )
{
  // (LBRACKET typeparam (COMMA typeparam)* RBRACKET)?
  AST( TK_TYPEPARAMS );
  OPTBLOCK( TK_NONE, TK_LBRACKET, TK_RBRACKET, TK_COMMA,
    { TK_NONE, typeparam }
    );
  DONE();
}

static ast_t* typeargs( parser_t* parser )
{
  // (LBRACKET type (COMMA type)* RBRACKET)?
  AST( TK_TYPEARGS );
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
  // (nominal | structural | typedecl | typeexpr) HAT?
  AST( TK_)
  FORWARDALT(
    { TK_LPAREN, adttype },
    { TK_FUN, funtype },
    { TK_ID, objtype }
    );
}

// FIX: above here

static ast_t* namedarg( parser_t* parser )
{
  // term ASSIGN seq
  AST( TK_NAMEDARG );
  RULE( term );
  EXPECT_DROP( TK_ASSIGN );
  RULE( seq );
  DONE();
}

static ast_t* named( parser_t* parser )
{
  // WHERE term ASSIGN seq (COMMA term ASSIGN seq)*
  AST( TK_NAMEDARGS );
  EXPECT_DROP( TK_WHERE );
  RULE( namedarg );
  WHILERULE( TK_COMMA, namedarg );
  DONE();
}

static ast_t* positional( parser_t* parser )
{
  // seq (COMMA seq)*)
  AST( TK_POSITIONALARGS );
  RULE( seq );
  WHILERULE( TK_COMMA, seq );
  DONE();
}

static ast_t* object( parser_t* parser )
{
  // LBRACE traits? members? RBRACE
  AST( TK_OBJECT );
  EXPECT_DROP( TK_LBRACE );
  OPTRULE( traits );
  OPTRULE( members );
  EXPECT_DROP( TK_RBRACE );
  DONE();
}

static ast_t* array( parser_t* parser )
{
  // LBRACKET positional? named? RBRACKET
  AST( TK_ARRAY );
  EXPECT_DROP( TK_LBRACKET );
  OPTRULE( positional );
  OPTRULE( named );
  EXPECT_DROP( TK_RBRACKET );
  DONE();
}

static ast_t* tuple( parser_t* parser )
{
  // LPAREN positional? named? RPAREN
  AST( TK_TUPLE );
  EXPECT_DROP( TK_LPAREN );
  OPTRULE( positional );
  OPTRULE( named );
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

static ast_t* atom( parser_t* parser )
{
  if( LOOK( TK_THIS, TK_INT, TK_FLOAT, TK_STRING, TK_ID ) )
  {
    FORWARD( consume );
  }

  FORWARD( tuple, array, object );
}

static ast_t* bang( parser_t* parser )
{
  AST_TOKEN( TK_BANG );
  EXPECT( TK_ID );
  DONE();
}

static ast_t* dot( parser_t* parser )
{
  AST_TOKEN( TK_DOT );
  EXPECT( TK_ID, TK_INT );
  DONE();
}

static ast_t* postfix( parser_t* parser )
{
  // atom (dot | bang | typeargs | tuple)*
  AST_RULE( atom );
  SEQRULE( dot, bang, typeargs, tuple );
  DONE();
}

static ast_t* unary( parser_t* parser )
{
  AST_TOKEN( TK_NOT, TK_MINUS );
  RULE( term );
  DONE();
}

static ast_t* local( parser_t* parser )
{
  // (VAR | VAL) ID (COLON type)?
  AST_TOKEN( TK_VAR, TK_VAL );
  EXPECT( TK_ID );
  IFRULE( TK_COLON, type );
  DONE();
}

static ast_t* elseif( parser_t* parser_t )
{
  // ELSEIF seq THEN seq
  AST_TOKEN( TK_ELSEIF );
  RULE( seq );
  EXPECT_DROP( TK_THEN );
  RULE( seq );
  DONE();
}

static ast_t* cond( parser_t* parser )
{
  // IF seq THEN seq elseif* (ELSE seq)? END
  AST_TOKEN( TK_IF );
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_THEN );
  RULE( seq );
  SCOPE();
  WHILERULE( TK_ELSEIF, elseif );
  IFRULE( TK_ELSE, seq );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* as( parser_t* parser_t )
{
  // AS idseq ':' type
  AST_TOKEN( TK_AS );
  RULE( idseq );
  EXPECT_DROP( TK_COLON );
  RULE( type );
  DONE();
}

static ast_t* caseexpr( parser_t* parser )
{
  // PIPE seq? as? (WHERE seq)? (ARROW seq)?
  AST( TK_CASE );
  EXPECT_DROP( TK_PIPE );
  SCOPE();
  OPTRULE( seq );
  OPTRULE( as );
  IFRULE( TK_WHERE, seq );
  IFRULE( TK_ARROW, seq );
  SCOPE();
  DONE();
}

static ast_t* match( parser_t* parser )
{
  // MATCH seq caseexpr* (ELSE seq)? END
  AST_TOKEN( TK_MATCH );
  SCOPE();
  RULE( seq );
  SEQRULE( caseexpr );
  IFRULE( TK_ELSE, seq );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* whileloop( parser_t* parser )
{
  // WHILE seq DO seq (ELSE seq)? END
  AST_TOKEN( TK_WHILE );
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_DO );
  RULE( seq );
  IFRULE( TK_ELSE, seq );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* doloop( parser_t* parser )
{
  // DO seq WHILE seq END
  AST_TOKEN( TK_DO );
  SCOPE();
  RULE( seq );
  EXPECT_DROP( TK_WHILE );
  RULE( seq );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* forloop( parser_t* parser )
{
  // FOR idseq (COLON type)? IN seq DO seq (ELSE seq)? END
  AST_TOKEN( TK_FOR );
  RULE( idseq );
  IFRULE( TK_COLON, type );
  EXPECT_DROP( TK_IN );
  RULE( seq );
  SCOPE();
  EXPECT_DROP( TK_DO );
  RULE( seq );
  IFRULE( TK_ELSE, seq );
  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* try( parser_t* parser )
{
  // TRY throwseq (ELSE seq)? (THEN seq)? END
  AST_TOKEN( TK_TRY );
  RULE( throwseq );
  SCOPE();

  IFRULE( TK_ELSE, seq );
  IFRULE( TK_THEN, seq );

  EXPECT_DROP( TK_END );
  DONE();
}

static ast_t* consumeexpr( parser_t* parser )
{
  // CONSUME term
  AST_TOKEN( TK_CONSUME );
  RULE( term );
  DONE();
}

static ast_t* recoverexpr( parser_t* parser )
{
  // RECOVER term
  AST_TOKEN( TK_RECOVER );
  RULE( term );
  DONE();
}

static ast_t* term( parser_t* parser )
{
  // local | cond | match | whileloop | doloop | forloop | try | consume |
  // recover | unary | postfix
  FORWARD( local, cond, match, whileloop, doloop, forloop, try, consumeexpr,
    recoverexpr, unary, postfix );
}

static ast_t* binary( parser_t* parser )
{
  // term (binop term)*
  AST_RULE( term );

  while( is_binop( parser ) )
  {
    RULE( term );
  }

  DONE();
}

static ast_t* returnexpr( parser_t* parser )
{
  // RETURN binary
  AST_TOKEN( TK_RETURN );
  RULE( binary );
  DONE();
}

static ast_t* breakexpr( parser_t* parser )
{
  // BREAK binary
  AST_TOKEN( TK_BREAK );
  RULE( binary );
  DONE();
}

static ast_t* expr( parser_t* parser )
{
  // CONTINUE | UNDEF | return | break | binary
  if( LOOK( TK_CONTINUE, TK_UNDEF ) )
  {
    FORWARD( consume );
  }

  FORWARD( returnexpr, breakexpr, binary );
}

static ast_t* seq( parser_t* parser )
{
  // expr (SEMI expr)*
  AST( TK_SEQ );
  RULE( expr );
  WHILERULE( TK_SEMI, expr );
  DONE();
}

static ast_t* function( parser_t* parser )
{
  // FUN RAW_CAP ID typeparams? params (COLON type)? QUESTION? (ARROW seq)?
  AST_TOKEN( TK_FUN );
  SCOPE();

  EXPECT( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG );
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  RULE( params );
  IFRULE( TK_COLON, type );

  bool throw = ACCEPT( TK_QUESTION );
  if( !throw ) { INSERT( TK_NONE ); }

  if( ACCEPT_DROP( TK_ARROW ) )
  {
    if( throw )
    {
      RULE( throwseq );
    } else {
      RULE( seq );
    }
  } else {
    INSERT( TK_NONE );
  }

  SCOPE();
  DONE();
}

static ast_t* behaviour( parser_t* parser )
{
  // BE ID typeparams params (ARROW seq)?
  AST_TOKEN( TK_BE );
  SCOPE();
  EXPECT( TK_ID );
  RULE( typeparams );
  RULE( params );
  IFRULE( TK_ARROW, seq );
  SCOPE();
  DONE();
}

static ast_t* constructor( parser_t* parser )
{
  // NEW ID typeparams? params QUESTION? (ARROW seq)?
  AST_TOKEN( TK_NEW );
  SCOPE();

  EXPECT( TK_ID );
  OPTRULE( typeparams );
  RULE( params );

  bool throw = ACCEPT( TK_QUESTION );
  if( !throw ) { INSERT( TK_NONE ); }

  if( ACCEPT_DROP( TK_ARROW ) )
  {
    if( throw )
    {
      RULE( throwseq );
    } else {
      RULE( seq );
    }
  } else {
    INSERT( TK_NONE );
  }

  SCOPE();
  DONE();
}

static ast_t* field( parser_t* parser )
{
  // (VAR | VAL) ID (COLON type)? (ASSIGN seq)?
  AST_TOKEN( TK_VAR, TK_VAL );
  EXPECT( TK_ID );
  IFRULE( TK_COLON, type );
  IFRULE( TK_ASSIGN, seq );
  DONE();
}

static ast_t* members( parser_t* parser )
{
  AST( TK_MEMBERS );
  SEQRULE( field, constructor, function, behaviour );
  DONE();
}

static ast_t* traits( parser_t* parser )
{
  // IS type (COMMA type)*
  AST_TOKEN( TK_IS );
  RULE( type );
  WHILERULE( TK_COMMA, type );
  DONE();
}

static ast_t* class( parser_t* parser )
{
  // (TRAIT | CLASS | ACTOR) ID typeparams? RAW_CAP? traits? members
  AST_TOKEN( TK_TRAIT, TK_CLASS, TK_ACTOR );
  SCOPE();
  EXPECT( TK_ID );
  OPTRULE( typeparams );

  if( !ACCEPT( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG ) )
  {
    INSERT( TK_MUT );
  }

  OPTRULE( traits );
  RULE( members );
  DONE();
}

static ast_t* typedecl( parser_t* parser )
{
  // TYPE ID typeparams? (COLON type)?
  AST_TOKEN( TK_TYPE );
  SCOPE();
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  IFRULE( TK_COLON, type );
  DONE();
}

static ast_t* use( parser_t* parser )
{
  // USE (ID ASSIGN)? STRING
  AST_TOKEN( TK_USE );

  if( ACCEPT( TK_ID ) )
  {
    EXPECT_DROP( TK_ASSIGN );
  } else {
    INSERT( TK_NONE );
  }

  EXPECT( TK_STRING );
  DONE();
}

static ast_t* module( parser_t* parser )
{
  // (use | typedecl | trait | class | actor)*
  AST( TK_MODULE );
  SCOPE();
  SEQRULE( use, typedecl, class );
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
