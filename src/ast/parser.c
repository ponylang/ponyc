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

    default: return 0;
  }
}

// rules

// type {COMMA type}
DEF(types);
  AST(TK_TYPES);
  RULE(type);
  WHILERULE(TK_COMMA, type);
  DONE();

// ID [COLON type] [ASSIGN seq]
DEF(param);
  CHECK(TK_ID, TK_ID_NEW);
  AST(TK_PARAM);
  EXPECT(TK_ID, TK_ID_NEW);
  IFRULE(TK_COLON, type);
  IFRULE(TK_ASSIGN, seq);
  DONE();

// ID [COLON type] [ASSIGN type]
DEF(typeparam);
  CHECK(TK_ID, TK_ID_NEW);
  AST(TK_TYPEPARAM);
  EXPECT(TK_ID, TK_ID_NEW);
  IFRULE(TK_COLON, type);
  IFRULE(TK_ASSIGN, type);
  DONE();

// param {COMMA param}
DEF(params);
  AST(TK_PARAMS);
  RULE(param);
  WHILERULE(TK_COMMA, param);
  DONE();

// LBRACKET typeparam {COMMA typeparam} RBRACKET
DEF(typeparams);
  SKIP(TK_LBRACKET);
  AST(TK_TYPEPARAMS);
  RULE(typeparam);
  WHILERULE(TK_COMMA, typeparam);
  SKIP(TK_RBRACKET);
  DONE();

// LBRACKET type {COMMA type} RBRACKET
DEF(typeargs);
  SKIP(TK_LBRACKET);
  AST(TK_TYPEARGS);
  RULE(type);
  WHILERULE(TK_COMMA, type);
  SKIP(TK_RBRACKET);
  DONE();

// NEW [ID] [typeparams] (LPAREN | TK_LPAREN_NEW) [types] RPAREN [QUESTION]
DEF(newtype);
  AST_TOKEN(TK_NEW);
  SCOPE();
  INSERT(TK_NONE);
  OPTIONAL(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(types);
  INSERT(TK_NONE);
  SKIP(TK_RPAREN);
  OPTIONAL(TK_QUESTION);
  INSERT(TK_NONE);
  DONE();

// BE [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN
DEF(betype);
  AST_TOKEN(TK_BE);
  SCOPE();
  INSERT(TK_NONE);
  OPTIONAL(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(types);
  SKIP(TK_RPAREN);
  INSERT(TK_NONE);
  INSERT(TK_NONE);
  DONE();

// FUN CAP [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN [COLON type]
// [QUESTION]
DEF(funtype);
  AST_TOKEN(TK_FUN);
  SCOPE();
  EXPECT(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPTIONAL(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(types);
  SKIP(TK_RPAREN);
  IFRULE(TK_COLON, type);
  OPTIONAL(TK_QUESTION);
  INSERT(TK_NONE);
  DONE();

// (LBRACE | LBRACE_NEW) {newtype | funtype | betype} RBRACE
DEF(structural);
  SKIP(TK_LBRACE, TK_LBRACE_NEW);
  AST(TK_STRUCTURAL);
  SCOPE();
  SEQRULE(newtype, funtype, betype);
  SKIP(TK_RBRACE);
  DONE();

// ID [DOT ID] [typeargs]
DEF(nominal);
  CHECK(TK_ID, TK_ID_NEW);
  AST(TK_NOMINAL);
  EXPECT(TK_ID, TK_ID_NEW);
  IFTOKEN(TK_DOT, TK_ID, TK_ID_NEW);
  OPTRULE(typeargs);
  DONE();

// COMMA type
DEF(tupletype);
  SKIP(TK_COMMA);
  AST(TK_TUPLETYPE);
  RULE(type);
  DONE();

// PIPE type
DEF(uniontype);
  SKIP(TK_PIPE);
  AST(TK_UNIONTYPE);
  RULE(type);
  DONE();

// AMP type
DEF(isecttype);
  SKIP(TK_AMP);
  AST(TK_ISECTTYPE);
  RULE(type);
  DONE();

// (LPAREN | LPAREN_NEW) type {uniontype | isecttype | tupletype} RPAREN
DEF(typeexpr);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  AST_RULE(type);
  EXPECTBINDOP(uniontype, isecttype, tupletype);
  SKIP(TK_RPAREN);
  DONE();

// TODO: add typedecl for "enum" types?
// typeexpr | nominal | structural
DEF(typebase);
  AST_RULE(typeexpr, nominal, structural);
  DONE();

// typebase [CAP] [HAT]
DEF(type);
  AST(TK_TYPEDEF);
  RULE(typebase);
  OPTIONAL(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPTIONAL(TK_HAT);
  INSERT(TK_NONE);
  // TODO: viewpoint
  DONE();

// term ASSIGN seq
DEF(namedarg);
  AST(TK_NAMEDARG);
  RULE(term);
  SKIP(TK_ASSIGN);
  RULE(seq);
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF(named);
  SKIP(TK_WHERE);
  AST(TK_NAMEDARGS);
  RULE(namedarg);
  WHILERULE(TK_COMMA, namedarg);
  DONE();

// seq {COMMA seq}
DEF(positional);
  AST(TK_POSITIONALARGS);
  RULE(seq);
  WHILERULE(TK_COMMA, seq);
  DONE();

// (LBRACE | LBRACE_NEW) [IS types] members RBRACE
DEF(object);
  SKIP(TK_LBRACE, TK_LBRACE_NEW);
  AST(TK_OBJECT);
  IFRULE(TK_IS, types);
  RULE(members);
  SKIP(TK_RBRACE);
  DONE();

// (LBRACKET | LBRACKET_NEW) [positional] [named] RBRACKET
DEF(array);
  SKIP(TK_LBRACKET, TK_LBRACKET_NEW);
  AST(TK_ARRAY);
  OPTRULE(positional);
  OPTRULE(named);
  SKIP(TK_RBRACKET);
  DONE();

// (LPAREN | LPAREN_NEW) seq {COMMA seq} RPAREN
DEF(tuple);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  AST(TK_TUPLE);
  RULE(seq);
  WHILERULE(TK_COMMA, seq);
  SKIP(TK_RPAREN);
  DONE();

// THIS | INT | FLOAT | STRING
DEF(literal);
  AST_TOKEN(TK_THIS, TK_INT, TK_FLOAT, TK_STRING);
  DONE();

DEF(ref);
  CHECK(TK_ID, TK_ID_NEW);
  AST(TK_REFERENCE);
  EXPECT(TK_ID, TK_ID_NEW);
  DONE();

// ref | literal | tuple | array | object
DEF(atom);
  AST_RULE(ref, literal, tuple, array, object);
  DONE();

// DOT (ID | INT)
DEF(dot);
  AST_TOKEN(TK_DOT);
  EXPECT(TK_ID, TK_ID_NEW, TK_INT);
  DONE();

// BANG ID
DEF(bang);
  AST_TOKEN(TK_BANG);
  EXPECT(TK_ID, TK_ID_NEW);
  DONE();

// typeargs
DEF(qualify);
  CHECK(TK_LBRACKET);
  AST(TK_QUALIFY);
  RULE(typeargs);
  DONE();

// LPAREN [positional] [named] RPAREN
DEF(call);
  SKIP(TK_LPAREN);
  AST(TK_CALL);
  OPTRULE(positional);
  OPTRULE(named);
  SKIP(TK_RPAREN);
  DONE();

// atom {dot | bang | qualify | call}
DEF(postfix);
  AST_RULE(atom);
  BINDOP(dot, bang, qualify, call);
  DONE();

// ID | (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseq);
  CHECK(TK_ID, TK_ID_NEW, TK_LPAREN, TK_LPAREN_NEW);
  AST(TK_IDSEQ);

  if(LOOK(TK_LPAREN, TK_LPAREN_NEW))
  {
    SKIP(TK_LPAREN, TK_LPAREN_NEW);
    EXPECT(TK_ID, TK_ID_NEW);
    WHILETOKEN(TK_COMMA, TK_ID, TK_ID_NEW);
    SKIP(TK_RPAREN);
  } else {
    EXPECT(TK_ID, TK_ID_NEW);
  }

  DONE();

// (VAR | VAL) idseq [COLON type]
DEF(local);
  AST_TOKEN(TK_VAR, TK_LET);
  RULE(idseq);
  IFRULE(TK_COLON, type);
  DONE();

// ELSEIF rawseq THEN seq (elseif | [ELSE seq] END)
DEF(elseif);
  SKIP(TK_ELSEIF);
  AST(TK_IF);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_THEN);
  RULE(seq);

  if(LOOK(TK_ELSEIF))
  {
    RULE(elseif);
  } else {
    IFRULE(TK_ELSE, seq);
    SKIP(TK_END);
  }

  DONE();

// IF rawseq THEN seq (elseif | [ELSE seq] END)
DEF(cond);
  AST_TOKEN(TK_IF);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_THEN);
  RULE(seq);

  if(LOOK(TK_ELSEIF))
  {
    RULE(elseif);
  } else {
    IFRULE(TK_ELSE, seq);
    SKIP(TK_END);
  }

  DONE();

// AS idseq COLON type
DEF(as);
  AST_TOKEN(TK_AS);
  RULE(idseq);
  SKIP(TK_COLON);
  RULE(type);
  DONE();

// PIPE [seq] [as] [WHERE seq] [ARROW seq]
DEF(caseexpr);
  SKIP(TK_PIPE);
  AST(TK_CASE);
  SCOPE();
  OPTRULE(seq);
  OPTRULE(as);
  IFRULE(TK_WHERE, seq);
  IFRULE(TK_ARROW, seq);
  DONE();

// {caseexpr}
DEF(cases);
  AST(TK_CASES);
  SEQRULE(caseexpr);
  DONE();

// MATCH rawseq cases [ELSE seq] END
DEF(match);
  AST_TOKEN(TK_MATCH);
  SCOPE();
  RULE(rawseq);
  RULE(cases);
  IFRULE(TK_ELSE, seq);
  SKIP(TK_END);
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF(whileloop);
  AST_TOKEN(TK_WHILE);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_DO);
  RULE(seq);
  IFRULE(TK_ELSE, seq);
  SKIP(TK_END);
  DONE();

// REPEAT rawseq UNTIL seq END
DEF(repeat);
  AST_TOKEN(TK_REPEAT);
  SCOPE();
  RULE(rawseq);
  SKIP(TK_UNTIL);
  RULE(seq);
  SKIP(TK_END);
  DONE();

// FOR idseq [COLON type] IN seq DO seq [ELSE seq] END
DEF(forloop);
  AST_TOKEN(TK_FOR);
  SCOPE();
  RULE(idseq);
  IFRULE(TK_COLON, type);
  SKIP(TK_IN);
  RULE(seq);
  SKIP(TK_DO);
  RULE(seq);
  IFRULE(TK_ELSE, seq);
  SKIP(TK_END);
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF(try);
  AST_TOKEN(TK_TRY);
  RULE(seq);
  IFRULE(TK_ELSE, seq);
  IFRULE(TK_THEN, seq);
  SKIP(TK_END);
  DONE();

// (NOT | MINUS | MINUS_NEW | CONSUME | RECOVER) term
DEF(prefix);
  AST_TOKEN(TK_NOT, TK_MINUS, TK_MINUS_NEW, TK_CONSUME, TK_RECOVER);
  RULE(term);
  DONE();

// local | cond | match | whileloop | repeat | forloop | try | prefix | postfix
DEF(term);
  AST_RULE(local, cond, match, whileloop, repeat, forloop, try, prefix,
    postfix);
  DONE();

// BINOP term
DEF(binop);
  AST_TOKEN(
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
  AST_RULE(term);
  BINDOP(binop);
  DONE();

// RETURN infix
DEF(returnexpr);
  AST_TOKEN(TK_RETURN);
  RULE(infix);
  DONE();

// BREAK | CONTINUE | ERROR
DEF(statement);
  AST_TOKEN(TK_BREAK, TK_CONTINUE, TK_ERROR);
  DONE();

// (statement | returnexpr | infix) [SEMI]
DEF(expr);
  AST_RULE(statement, returnexpr, infix);
  ACCEPT_DROP(TK_SEMI);
  DONE();

// expr {expr}
DEF(rawseq);
  AST(TK_SEQ);
  RULE(expr);
  SEQRULE(expr);
  DONE();

// expr {expr}
DEF(seq);
  AST(TK_SEQ);
  SCOPE();
  RULE(expr);
  SEQRULE(expr);
  DONE();

// FUN CAP ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN
// [COLON type] [QUESTION] [ARROW seq]
DEF(function);
  AST_TOKEN(TK_FUN);
  SCOPE();
  EXPECT(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPTIONAL(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(params);
  SKIP(TK_RPAREN);
  IFRULE(TK_COLON, type);
  OPTIONAL(TK_QUESTION);
  IFRULE(TK_ARROW, seq);
  DONE();

// BE ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [ARROW seq]
DEF(behaviour);
  AST_TOKEN(TK_BE);
  SCOPE();
  INSERT(TK_NONE);
  EXPECT(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(params);
  SKIP(TK_RPAREN);
  INSERT(TK_NONE);
  INSERT(TK_NONE);
  IFRULE(TK_ARROW, seq);
  DONE();

// NEW ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [QUESTION]
// [ARROW seq]
DEF(constructor);
  AST_TOKEN(TK_NEW);
  SCOPE();
  INSERT(TK_NONE);
  OPTIONAL(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  SKIP(TK_LPAREN, TK_LPAREN_NEW);
  OPTRULE(params);
  SKIP(TK_RPAREN);
  INSERT(TK_NONE);
  OPTIONAL(TK_QUESTION);
  IFRULE(TK_ARROW, seq);
  DONE();

// VAR ID [COLON type] [ASSIGN expr]
DEF(fieldvar);
  SKIP(TK_VAR);
  AST(TK_FVAR);
  EXPECT(TK_ID, TK_ID_NEW);
  IFRULE(TK_COLON, type);
  IFRULE(TK_ASSIGN, expr);
  DONE();

// VAL ID [COLON type] [ASSIGN expr]
DEF(fieldval);
  SKIP(TK_LET);
  AST(TK_FLET);
  EXPECT(TK_ID, TK_ID_NEW);
  IFRULE(TK_COLON, type);
  IFRULE(TK_ASSIGN, expr);
  DONE();

// {field} {constructor} {behaviour} {function}
DEF(members);
  AST(TK_MEMBERS);
  SEQRULE(fieldvar, fieldval);
  SEQRULE(constructor);
  SEQRULE(behaviour);
  SEQRULE(function);
  DONE();

// (TYPE | TRAIT | CLASS | ACTOR) ID [typeparams] [CAP] [IS types] members
DEF(class);
  AST_TOKEN(TK_TYPE, TK_TRAIT, TK_CLASS, TK_ACTOR);
  SCOPE();
  EXPECT(TK_ID, TK_ID_NEW);
  OPTRULE(typeparams);
  OPTIONAL(TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  IFRULE(TK_IS, types);
  RULE(members);
  DONE();

// USE STRING [AS ID]
DEF(use);
  AST_TOKEN(TK_USE);
  EXPECT(TK_STRING);
  IFTOKEN(TK_AS, TK_ID, TK_ID_NEW);
  DONE();

// {use} {class}
DEF(module);
  AST(TK_MODULE);
  SCOPE();
  SEQRULE(use);
  SEQRULE(class);
  SKIP(TK_EOF);
  DONE();

// external API
ast_t* parser(source_t* source)
{
  return parse(source, module);
}
