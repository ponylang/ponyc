#include "parserapi.h"
#include <stdio.h>

// forward declarations
DECL( typedecl );
DECL( typeexpr );
DECL( type );
DECL( rawseq );
DECL( seq );
DECL( term );
DECL( members );

// operator precedence
static int precedence( token_id id )
{
  switch( id )
  {
    // type operators
    case TK_PIPE:
      return 20;

    case TK_COMMA:
      return 10;

    // postfix operators
    case TK_DOT:
    case TK_BANG:
    case TK_LBRACKET:
    case TK_LBRACE:
      return 100;

    // infix operators
    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
      return 90;

    case TK_PLUS:
    case TK_MINUS:
      return 80;

    case TK_LSHIFT:
    case TK_RSHIFT:
      return 70;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      return 60;

    case TK_IS:
    case TK_EQ:
    case TK_NE:
      return 50;

    case TK_AND: return 40;
    case TK_XOR: return 30;
    case TK_OR: return 20;
    case TK_ASSIGN: return 10;

    default: return 0;
  }
}

// rules

// type {COMMA type}
DEF( types );
  AST( TK_TYPES );
  RULE( type );
  WHILERULE( TK_COMMA, type );
  DONE();

// ID [COLON type] [ASSIGN seq]
DEF( param );
  CHECK( TK_ID );
  AST( TK_PARAM );
  EXPECT( TK_ID );
  IFRULE( TK_COLON, type );
  IFRULE( TK_ASSIGN, seq );
  DONE();

// ID [COLON type] [ASSIGN seq]
DEF( typeparam );
  CHECK( TK_ID );
  AST( TK_TYPEPARAM );
  EXPECT( TK_ID );
  IFRULE( TK_COLON, type );
  IFRULE( TK_ASSIGN, seq );
  DONE();

// param {COMMA param}
DEF( params );
  AST( TK_PARAMS );
  RULE( param );
  WHILERULE( TK_COMMA, param );
  DONE();

// LBRACKET typeparam {COMMA typeparam} RBRACKET
DEF( typeparams );
  SKIP( TK_LBRACKET );
  AST( TK_TYPEPARAMS );
  RULE( typeparam );
  WHILERULE( TK_COMMA, typeparam );
  SKIP( TK_RBRACKET );
  DONE();

// LBRACKET type {COMMA type} RBRACKET
DEF( typeargs );
  SKIP( TK_LBRACKET );
  AST( TK_TYPEARGS );
  RULE( type );
  WHILERULE( TK_COMMA, type );
  SKIP( TK_RBRACKET );
  DONE();

// BE [ID] [typeparams] LPAREN [types] RPAREN
DEF( betype );
  SKIP( TK_BE );
  AST( TK_BETYPE );
  INSERT( TK_NONE );
  OPTIONAL( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( types );
  SKIP( TK_RPAREN );
  INSERT( TK_NONE );
  INSERT( TK_NONE );
  DONE();

// FUN RAW_CAP [ID] [typeparams] LPAREN [types] RPAREN [COLON type] [QUESTION]
DEF( funtype );
  SKIP( TK_FUN );
  AST( TK_FUNTYPE );
  EXPECT( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG );
  OPTIONAL( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( types );
  SKIP( TK_RPAREN );
  IFRULE( TK_COLON, type );
  OPTIONAL( TK_QUESTION );
  DONE();

// NEW [ID] [typeparams] LPAREN [types] RPAREN [QUESTION]
DEF( newtype );
  SKIP( TK_NEW );
  AST( TK_NEWTYPE );
  INSERT( TK_NONE );
  OPTIONAL( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( types );
  INSERT( TK_NONE );
  SKIP( TK_RPAREN );
  OPTIONAL( TK_QUESTION );
  DONE();

// LBRACE {newtype | funtype | betype} RBRACE [CAP]
DEF( structural );
  SKIP( TK_LBRACE );
  AST( TK_STRUCTURAL );
  SEQRULE( newtype, funtype, betype );
  SKIP( TK_RBRACE );
  OPTIONAL( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG, TK_ID, TK_THIS );
  DONE();

// (ID | THIS) {DOT (ID | THIS)}
DEF( typename );
  CHECK( TK_ID, TK_THIS );
  AST( TK_TYPENAME );
  EXPECT( TK_ID, TK_THIS );
  WHILETOKEN( TK_DOT, TK_ID, TK_THIS );
  DONE();

// typename [typeargs] [CAP]
DEF( nominal );
  CHECK( TK_ID, TK_THIS );
  AST( TK_NOMINAL );
  RULE( typename );
  OPTRULE( typeargs );
  OPTIONAL( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG, TK_ID, TK_THIS );
  DONE();

// typeexpr | nominal | structural | typedecl
DEF( typebase );
  AST_RULE( typeexpr, nominal, structural, typedecl );
  DONE();

// COMMA typebase
DEF( tupletype );
  SKIP( TK_COMMA );
  AST( TK_TUPLETYPE );
  RULE( typebase );
  DONE();

// PIPE typebase
DEF( uniontype );
  SKIP( TK_PIPE );
  AST( TK_UNIONTYPE );
  RULE( typebase );
  DONE();

// LPAREN typebase {uniontype | tupletype} RPAREN [CAP]
DEF( typeexpr );
  SKIP( TK_LPAREN );
  AST_RULE( typebase );
  BINDOP( uniontype, tupletype );
  SKIP( TK_RPAREN );
  OPTIONAL( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG, TK_ID, TK_THIS );
  DONE();

// typebase [HAT]
DEF( type );
  AST( TK_TYPE );
  RULE( typebase );
  OPTIONAL( TK_HAT );
  DONE();

// term ASSIGN seq
DEF( namedarg );
  AST( TK_NAMEDARG );
  RULE( term );
  SKIP( TK_ASSIGN );
  RULE( seq );
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF( named );
  SKIP( TK_WHERE );
  AST( TK_NAMEDARGS );
  RULE( namedarg );
  WHILERULE( TK_COMMA, namedarg );
  DONE();

// seq {COMMA seq}
DEF( positional );
  AST( TK_POSITIONALARGS );
  RULE( seq );
  WHILERULE( TK_COMMA, seq );
  DONE();

// LBRACE [IS types] members RBRACE
DEF( object );
  EXPECT( TK_LBRACE );
  AST( TK_OBJECT );
  IFRULE( TK_IS, types );
  RULE( members );
  SKIP( TK_RBRACE );
  DONE();

// LBRACKET [positional] [named] RBRACKET
DEF( array );
  EXPECT( TK_LBRACKET );
  AST( TK_ARRAY );
  OPTRULE( positional );
  OPTRULE( named );
  SKIP( TK_RBRACKET );
  DONE();

// LPAREN [positional] [named] RPAREN
DEF( tuple );
  EXPECT( TK_LPAREN );
  AST( TK_TUPLE );
  OPTRULE( positional );
  OPTRULE( named );
  SKIP( TK_RPAREN );
  DONE();

// THIS | INT | FLOAT | STRING | ID
DEF( literal );
  AST_TOKEN( TK_THIS, TK_INT, TK_FLOAT, TK_STRING, TK_ID );
  DONE();

// literal | tuple | array | object
DEF( atom );
  AST_RULE( literal, tuple, array, object );
  DONE();

// DOT (ID | INT)
DEF( dot );
  AST_TOKEN( TK_DOT );
  EXPECT( TK_ID, TK_INT );
  DONE();

// BANG ID
DEF( bang );
  AST_TOKEN( TK_BANG );
  EXPECT( TK_ID );
  DONE();

// typeargs
DEF( qualify );
  CHECK( TK_LBRACKET );
  AST( TK_QUALIFY );
  RULE( typeargs );
  DONE();

// call
DEF( call );
  CHECK( TK_LPAREN );
  AST( TK_CALL );
  RULE( tuple );
  DONE();

// atom {dot | bang | qualify | call}
DEF( postfix );
  AST_RULE( atom );
  BINDOP( dot, bang, qualify, call );
  DONE();

// ID | LPAREN ID {COMMA ID} RPAREN
DEF( idseq );
  CHECK( TK_ID, TK_LPAREN );
  AST( TK_IDSEQ );

  if( LOOK( TK_LPAREN ) )
  {
    SKIP( TK_LPAREN );
    EXPECT( TK_ID );
    WHILETOKEN( TK_COMMA, TK_ID );
    SKIP( TK_RPAREN );
  } else {
    EXPECT( TK_ID );
  }

  DONE();

// (VAR | VAL) idseq [COLON type]
DEF( local );
  AST_TOKEN( TK_VAR, TK_VAL );
  RULE( idseq );
  IFRULE( TK_COLON, type );
  DONE();

// ELSEIF rawseq THEN seq (elseif | [ELSE seq] END)
DEF( elseif );
  SKIP( TK_ELSEIF );
  AST( TK_IF );
  SCOPE();
  RULE( rawseq );
  SKIP( TK_THEN );
  RULE( seq );

  if( LOOK( TK_ELSEIF ) )
  {
    RULE( elseif );
  } else {
    IFRULE( TK_ELSE, seq );
    SKIP( TK_END );
  }

  DONE();

// IF rawseq THEN seq (elseif | [ELSE seq] END)
DEF( cond );
  AST_TOKEN( TK_IF );
  SCOPE();
  RULE( rawseq );
  SKIP( TK_THEN );
  RULE( seq );

  if( LOOK( TK_ELSEIF ) )
  {
    RULE( elseif );
  } else {
    IFRULE( TK_ELSE, seq );
    SKIP( TK_END );
  }

  DONE();

// AS idseq COLON type
DEF( as );
  AST_TOKEN( TK_AS );
  RULE( idseq );
  SKIP( TK_COLON );
  RULE( type );
  DONE();

// PIPE [seq] [as] [WHERE seq] [ARROW seq]
DEF( caseexpr );
  SKIP( TK_PIPE );
  AST( TK_CASE );
  OPTRULE( seq );
  OPTRULE( as );
  IFRULE( TK_WHERE, seq );
  IFRULE( TK_ARROW, seq );
  DONE();

// MATCH rawseq {caseexpr} [ELSE seq] END
DEF( match );
  AST_TOKEN( TK_MATCH );
  SCOPE();
  RULE( rawseq );
  SEQRULE( caseexpr );
  IFRULE( TK_ELSE, seq );
  SKIP( TK_END );
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF( whileloop );
  AST_TOKEN( TK_WHILE );
  SCOPE();
  RULE( rawseq );
  SKIP( TK_DO );
  RULE( seq );
  IFRULE( TK_ELSE, seq );
  SKIP( TK_END );
  DONE();

// DO rawseq WHILE seq END
DEF( doloop );
  AST_TOKEN( TK_DO );
  SCOPE();
  RULE( rawseq );
  SKIP( TK_WHILE );
  RULE( seq );
  SKIP( TK_END );
  DONE();

// FOR idseq [COLON type] IN seq DO seq [ELSE seq] END
DEF( forloop );
  AST_TOKEN( TK_FOR );
  RULE( idseq );
  IFRULE( TK_COLON, type );
  SKIP( TK_IN );
  RULE( seq );
  SKIP( TK_DO );
  RULE( seq );
  IFRULE( TK_ELSE, seq );
  SKIP( TK_END );
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF( try );
  AST_TOKEN( TK_TRY );
  RULE( seq );
  IFRULE( TK_ELSE, seq );
  IFRULE( TK_THEN, seq );
  SKIP( TK_END );
  DONE();

// (NOT | MINUS | CONSUME | RECOVER) term
DEF( prefix );
  AST_TOKEN( TK_NOT, TK_MINUS, TK_CONSUME, TK_RECOVER );
  RULE( term );
  DONE();

// local | cond | match | whileloop | doloop | forloop | try | prefix | postfix
DEF( term );
  AST_RULE( local, cond, match, whileloop, doloop, forloop, try, prefix, postfix );
  DONE();

// BINOP term
DEF( binop );
  AST_TOKEN(
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT,
    TK_ASSIGN
    );
  RULE( term );
  DONE();

// term {binop}
DEF( infix );
  AST_RULE( term );
  BINDOP( binop );
  DONE();

// (BREAK | RETURN) infix
DEF( breakexpr );
  AST_TOKEN( TK_BREAK, TK_RETURN );
  RULE( infix );
  DONE();

// CONTINUE | UNDEF
DEF( statement );
  AST_TOKEN( TK_CONTINUE, TK_UNDEF );
  DONE();

// statement | breakexpr | infix
DEF( expr );
  AST_RULE( statement, breakexpr, infix );
  DONE();

// expr {SEMI expr}
DEF( rawseq );
  AST( TK_SEQ );
  RULE( expr );
  WHILERULE( TK_SEMI, expr );
  DONE();

// expr {SEMI expr}
DEF( seq );
  AST( TK_SEQ );
  SCOPE();
  RULE( expr );
  WHILERULE( TK_SEMI, expr );
  DONE();

// FUN RAW_CAP ID [typeparams] LPAREN [params] RPAREN [COLON type] [QUESTION]
// [ARROW seq]
DEF( function );
  AST_TOKEN( TK_FUN );
  SCOPE();
  EXPECT( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG );
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( params );
  SKIP( TK_RPAREN );
  IFRULE( TK_COLON, type );
  OPTIONAL( TK_QUESTION );
  IFRULE( TK_ARROW, seq );
  DONE();

// BE ID [typeparams] LPAREN [params] RPAREN [ARROW seq]
DEF( behaviour );
  AST_TOKEN( TK_BE );
  SCOPE();
  INSERT( TK_NONE );
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( params );
  SKIP( TK_RPAREN );
  INSERT( TK_NONE );
  INSERT( TK_NONE );
  IFRULE( TK_ARROW, seq );
  DONE();

// NEW ID [typeparams] LPAREN [params] RPAREN [QUESTION] [ARROW seq]
DEF( constructor );
  AST_TOKEN( TK_NEW );
  SCOPE();
  INSERT( TK_NONE );
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  SKIP( TK_LPAREN );
  OPTRULE( params );
  SKIP( TK_RPAREN );
  INSERT( TK_NONE );
  OPTIONAL( TK_QUESTION );
  IFRULE( TK_ARROW, seq );
  DONE();

// (VAR | VAL) ID [COLON type] [ASSIGN seq]
DEF( field );
  AST_TOKEN( TK_VAR, TK_VAL );
  EXPECT( TK_ID );
  IFRULE( TK_COLON, type );
  IFRULE( TK_ASSIGN, seq );
  DONE();

// {field | constructor | function | behaviour}
DEF( members );
  AST( TK_MEMBERS );
  SEQRULE( field, constructor, function, behaviour );
  DONE();

// (TRAIT | CLASS | ACTOR) ID [typeparams] [RAW_CAP] [IS types] members
DEF( class );
  AST_TOKEN( TK_TRAIT, TK_CLASS, TK_ACTOR );
  SCOPE();
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  OPTIONAL( TK_ISO, TK_TRN, TK_MUT, TK_IMM, TK_BOX, TK_TAG );
  IFRULE( TK_IS, types );
  RULE( members );
  DONE();

// TYPE ID [typeparams] [COLON (typeexpr | nominal | structural | typedecl)]
DEF( typedecl );
  EXPECT( TK_TYPE );
  AST( TK_TYPEDECL );
  SCOPE();
  EXPECT( TK_ID );
  OPTRULE( typeparams );
  IFRULE( TK_COLON, typeexpr, nominal, structural, typedecl );
  DONE();

// USE STRING [AS ID]
DEF( use );
  AST_TOKEN( TK_USE );
  EXPECT( TK_STRING );
  IFTOKEN( TK_AS, TK_ID );
  DONE();

// {use | typedecl | trait | class | actor}
DEF( module );
  AST( TK_MODULE );
  SCOPE();
  SEQRULE( use, typedecl, class );
  SKIP( TK_EOF );
  DONE();

// external API
ast_t* parser( source_t* source )
{
  return parse( source, module );
}
