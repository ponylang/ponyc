#ifndef PARSERAPI_H
#define PARSERAPI_H

#include "lexer.h"
#include "ast.h"
#include <stdbool.h>
#include <limits.h>

typedef struct parser_t
{
  source_t* source;
  lexer_t* lexer;
  token_t* t;
} parser_t;

/// State of parsing current rule
typedef struct rule_state_t
{
  bool matched;   // Has the rule matched yet
  bool opt;       // Is the next sub item optional
  bool scope;     // Is this rule a scope
  bool deferred;  // Do we have a deferred AST node
  token_id deferred_ast;  // ID of deferred AST node
  token_t deferred_token; // Token to base deferred AST node on
} rule_state_t;

#define PARSE_ERROR     ((ast_t*)1)   // A parse error has occured
#define RULE_NOT_FOUND  ((ast_t*)2)   // Sub item was not found

typedef int (*prec_t)(token_id id);

typedef bool (*assoc_t)(token_id id);

typedef ast_t* (*rule_t)(parser_t* parser);

/*
ast_t* consume(parser_t* parser);

void insert(parser_t* parser, token_id id, ast_t* ast);

bool look(parser_t* parser, const token_id* id);

bool accept(parser_t* parser, const token_id* id, ast_t* ast);

ast_t* rulealt(parser_t* parser, const rule_t* alt, ast_t* ast, bool opt);

ast_t* bindop(parser_t* parser, prec_t prec, assoc_t assoc, ast_t* ast,
  const rule_t* alt);

void syntax_error(parser_t* parser_t, const char* func, int line);

void scope(ast_t* ast);
*/


// Functions used by macros

ast_t* consume_token(parser_t* parser);

void consume_token_no_ast(parser_t* parser);

bool consume_if_match_no_ast(parser_t* parser, token_id id);

void process_deferred_ast(ast_t** rule_ast, rule_state_t* state);

void add_ast(ast_t* new_ast, ast_t** rule_ast, rule_state_t* state);

void add_deferrable_ast(parser_t* parser, token_id id, ast_t** ast,
  rule_state_t* state);

void add_infix_ast(ast_t* new_ast, ast_t* prev_ast, ast_t** rule_ast,
  prec_t prec, assoc_t assoc);

bool sub_result(parser_t* parser, ast_t** rule_ast, rule_state_t* state,
  ast_t* sub_ast, const char* function, int line_no);

void make_default_ast(parser_t* parser, ast_t** ast, token_id id);

ast_t* token_in_set(parser_t* parser, const token_id* id_set, bool make_ast);

ast_t* rule_in_set(parser_t* parser, const rule_t* rule_set);

void apply_scope(ast_t* ast);

void syntax_error(parser_t* parser, const char* func, int line);


// Worker macros

#define HANDLE_ERRORS(sub_ast) \
  { \
    if(sub_result(parser, &ast, &state, sub_ast, __FUNCTION__, __LINE__)) \
      return ast; \
  }


#define ERROR() \
  { \
    syntax_error(parser, __FUNCTION__, __LINE__); \
    ast_free(ast); \
    return PARSE_ERROR; \
  }


/* This is the only external API call.
 * Returns generated AST (which may be NULL) or PARSE_ERROR on error.
 */
ast_t* parse(source_t* source, rule_t start);


/* The API for parser rules starts here */

/// Rule forward declaration
#define DECL(rule) \
  static ast_t* rule(parser_t* parser)


/// Rule definition
#define DEF(rule) \
  static ast_t* rule(parser_t* parser) \
  { \
    ast_t* ast = NULL; \
    rule_state_t state = { false, false, false, false };


/** Add a node to our AST.
 * The first node in a rule is the parent and all subsequent nodes are children
 * of the parent.
 * In the interests of efficiency when an initial node created by this macro
 * the creation of that node is deferred until the rule has matched.
 * Example:
 *    AST_NODE(TK_CASE);
 */
#define AST_NODE(ID)  add_deferrable_ast(parser, ID, &ast, &state);


/** Specify that the containing rule is a scope.
 * May appear anywhere within the rule definition.
 * Example:
 *    SCOPE();
 */
#define SCOPE()   state.scope = true;


/** Specify that the following token or rule is optional.
 * If an optional token or rule does not match (but not a skip) then a default
 * TK_NONE node is created instead.
 * Example:
 *    OPT TOKEN(TK_FOO);
 */
#define OPT state.opt = true;


/** Attempt to match one of the given set of tokens.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * Example:
 *    TOKEN(TK_PLUS, TK_MINUS, TK_FOO);
 */
#define TOKEN(...) \
  { \
    static const token_id id_set[] = { __VA_ARGS__, TK_NONE }; \
    ast_t* sub_ast = token_in_set(parser, id_set, true); \
    HANDLE_ERRORS(sub_ast); \
    make_default_ast(parser, &ast, TK_NONE); \
    add_ast(sub_ast, &ast, &state) ; \
  }


/** Attempt to match one of the given set of tokens, but do not generate an AST
 * node for it.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * Example:
 *    SKIP(TK_PLUS, TK_MINUS, TK_FOO);
 */
#define SKIP(...) \
  { \
    static const token_id id_set[] = { __VA_ARGS__, TK_NONE }; \
    ast_t* sub_ast = token_in_set(parser, id_set, false); \
    HANDLE_ERRORS(sub_ast); \
  }


/** Attempt to match one of the given set of rules.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * Example:
 *    RULE(constructor, behaviour, function);
 */
#define RULE(...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    ast_t* sub_ast = rule_in_set(parser, rule_set); \
    HANDLE_ERRORS(sub_ast); \
    make_default_ast(parser, &sub_ast, TK_NONE); \
    add_ast(sub_ast, &ast, &state); \
  }


/** If the next token is the specified id consume it and parse the specified
 * block of tokens and / or rules.
 * Example:
 *    IF(TK_COLON, RULE(type));
 */
#define IF(id, body) \
  { \
    if(consume_if_match_no_ast(parser, id)) \
    { \
      body; \
    } \
    else \
    { \
      add_ast(ast_new(parser->t, TK_NONE), &ast, &state); \
    } \
  }


/** Repeatedly try to parse one optional token or rule and every time it
 * succeeds parse the specified block of tokens and / or rules.
 * Example:
 *    WHILE(TK_COLON, RULE(type));
 */
#define WHILE(id, body) \
  while(consume_if_match_no_ast(parser, id)) \
  { \
    body; \
  }


/** Repeatedly try to parse any of the given set of rules as long as it
 * succeeds.
 * Example:
 *    SEQ(TK_CLASS, TK_USE);
 */
#define SEQ(...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    while(true) \
    { \
      ast_t* sub_ast = rule_in_set(parser, rule_set); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      HANDLE_ERRORS(sub_ast); \
      add_ast(sub_ast, &ast, &state); \
    } \
  }


/** Repeatedly try to parse any of the given set of binary infix rules as long
 * as it succeeds and build AST with precedence rules.
 * If OPT is not specified then at least one match is required or a syntax
 * error occurs.
 * Example:
 *    BINDOP(uniontype, tupletype);
 */
#define BINDOP(...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    ast_t* prev_ast = ast; \
    ast_t* orig_ast = ast; \
    while(true) \
    { \
      ast_t* sub_ast = rule_in_set(parser, rule_set); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      if(sub_ast == NULL) ERROR(); /* Should this be an assert? */ \
      HANDLE_ERRORS(sub_ast); \
      add_infix_ast(sub_ast, prev_ast, &ast, precedence, associativity); \
      prev_ast = sub_ast; \
    } \
    if(!state.opt && orig_ast == ast) ERROR(); \
  }


/// Must appear at the end of each defined rule
#define DONE() \
    process_deferred_ast(&ast, &state); \
    if(state.scope) apply_scope(ast); \
    return ast; \
  }




/*


#define CHECK(...) \
  NEED(LOOK(__VA_ARGS__))

#define AST(ID) \
  ast = ast_new(parser->t, ID)

#define AST_TOKEN(...) \
  NEED(LOOK(__VA_ARGS__)); \
  ast = consume(parser)

#define AST_RULE(...) \
  { \
    ALTS(__VA_ARGS__); \
    ast = rulealt(parser, alt, NULL, opt); \
    NEED(ast); \
  }

#define INSERT(ID) insert(parser, ID, ast)

#define LOOK(...) \
  ({ \
    TOKS(__VA_ARGS__); \
    look(parser, tok); \
  })

#define ACCEPT(...) \
  ({ \
    TOKS(__VA_ARGS__); \
    accept(parser, tok, ast); \
  })

#define ACCEPT_DROP(...) \
  ({ \
    TOKS(__VA_ARGS__); \
    accept(parser, tok, NULL); \
  })

#define OPTIONAL(...) \
  if(!ACCEPT(__VA_ARGS__)) \
  { \
    INSERT(TK_NONE); \
  }

#define EXPECT(...) \
  { \
    TOKS(__VA_ARGS__); \
    NEED(accept(parser, tok, ast)); \
  }

#define SKIP(...) \
  { \
    TOKS(__VA_ARGS__); \
    NEED(accept(parser, tok, NULL)); \
  }

#define RULE(...) \
  { \
    ALTS(__VA_ARGS__); \
    NEED(rulealt(parser, alt, ast, opt)); \
  }

#define OPTRULE(...) \
  { \
    ALTS(__VA_ARGS__); \
    if(!rulealt(parser, alt, ast, true)) \
    { \
      INSERT(TK_NONE); \
    } \
  }

#define IFRULE(X, ...) \
  if(ACCEPT_DROP(X)) \
  { \
    RULE(__VA_ARGS__); \
  } else { \
    INSERT(TK_NONE); \
  }

#define IFTOKEN(X, ...) \
  if(ACCEPT_DROP(X)) \
  { \
    EXPECT(__VA_ARGS__); \
  } else { \
    INSERT(TK_NONE); \
  }

#define WHILERULE(X, ...) \
  while(ACCEPT_DROP(X)) \
  { \
    RULE(__VA_ARGS__); \
  }

#define WHILETOKEN(X, ...) \
  while(ACCEPT_DROP(X)) \
  { \
    EXPECT(__VA_ARGS__); \
  }

#define SEQRULE(...) \
  { \
    ALTS(__VA_ARGS__); \
    while(rulealt(parser, alt, ast, true)); \
  }

#define BINDOP(...) \
  { \
    ALTS(__VA_ARGS__); \
    ast = bindop(parser, precedence, associativity, ast, alt); \
  }

#define EXPECTBINDOP(...) \
  { \
    ALTS(__VA_ARGS__); \
    ast_t* nast = bindop(parser, precedence, associativity, ast, alt); \
    NEED(nast != ast); \
    ast = nast; \
  }

#define SCOPE() scope(ast)

#define DONE() \
    return ast; \
  }

*/

#endif
