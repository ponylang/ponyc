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
  RULE("type", type);
  WHILE(TK_COMMA, RULE("type", type));
  DONE();

// ID COLON type [ASSIGN seq]
DEF(param);
  AST_NODE(TK_PARAM);
  TOKEN("name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("parameter type", type);
  IF(TK_ASSIGN, RULE("default value", seq));
  DONE();

// ID [COLON type] [ASSIGN type]
DEF(typeparam);
  AST_NODE(TK_TYPEPARAM);
  TOKEN("name", TK_ID);
  IF(TK_COLON, RULE("type constraint", type));
  IF(TK_ASSIGN, RULE("default type", type));
  DONE();

// param {COMMA param}
DEF(params);
  AST_NODE(TK_PARAMS);
  RULE("parameter", param);
  WHILE(TK_COMMA, RULE("parameter", param));
  DONE();

// LSQUARE typeparam {COMMA typeparam} RSQUARE
DEF(typeparams);
  AST_NODE(TK_TYPEPARAMS);
  SKIP(NULL, TK_LSQUARE);
  RULE("type parameter", typeparam);
  WHILE(TK_COMMA, RULE("type parameter", typeparam));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// LSQUARE type {COMMA type} RSQUARE
DEF(typeargs);
  AST_NODE(TK_TYPEARGS);
  SKIP(NULL, TK_LSQUARE);
  RULE("type argument", type);
  WHILE(TK_COMMA, RULE("type argument", type));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// BE [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN
DEF(betype);
  TOKEN(NULL, TK_BE);
  SCOPE();
  AST_NODE(TK_NONE);  // Capability
  OPT TOKEN("name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", types);
  SKIP(NULL, TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  AST_NODE(TK_NONE);  // Partial
  AST_NODE(TK_NONE);  // Body
  DONE();

// FUN CAP [ID] [typeparams] (LPAREN | LPAREN_NEW) [types] RPAREN [COLON type]
// [QUESTION]
DEF(funtype);
  TOKEN(NULL, TK_FUN);
  SCOPE();
  TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN("name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", types);
  SKIP(NULL, TK_RPAREN);
  IF(TK_COLON, RULE("return type", type));
  OPT TOKEN(NULL, TK_QUESTION);
  AST_NODE(TK_NONE);  // Body
  DONE();

// {betype} {funtype}
DEF(funs);
  AST_NODE(TK_MEMBERS);
  SEQ("behaviour types", betype);
  SEQ("function types", funtype);
  DONE();

// LBRACE funs RBRACE [cap] [HAT]
DEF(structural);
  AST_NODE(TK_STRUCTURAL);
  SCOPE();
  SKIP(NULL, TK_LBRACE);
  RULE("functions", funs);
  SKIP(NULL, TK_RBRACE);
  OPT TOKEN("capabality", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(NULL, TK_HAT);
  DONE();

// ID [DOT ID] [typeargs]
DEF(nominal);
  AST_NODE(TK_NOMINAL);
  TOKEN("name", TK_ID);
  IF(TK_DOT, TOKEN("name", TK_ID));
  OPT RULE("type arguments", typeargs);
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(NULL, TK_HAT);
  DONE();

// COMMA type {COMMA type}
DEF(tupletype);
  AST_NODE(TK_TUPLETYPE);
  SKIP(NULL, TK_COMMA);
  RULE("type", type);
  WHILE(TK_COMMA, RULE("type", type));
  DONE();

// PIPE type {PIPE type}
DEF(uniontype);
  AST_NODE(TK_UNIONTYPE);
  SKIP(NULL, TK_PIPE);
  RULE("type", type);
  WHILE(TK_PIPE, RULE("type", type));
  DONE();

// AMP type {AMP type}
DEF(isecttype);
  AST_NODE(TK_ISECTTYPE);
  SKIP(NULL, TK_AMP);
  RULE("type", type);
  WHILE(TK_AMP, RULE("type", type));  // TODO: Fix this, it's ambiguous
  DONE();

// (LPAREN | LPAREN_NEW) type {uniontype | isecttype | tupletype} RPAREN
DEF(typeexpr);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("type", type);
  BINDOP("compound type", uniontype, isecttype, tupletype);
  SKIP(NULL, TK_RPAREN);
  DONE();

// THIS
DEF(thistype);
  AST_NODE(TK_THISTYPE);
  SKIP(NULL, TK_THIS);
  DONE();

// ARROW (thistype | typeexpr | nominal | structural)
DEF(viewpoint);
  TOKEN(NULL, TK_ARROW);
  RULE("viewpoint", typeexpr, nominal, structural, thistype);
  DONE();

// (thistype | typeexpr | nominal | structural) {viewpoint}
DEF(type);
  RULE("type", thistype, typeexpr, nominal, structural);
  OPT BINDOP("viewpoint", viewpoint);
  DONE();

// term ASSIGN rawseq
DEF(namedarg);
  AST_NODE(TK_NAMEDARG);
  RULE("argument name", term);
  SKIP(NULL, TK_ASSIGN);
  RULE("argument value", rawseq);
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF(named);
  AST_NODE(TK_NAMEDARGS);
  SKIP(NULL, TK_WHERE);
  RULE("named argument", namedarg);
  WHILE(TK_COMMA, RULE("named argument", namedarg));
  DONE();

// rawseq {COMMA rawseq}
DEF(positional);
  AST_NODE(TK_POSITIONALARGS);
  RULE("argument", rawseq);
  WHILE(TK_COMMA, RULE("argument", rawseq));
  DONE();

// LBRACE [IS types] members RBRACE
DEF(object);
  AST_NODE(TK_OBJECT);
  SKIP(NULL, TK_LBRACE);
  IF(TK_IS, RULE("provided type", types));
  RULE("object member", members);
  SKIP(NULL, TK_RBRACE);
  DONE();

// (LSQUARE | LSQUARE_NEW) [positional] [named] RSQUARE
DEF(array);
  AST_NODE(TK_ARRAY);
  SKIP(NULL, TK_LSQUARE, TK_LSQUARE_NEW);
  OPT RULE("array element", positional);
  OPT RULE("array element", named);
  SKIP(NULL, TK_RSQUARE);
  DONE();

// (LPAREN | LPAREN_NEW) rawseq {COMMA rawseq} RPAREN
DEF(tuple);
  AST_NODE(TK_TUPLE);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("value", rawseq);
  WHILE(TK_COMMA, RULE("value", rawseq));
  SKIP(NULL, TK_RPAREN);
  DONE();

// THIS | INT | FLOAT | STRING
DEF(literal);
  TOKEN("literal", TK_THIS, TK_INT, TK_FLOAT, TK_STRING);
  DONE();

DEF(ref);
  AST_NODE(TK_REFERENCE);
  TOKEN("name", TK_ID);
  DONE();

// AT ID typeargs (LPAREN | LPAREN_NEW) [positional] RPAREN
DEF(ffi);
  TOKEN(NULL, TK_AT);
  TOKEN("ffi name", TK_ID);
  RULE("return type", typeargs);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("ffi arguments", positional);
  SKIP(NULL, TK_RPAREN);
  DONE();

// ref | literal | tuple | array | object | ffi
DEF(atom);
  RULE("value", ref, literal, tuple, array, object, ffi);
  DONE();

// DOT (ID | INT)
DEF(dot);
  TOKEN(NULL, TK_DOT);
  TOKEN("member name", TK_ID, TK_INT);
  DONE();

// BANG ID
DEF(bang);
  TOKEN(NULL, TK_BANG);
  TOKEN("method name", TK_ID);
  DONE();

// typeargs
DEF(qualify);
  AST_NODE(TK_QUALIFY);
  RULE("type arguments", typeargs);
  DONE();

// LPAREN [positional] [named] RPAREN
DEF(call);
  AST_NODE(TK_CALL);
  SKIP(NULL, TK_LPAREN);
  OPT RULE("argument", positional);
  OPT RULE("argument", named);
  SKIP(NULL, TK_RPAREN);
  DONE();

// atom {dot | bang | qualify | call}
DEF(postfix);
  RULE("value", atom);
  OPT BINDOP("postfix expression", dot, bang, qualify, call);
  DONE();

// ID
DEF(idseqid);
  AST_NODE(TK_IDSEQ);
  TOKEN("variable name", TK_ID);
  DONE();

// (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseqseq);
  AST_NODE(TK_IDSEQ);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  TOKEN("variable name", TK_ID);
  WHILE(TK_COMMA, TOKEN("variable name", TK_ID));
  SKIP(NULL, TK_RPAREN);
  DONE();

// ID | (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseq);
  RULE("variable name", idseqid, idseqseq);
  DONE();

// (VAR | VAL) idseq [COLON type]
DEF(local);
  TOKEN(NULL, TK_VAR, TK_LET);
  RULE("variable name", idseq);
  IF(TK_COLON, RULE("variable type", type));
  DONE();

// ELSE seq END
DEF(elseclause);
  SKIP(NULL, TK_ELSE);
  RULE("else value", seq);
  DONE();

// ELSEIF rawseq THEN seq
DEF(elseif);
  AST_NODE(TK_IF);
  SCOPE();
  SKIP(NULL, TK_ELSEIF);
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  DONE();

// IF rawseq THEN seq {elseif} [ELSE seq] END
DEF(cond);
  TOKEN(NULL, TK_IF);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  SKIP(NULL, TK_END);
  DONE();

// AS idseq COLON type
DEF(as);
  TOKEN(NULL, TK_AS);
  RULE("variable name", idseq);
  SKIP(NULL, TK_COLON);
  RULE("match type", type);
  DONE();

// PIPE [rawseq] [as] [WHERE rawseq] [ARROW rawseq]
DEF(caseexpr);
  AST_NODE(TK_CASE);
  SCOPE();
  SKIP(NULL, TK_PIPE);
  OPT RULE("match value", rawseq);
  OPT RULE("as clause", as);
  IF(TK_WHERE, RULE("guard expression", rawseq));
  IF(TK_DBLARROW, RULE("case body", rawseq));
  DONE();

// {caseexpr}
DEF(cases);
  AST_NODE(TK_CASES);
  SEQ("cases", caseexpr);
  DONE();

// MATCH rawseq cases [ELSE seq] END
DEF(match);
  TOKEN(NULL, TK_MATCH);
  SCOPE();
  RULE("match expression", rawseq);
  RULE("cases", cases);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF(whileloop);
  TOKEN(NULL, TK_WHILE);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_DO);
  RULE("while body", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// REPEAT rawseq UNTIL rawseq END
DEF(repeat);
  TOKEN(NULL, TK_REPEAT);
  SCOPE();
  RULE("repeat body", rawseq);
  SKIP(NULL, TK_UNTIL);
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_END);
  DONE();

// FOR idseq [COLON type] IN seq DO seq [ELSE seq] END
DEF(forloop);
  TOKEN(NULL, TK_FOR);
  SCOPE();
  RULE("iterator name", idseq);
  IF(TK_COLON, RULE("iterator type", type));
  SKIP(NULL, TK_IN);
  RULE("sequence", seq);
  SKIP(NULL, TK_DO);
  RULE("for body", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF(try_block);
  TOKEN(NULL, TK_TRY);
  RULE("try body", seq);
  IF(TK_ELSE, RULE("try else body", seq));
  IF(TK_THEN, RULE("try then body", seq));
  SKIP(NULL, TK_END);
  DONE();

// (NOT | CONSUME | RECOVER) term
DEF(prefix);
  TOKEN("prefix", TK_NOT, TK_CONSUME, TK_RECOVER);
  RULE("expression", term);
  DONE();

// (MINUS | MINUS_NEW) term
DEF(prefixminus);
  AST_NODE(TK_UNARY_MINUS);
  SKIP(NULL, TK_MINUS, TK_MINUS_NEW);
  RULE("value", term);
  DONE();

// local | cond | match | whileloop | repeat | forloop | try | prefix |
//  prefixminus | postfix
DEF(term);
  RULE("value", local, cond, match, whileloop, repeat, forloop, try_block,
    prefix, prefixminus, postfix);
  DONE();

// BINOP term
DEF(binop);
  TOKEN("binary operator",
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT,
    TK_ASSIGN
    );
  RULE("value", term);
  DONE();

// term {binop}
DEF(infix);
  RULE("value", term);
  OPT BINDOP("value", binop);
  DONE();

// (RETURN | BREAK) infix
DEF(returnexpr);
  TOKEN(NULL, TK_RETURN, TK_BREAK);
  RULE("return value", infix);
  DONE();

// CONTINUE | ERROR | COMPILER_INTRINSIC
DEF(statement);
  TOKEN("statement", TK_CONTINUE, TK_ERROR, TK_COMPILER_INTRINSIC);
  DONE();

// (statement | returnexpr | infix) [SEMI]
DEF(expr);
  RULE("statement", statement, returnexpr, infix);
  OPT SKIP(NULL, TK_SEMI);
  DONE();

// expr {expr}
DEF(rawseq);
  AST_NODE(TK_SEQ);
  RULE("value", expr);
  SEQ("value", expr);
  DONE();

// expr {expr}
DEF(seq);
  AST_NODE(TK_SEQ);
  SCOPE();
  RULE("value", expr);
  SEQ("value", expr);
  DONE();

// FUN CAP ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN
// [COLON type] [QUESTION] [ARROW rawseq]
DEF(function);
  TOKEN(NULL, TK_FUN);
  SCOPE();
  TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  TOKEN("function name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  IF(TK_COLON, RULE("return type", type));
  OPT TOKEN(NULL, TK_QUESTION);
  IF(TK_DBLARROW, RULE("function body", rawseq));
  DONE();

// BE ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [ARROW rawseq]
DEF(behaviour);
  TOKEN(NULL, TK_BE);
  SCOPE();
  AST_NODE(TK_NONE);  // Capability
  TOKEN("behaviour name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  AST_NODE(TK_NONE);  // Partial
  IF(TK_DBLARROW, RULE("behaviour body", rawseq));
  DONE();

// NEW ID [typeparams] (LPAREN | LPAREN_NEW) [params] RPAREN [QUESTION]
// [ARROW rawseq]
DEF(constructor);
TOKEN(NULL, TK_NEW);
  SCOPE();
  AST_NODE(TK_NONE);  // Capability
  OPT TOKEN("constructor name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  AST_NODE(TK_NONE);  // Return type
  OPT TOKEN(NULL, TK_QUESTION);
  IF(TK_DBLARROW, RULE("constructor body", rawseq));
  DONE();

// (VAR | VAL) ID [COLON type] [ASSIGN expr]
DEF(field);
  TOKEN(NULL, TK_VAR, TK_LET);
  MAP_ID(TK_VAR, TK_FVAR);
  MAP_ID(TK_LET, TK_FLET);
  TOKEN("field name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("field type", type);
  IF(TK_ASSIGN, RULE("field value", expr));
  DONE();

// {field} {constructor} {behaviour} {function}
DEF(members);
  AST_NODE(TK_MEMBERS);
  SEQ("field", field);
  SEQ("constructor", constructor);
  SEQ("behaviour", behaviour);
  SEQ("function", function);
  DONE();

// (TRAIT | PRIMITIVE | CLASS | ACTOR) ID [typeparams] [CAP] [IS types] members
DEF(class_def);
  TOKEN("entity", TK_TRAIT, TK_PRIMITIVE, TK_CLASS, TK_ACTOR);
  SCOPE();
  TOKEN("entity name", TK_ID);
  OPT RULE("type parameters", typeparams);
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  IF(TK_IS, RULE("provided types", types));
  RULE("members", members);
  DONE();

// TYPE ID IS type
DEF(typealias);
  TOKEN(NULL, TK_TYPE);
  TOKEN("type name", TK_ID);
  SKIP(NULL, TK_IS);
  RULE("aliased type", type);
  DONE();

// USE STRING [AS ID]
DEF(use);
  TOKEN(NULL, TK_USE);
  TOKEN("URL", TK_STRING);
  IF(TK_AS, TOKEN("use alias", TK_ID));
  DONE();

// {use} {class | typealias}
DEF(module);
  AST_NODE(TK_MODULE);
  SCOPE();
  SEQ("use command", use);
  SEQ("entity definition", class_def, typealias);
  SKIP(NULL, TK_EOF);
  DONE();

// external API
ast_t* parser(source_t* source)
{
  return parse(source, module);
}
