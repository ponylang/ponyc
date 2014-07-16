#ifndef PARSERAPI_H
#define PARSERAPI_H

#include "lexer.h"
#include "ast.h"
#include "token.h"
#include <stdbool.h>
#include <limits.h>

/** We use a simple recursive decent parser. Each grammar rule is specified
 * using the macros defined below. Whilst it is perfectly possible to mix
 * normal C code in with the macros it should not be necessary. The underlying
 * functions that the macros use should not be called outside of the macros.
 *
 * Note that we assume that no look ahead is needed.
 *
 * When called each rule tries to match its tokens and subrules in order. When
 * the first token or subrule is found then the rule is considered to be
 * matched and the rest of it must be found or a syntax error is declared. If
 * the first required token or subrule is not found the the rule is a miss and
 * reports "not found" to its caller. It is up to the caller to decide whether
 * this is an error.
 *
 * An optional token or subrule not found does not cause a miss, so rules can
 * safely start with optional items.
 *
 * Each rule returns one of 4 things:
 * 1. PARSE_ERROR - indicates that an error occurred. Parse errors are
 *    propogated up to the caller without re-reporting them.
 * 2. RULE_NOT_FOUND - indcates that the rule was not found. It is up to the
 *    caller whether this constitutes an error.
 * 3. An AST tree - generated from a successful rule parse. It is the
 *    responsibility to free the tree with ast_free().
 * 4. NULL - indicates a successful rule parse, but that rule generates no AST.
 *    This is perfectly legal for all rules except the top level initial rule.
 *
 * The AST nodes created from token and returned by subrules are built into a
 * tree for the rule. The first node found is the rule parent node and all
 * subsequent nodes are built up as its children.
 *
 * The AST_NODE macro adds a node with a specified ID. Specifying this at the
 * start of a rule will ensure that this is the parent node. For the sake of
 * efficiency the creation of such a node is deferred until such time as the
 * rule has matched or reached its end.
 */


typedef struct lexer_t lexer_t;
typedef struct source_t source_t;
typedef struct token_t token_t;

typedef struct parser_t
{
  source_t* source;
  lexer_t* lexer;
  token_t* token;
} parser_t;

/// State of parsing current rule
typedef struct rule_state_t
{
  bool matched;   // Has the rule matched yet
  bool opt;       // Is the next sub item optional
  bool scope;     // Is this rule a scope
  bool deferred;  // Do we have a deferred AST node
  token_id deferred_id; // ID of deferred AST node
  int line, pos;        // Locatino to claim deferred node is from
} rule_state_t;

typedef int (*prec_t)(token_id id);
typedef bool (*assoc_t)(token_id id);
typedef ast_t* (*rule_t)(parser_t* parser);


#define PARSE_ERROR     ((ast_t*)1)   // A parse error has occured
#define RULE_NOT_FOUND  ((ast_t*)2)   // Sub item was not found


// Functions used by macros

bool consume_if_match_no_ast(parser_t* parser, token_id id);

void process_deferred_ast(parser_t* parser, ast_t** rule_ast,
  rule_state_t* state);

void add_ast(parser_t* parser, ast_t* new_ast, ast_t** rule_ast,
  rule_state_t* state);

void add_deferrable_ast(parser_t* parser, token_id id, ast_t** ast,
  rule_state_t* state);

void add_infix_ast(ast_t* new_ast, ast_t* prev_ast, ast_t** rule_ast,
  prec_t prec, assoc_t assoc);

ast_t* sub_result(parser_t* parser, ast_t* rule_ast, rule_state_t* state,
  ast_t* sub_ast, const char* function, int line_no);

ast_t* token_in_set(parser_t* parser, const token_id* id_set, bool make_ast);

ast_t* rule_in_set(parser_t* parser, const rule_t* rule_set);

void syntax_error(parser_t* parser, const char* func, int line);


// Worker macros

#define HANDLE_ERRORS(sub_ast) \
  { \
    ast_t*r = sub_result(parser, ast, &state, sub_ast, __FUNCTION__, __LINE__); \
    if(r != NULL) return r;  \
  }

#define MAKE_DEFAULT(sub_ast, id) \
  if(sub_ast == RULE_NOT_FOUND) sub_ast = ast_new(parser->token, id);

#define ERROR() \
  { \
    syntax_error(parser, __FUNCTION__, __LINE__); \
    ast_free(ast); \
    return PARSE_ERROR; \
  }


/* This is the only external API call.
 * Returns generated AST or NULL on error.
 * It is the caller's responsibility to free the returned AST with ast_free().
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
    rule_state_t state = { false, false, false, false }


/** Add a node to our AST.
 * The first node in a rule is the parent and all subsequent nodes are children
 * of the parent.
 * In the interests of efficiency when an initial node created by this macro
 * the creation of that node is deferred until the rule has matched.
 * Example:
 *    AST_NODE(TK_CASE);
 */
#define AST_NODE(ID)  add_deferrable_ast(parser, ID, &ast, &state)


/** Specify that the containing rule is a scope.
 * May appear anywhere within the rule definition.
 * Example:
 *    SCOPE();
 */
#define SCOPE()   state.scope = true


/** Specify that the following token or rule is optional.
 * May be applied to TOKEN, SKIP and RULE.
 * Example:
 *    OPT TOKEN(TK_FOO);
 */
#define OPT state.opt = true;


/** Attempt to match one of the given set of tokens.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * If an optional token is not found a default TK_NONE node is created instead.
 * Example:
 *    TOKEN(TK_PLUS, TK_MINUS, TK_FOO);
 */
#define TOKEN(...) \
  { \
    static const token_id id_set[] = { __VA_ARGS__, TK_NONE }; \
    ast_t* sub_ast = token_in_set(parser, id_set, true); \
    HANDLE_ERRORS(sub_ast); \
    MAKE_DEFAULT(sub_ast, TK_NONE); \
    add_ast(parser, sub_ast, &ast, &state) ; \
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
 * If an optional token is not found a default TK_NONE node is created instead.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * Example:
 *    RULE(constructor, behaviour, function);
 */
#define RULE(...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    ast_t* sub_ast = rule_in_set(parser, rule_set); \
    HANDLE_ERRORS(sub_ast); \
    MAKE_DEFAULT(sub_ast, TK_NONE); \
    add_ast(parser, sub_ast, &ast, &state); \
  }


/** If the next token is the specified id consume it and parse the specified
 * block of tokens and / or rules.
 * If the condition id is not found the next token is not consumed and a
 * default AST node (with id TK_NONE) is created.
 * Example:
 *    IF(TK_COLON, RULE(type));
 */
#define IF(id, body) \
  { \
    if(consume_if_match_no_ast(parser, id)) \
    { \
      state.matched = true; \
      body; \
    } \
    else \
    { \
      add_ast(parser, ast_new(parser->token, TK_NONE), &ast, &state); \
    } \
  }


/** Repeatedly try to parse one optional token or rule and every time it
 * succeeds parse the specified block of tokens and / or rules.
 * When the condition id is not found the next token is not consumed.
 * Example:
 *    WHILE(TK_COLON, RULE(type));
 */
#define WHILE(id, body) \
  while(consume_if_match_no_ast(parser, id)) \
  { \
    state.matched = true; \
    body; \
  }


/** Repeatedly try to parse any of the given set of rules as long as it
 * succeeds.
 * Example:
 *    SEQ(class, use);
 */
#define SEQ(...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    while(true) \
    { \
      ast_t* sub_ast = rule_in_set(parser, rule_set); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      HANDLE_ERRORS(sub_ast); \
      add_ast(parser, sub_ast, &ast, &state); \
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
      if(sub_ast == NULL) ERROR(); \
      HANDLE_ERRORS(sub_ast); \
      add_infix_ast(sub_ast, prev_ast, &ast, precedence, associativity); \
      prev_ast = sub_ast; \
      state.opt = true; \
    } \
    if(!state.opt && orig_ast == ast) ERROR(); \
  }


/// Must appear at the end of each defined rule
#define DONE() \
    process_deferred_ast(parser, &ast, &state); \
    if(state.scope && ast != NULL) ast_scope(ast); \
    return ast; \
  }


#endif
