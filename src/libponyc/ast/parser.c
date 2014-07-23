#include "parserapi.h"
#include <stdio.h>

// forward declarations
DECL(type);
DECL(rawseq);
DECL(seq);
DECL(term);
DECL(members);


// operator precedence
static int precedence(token_id id)
{
  switch(id)
  {
    // type operators
    case TK_ARROW:
      return 40;

    case TK_ISECTTYPE:
      return 30;

    case TK_UNIONTYPE:
      return 20;

    case TK_TUPLETYPE:
      return 10;

    // postfix operators
    case TK_DOT:
    case TK_BANG:
    case TK_QUALIFY:
    case TK_CALL:
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
    case TK_ISNT:
    case TK_EQ:
    case TK_NE:
      return 50;

    case TK_AND: return 40;
    case TK_XOR: return 30;
    case TK_OR: return 20;
    case TK_ASSIGN: return 10;

    default: return INT_MAX;
  }
}

static bool associativity(token_id id)
{
  // return true for right associative, false for left associative
  switch(id)
  {
    case TK_ASSIGN:
    case TK_ARROW:
      return true;

    default:
      return false;
  }
}


// rules

// type {COMMA type}
DEF(types);
  AST_NODE(TK_TYPES);
  RULE(type);
  WHILE(TK_COMMA, RULE(type));
  DONE();

// ID COLON type [ASSIGN seq]
DEF(param);
  AST_NODE(TK_PARAM);
  TOKEN(TK_ID);
  SKIP(TK_COLON);
  RULE(type);
  IF(TK_ASSIGN, RULE(seq));
  DONE();

// ID [COLON type] [ASSIGN type]
DEF(typeparam);
  AST_NODE(TK_TYPEPARAM);
  TOKEN(TK_ID);
  IF(TK_COLON, RULE(type));
  IF(TK_ASSIGN, RULE(type));
  DONE();

// param {COMMA param}
DEF(params);
  AST_NODE(TK_PARAMS);
  RULE(param);
  WHILE(TK_COMMA, RULE(param));
  DONE();

// LSQUARE typeparam {COMMA typeparam} RSQUARE
DEF(typeparams);
  AST_NODE(TK_TYPEPARAMS);
  SKIP(TK_LSQUARE);
  RULE(typeparam);
  WHILE(TK_COMMA, RULE(typeparam));
  SKIP(TK_RSQUARE);
  DONE();

// LSQUARE type {COMMA type} RSQUARE
DEF(typeargs);
  AST_NODE(TK_TYPEARGS);
  SKIP(TK_LSQUARE);
  RULE(type);
  WHILE(TK_COMMA, RULE(type));
  SKIP(TK_RSQUARE);
  DONE();

// BE [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN
DEF(betype);
  TOKEN(TK_BE);
  SCOPE();
  AST_NODE(TK_NONE);  // Capability
  OPT TOKEN(TK_ID);
  OPT RULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE(types);
  SKIP(TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  AST_NODE(TK_NONE);  // Partial
  AST_NODE(TK_NONE);  // Body
  DONE();

// FUN CAP [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN [COLON type]
// [QUESTION]
DEF(funtype);
  TOKEN(TK_FUN);
  SCOPE();
  TOKEN(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(TK_ID);
  OPT RULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE(types);
  SKIP(TK_RPAREN);
  IF(TK_COLON, RULE(type));
  OPT TOKEN(TK_QUESTION);
  AST_NODE(TK_NONE);  // Body
  DONE();

// {betype} {funtype}
DEF(funs);
  AST_NODE(TK_MEMBERS);
  SEQ(betype);
  SEQ(funtype);
  DONE();

// LBRACE funs RBRACE [cap] [HAT]
DEF(structural);
  AST_NODE(TK_STRUCTURAL);
  SCOPE();
  SKIP(TK_LBRACE);
  RULE(funs);
  SKIP(TK_RBRACE);
  OPT TOKEN(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(TK_HAT);
  DONE();

// ID [DOT ID] [typeargs]
DEF(nominal);
  AST_NODE(TK_NOMINAL);
  TOKEN(TK_ID);
  IF(TK_DOT, TOKEN(TK_ID));
  OPT RULE(typeargs);
  OPT TOKEN(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(TK_HAT);
  DONE();

// COMMA type {COMMA type}
DEF(tupletype);
  AST_NODE(TK_TUPLETYPE);
  SKIP(TK_COMMA);
  RULE(type);
  WHILE(TK_COMMA, RULE(type));
  DONE();

// PIPE type {PIPE type}
DEF(uniontype);
  AST_NODE(TK_UNIONTYPE);
  SKIP(TK_PIPE);
  RULE(type);
  WHILE(TK_PIPE, RULE(type));
  DONE();

// AMP type {AMP type}
DEF(isecttype);
  AST_NODE(TK_ISECTTYPE);
  SKIP(TK_AMP);
  RULE(type);
  WHILE(TK_AMP, RULE(type));
  DONE();

// (LPAREN | LPAREN_NEW) type {uniontype | isecttype | tupletype} RPAREN
DEF(typeexpr);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  RULE(type);
  BINDOP(uniontype, isecttype, tupletype);
  SKIP(TK_RPAREN);
  DONE();

// THIS
DEF(thistype);
  AST_NODE(TK_THISTYPE);
  SKIP(TK_THIS);
  DONE();

// ARROW (thistype | typeexpr | nominal | structural)
DEF(viewpoint);
  TOKEN(TK_ARROW);
  RULE(typeexpr, nominal, structural, thistype);
  DONE();

// (thistype | typeexpr | nominal | structural) {viewpoint}
DEF(type);
  RULE(thistype, typeexpr, nominal, structural);
  OPT BINDOP(viewpoint);
  DONE();

// term ASSIGN seq
DEF(namedarg);
  AST_NODE(TK_NAMEDARG);
  RULE(term);
  SKIP(TK_ASSIGN);
  RULE(seq);
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF(named);
  AST_NODE(TK_NAMEDARGS);
  SKIP(TK_WHERE);
  RULE(namedarg);
  WHILE(TK_COMMA, RULE(namedarg));
  DONE();

// seq {COMMA seq}
DEF(positional);
  AST_NODE(TK_POSITIONALARGS);
  RULE(seq);
  WHILE(TK_COMMA, RULE(seq));
  DONE();

// LBRACE [IS types] members RBRACE
DEF(object);
  AST_NODE(TK_OBJECT);
  SKIP(TK_LBRACE);
  IF(TK_IS, RULE(types));
  RULE(members);
  SKIP(TK_RBRACE);
  DONE();

// (LSQUARE | LSQUARE_NEW) [positional] [named] RSQUARE
DEF(array);
  AST_NODE(TK_ARRAY);
  SKIP(TK_LSQUARE, TK_LSQUARE_NEW);
  OPT RULE(positional);
  OPT RULE(named);
  SKIP(TK_RSQUARE);
  DONE();

// (LPAREN | LPAREN_NEW) seq {COMMA seq} RPAREN
DEF(tuple);
  AST_NODE(TK_TUPLE);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  RULE(seq);
  WHILE(TK_COMMA, RULE(seq));
  SKIP(TK_RPAREN);
  DONE();

// THIS | INT | FLOAT | STRING
DEF(literal);
  TOKEN(TK_THIS, TK_INT, TK_FLOAT, TK_STRING);
  DONE();

DEF(ref);
  AST_NODE(TK_REFERENCE);
  TOKEN(TK_ID);
  DONE();

// ref | literal | tuple | array | object
DEF(atom);
  RULE(ref, literal, tuple, array, object);
  DONE();

// DOT (ID | INT)
DEF(dot);
  TOKEN(TK_DOT);
  TOKEN(TK_ID, TK_INT);
  DONE();

// BANG ID
DEF(bang);
  TOKEN(TK_BANG);
  TOKEN(TK_ID);
  DONE();

// typeargs
DEF(qualify);
  AST_NODE(TK_QUALIFY);
  RULE(typeargs);
  DONE();

// LPAREN [positional] [named] RPAREN
DEF(call);
  AST_NODE(TK_CALL);
  SKIP(TK_LPAREN);
  OPT RULE(positional);
  OPT RULE(named);
  SKIP(TK_RPAREN);
  DONE();

// atom {dot | bang | qualify | call}
DEF(postfix);
  RULE(atom);
  OPT BINDOP(dot, bang, qualify, call);
  DONE();

// ID
DEF(idseqid);
  AST_NODE(TK_IDSEQ);
  TOKEN(TK_ID);
  DONE();

// (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseqseq);
  AST_NODE(TK_IDSEQ);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  TOKEN(TK_ID);
  WHILE(TK_COMMA, TOKEN(TK_ID));
  SKIP(TK_RPAREN);
  DONE();

// ID | (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseq);
  RULE(idseqid, idseqseq);
  DONE();

// (VAR | VAL) idseq [COLON type]
DEF(local);
  TOKEN(TK_VAR, TK_LET);
  RULE(idseq);
  IF(TK_COLON, RULE(type));
  DONE();

// ELSE seq END
DEF(elseclause);
  SKIP(TK_ELSE);
  RULE(seq);
  DONE();

// ELSEIF rawseq THEN seq
DEF(elseif);
  AST_NODE(TK_IF);
  SCOPE();
  SKIP(TK_ELSEIF);
  RULE(rawseq);
  SKIP(TK_THEN);
  RULE(seq);
  RULE(elseif, elseclause);
  DONE();

// IF rawseq THEN seq {elseif} [ELSE seq] END
DEF(cond);
  TOKEN(TK_IF);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_THEN);
  RULE(seq);
  OPT RULE(elseif, elseclause);
  SKIP(TK_END);
  DONE();

// AS idseq COLON type
DEF(as);
  TOKEN(TK_AS);
  RULE(idseq);
  SKIP(TK_COLON);
  RULE(type);
  DONE();

// PIPE [rawseq] [as] [WHERE rawseq] [ARROW seq]
DEF(caseexpr);
  AST_NODE(TK_CASE);
  SCOPE();
  SKIP(TK_PIPE);
  OPT RULE(rawseq);
  OPT RULE(as);
  IF(TK_WHERE, RULE(rawseq));
  IF(TK_DBLARROW, RULE(seq));
  DONE();

// {caseexpr}
DEF(cases);
  AST_NODE(TK_CASES);
  SEQ(caseexpr);
  DONE();

// MATCH rawseq cases [ELSE seq] END
DEF(match);
  TOKEN(TK_MATCH);
  SCOPE();
  RULE(rawseq);
  RULE(cases);
  IF(TK_ELSE, RULE(seq));
  SKIP(TK_END);
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF(whileloop);
  TOKEN(TK_WHILE);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_DO);
  RULE(seq);
  IF(TK_ELSE, RULE(seq));
  SKIP(TK_END);
  DONE();

// REPEAT rawseq UNTIL seq END
DEF(repeat);
  TOKEN(TK_REPEAT);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_UNTIL);
  RULE(seq);
  SKIP(TK_END);
  DONE();

// FOR idseq [COLON type] IN seq DO seq [ELSE seq] END
DEF(forloop);
  TOKEN(TK_FOR);
  SCOPE();
  RULE(idseq);
  IF(TK_COLON, RULE(type));
  SKIP(TK_IN);
  RULE(seq);
  SKIP(TK_DO);
  RULE(seq);
  IF(TK_ELSE, RULE(seq));
  SKIP(TK_END);
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF(try);
  TOKEN(TK_TRY);
  RULE(seq);
  IF(TK_ELSE, RULE(seq));
  IF(TK_THEN, RULE(seq));
  SKIP(TK_END);
  DONE();

// (NOT | CONSUME | RECOVER) term
DEF(prefix);
  TOKEN(TK_NOT, TK_CONSUME, TK_RECOVER);
  RULE(term);
  DONE();

// (MINUS | MINUS_NEW) term
DEF(prefixminus);
  AST_NODE(TK_UNARY_MINUS);
  SKIP(TK_MINUS, TK_MINUS_NEW);
  RULE(term);
  DONE();

// local | cond | match | whileloop | repeat | forloop | try | prefix |
//  prefixminus | postfix
DEF(term);
  RULE(local, cond, match, whileloop, repeat, forloop, try, prefix,
    prefixminus, postfix);
  DONE();

// BINOP term
DEF(binop);
  TOKEN(
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT,
    TK_ASSIGN
    );
  RULE(term);
  DONE();

// term {binop}
DEF(infix);
  RULE(term);
  OPT BINDOP(binop);
  DONE();

// (RETURN | BREAK) infix
DEF(returnexpr);
  TOKEN(TK_RETURN, TK_BREAK);
  RULE(infix);
  DONE();

// CONTINUE | ERROR | COMPILER_INTRINSIC
DEF(statement);
  TOKEN(TK_CONTINUE, TK_ERROR, TK_COMPILER_INTRINSIC);
  DONE();

// (statement | returnexpr | infix) [SEMI]
DEF(expr);
  RULE(statement, returnexpr, infix);
  OPT SKIP(TK_SEMI);
  DONE();

// expr {expr}
DEF(rawseq);
  AST_NODE(TK_SEQ);
  RULE(expr);
  SEQ(expr);
  DONE();

// expr {expr}
DEF(seq);
  AST_NODE(TK_SEQ);
  SCOPE();
  RULE(expr);
  SEQ(expr);
  DONE();

// FUN CAP ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN
// [COLON type] [QUESTION] [ARROW seq]
DEF(function);
  TOKEN(TK_FUN);
  SCOPE();
  TOKEN(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  TOKEN(TK_ID);
  OPT RULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE(params);
  SKIP(TK_RPAREN);
  IF(TK_COLON, RULE(type));
  OPT TOKEN(TK_QUESTION);
  IF(TK_DBLARROW, RULE(seq));
  DONE();

// BE ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [ARROW seq]
DEF(behaviour);
  TOKEN(TK_BE);
  SCOPE();
  AST_NODE(TK_TAG);
  TOKEN(TK_ID);
  OPT RULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE(params);
  SKIP(TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  AST_NODE(TK_NONE);  // Partial
  IF(TK_DBLARROW, RULE(seq));
  DONE();

// NEW ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [QUESTION]
// [ARROW seq]
DEF(constructor);
  TOKEN(TK_NEW);
  SCOPE();
  AST_NODE(TK_REF);
  OPT TOKEN(TK_ID);
  OPT RULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE(params);
  SKIP(TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  OPT TOKEN(TK_QUESTION);
  IF(TK_DBLARROW, RULE(seq));
  DONE();

// VAR ID [COLON type] [ASSIGN expr]
DEF(fieldvar);
  AST_NODE(TK_FVAR);
  SKIP(TK_VAR);
  TOKEN(TK_ID);
  IF(TK_COLON, RULE(type));
  IF(TK_ASSIGN, RULE(expr));
  DONE();

// VAL ID [COLON type] [ASSIGN expr]
DEF(fieldval);
  AST_NODE(TK_FLET);
  SKIP(TK_LET);
  TOKEN(TK_ID);
  IF(TK_COLON, RULE(type));
  IF(TK_ASSIGN, RULE(expr));
  DONE();

// {field} {constructor} {behaviour} {function}
DEF(members);
  AST_NODE(TK_MEMBERS);
  SEQ(fieldvar, fieldval);
  SEQ(constructor);
  SEQ(behaviour);
  SEQ(function);
  DONE();

// (TRAIT | DATA | CLASS | ACTOR) ID [typeparams] [CAP] [IS types] members
DEF(class);
  TOKEN(TK_TRAIT, TK_DATA, TK_CLASS, TK_ACTOR);
  SCOPE();
  TOKEN(TK_ID);
  OPT RULE(typeparams);
  OPT TOKEN(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  IF(TK_IS, RULE(types));
  RULE(members);
  DONE();

// TYPE ID IS type
DEF(typealias);
  TOKEN(TK_TYPE);
  TOKEN(TK_ID);
  SKIP(TK_IS);
  RULE(type);
  DONE();

// USE STRING [AS ID]
DEF(use);
  TOKEN(TK_USE);
  TOKEN(TK_STRING);
  IF(TK_AS, TOKEN(TK_ID));
  DONE();

// {use} {class | typealias}
DEF(module);
  AST_NODE(TK_MODULE);
  SCOPE();
  SEQ(use);
  SEQ(class, typealias);
  SKIP(TK_EOF);
  DONE();

// external API
ast_t* parser(source_t* source)
{
  return parse(source, module);
}
