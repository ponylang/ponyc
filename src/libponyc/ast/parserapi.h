#ifndef PARSERAPI_H
#define PARSERAPI_H

#include "lexer.h"
#include "ast.h"
#include "token.h"
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

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
 *
 * Note that since we use a simple recursive descent approach we cannot handle
 * grammars including left recursive rules (ie a rule where the first element
 * is a recursive call to the same rule). Such rules will lead to infinite
 * loops and/or stack overflow.
 */


typedef struct lexer_t lexer_t;
typedef struct source_t source_t;
typedef struct token_t token_t;

typedef struct parser_t
{
  source_t* source;
  lexer_t* lexer;
  token_t* token;
  const char* last_matched;
  bool trace;
} parser_t;

/// State of parsing current rule
typedef struct rule_state_t
{
  const char* fn_name;  // Name of the current function, for tracing
  bool matched;         // Has the rule matched yet
  bool opt;             // Is the next sub item optional
  bool top;             // Should the next AST node be top of our sub tree
  bool scope;           // Is this rule a scope
  bool deferred;        // Do we have a deferred AST node
  token_id deferred_id; // ID of deferred AST node
  size_t line, pos;        // Locatino to claim deferred node is from
} rule_state_t;

typedef int (*prec_t)(token_id id);
typedef bool (*assoc_t)(token_id id);
typedef ast_t* (*rule_t)(parser_t* parser);


#define PARSE_ERROR     ((ast_t*)1)   // A parse error has occured
#define RULE_NOT_FOUND  ((ast_t*)2)   // Sub item was not found


// Functions used by macros

void process_deferred_ast(parser_t* parser, ast_t** rule_ast,
  rule_state_t* state);

void add_ast(parser_t* parser, ast_t* new_ast, ast_t** rule_ast,
  rule_state_t* state);

void add_deferrable_ast(parser_t* parser, token_id id, ast_t** ast,
  rule_state_t* state);

void add_infix_ast(ast_t* new_ast, ast_t* prev_ast, ast_t** rule_ast,
  prec_t prec, assoc_t assoc);

ast_t* sub_result(parser_t* parser, ast_t* rule_ast, rule_state_t* state,
  ast_t* sub_ast, const char* desc);

ast_t* token_in_set(parser_t* parser, rule_state_t* state, const char* desc,
  const token_id* id_set, bool make_ast);

ast_t* rule_in_set(parser_t* parser, rule_state_t* state, const char* desc,
  const rule_t* rule_set);

void syntax_error(parser_t* parser, const char* expected);

token_id current_token_id(parser_t* parser);


// Worker macros

#define NORMALISE_TOKEN_DESC(desc, deflt) \
  ((desc == NULL) ? token_id_desc(deflt) : desc)

#define HANDLE_ERRORS(sub_ast, desc) \
  { \
    ast_t* r = sub_result(parser, ast, &state, sub_ast, desc); \
    if(r != NULL) return r;  \
  }

#define RESET_STATE() \
  state.opt = false; \
  state.top = false;

#define THROW_ERROR(desc) \
  { \
    syntax_error(parser, desc); \
    ast_free(ast); \
    return PARSE_ERROR; \
  }


// External API

/// Enable or disable parsing trace output
void parse_trace(bool enable);

/** Returns generated AST or NULL on error.
 * The expected argument is used in the generated error message if nothing is
 * found. It is not stored.
 * It is the caller's responsibility to free the returned AST with ast_free().
 */
ast_t* parse(source_t* source, rule_t start, const char* expected);


/* The API for parser rules starts here */

/// Rule forward declaration
#define DECL(rule) \
  static ast_t* rule(parser_t* parser)


/// Rule definition
#define DEF(rule) \
  static ast_t* rule(parser_t* parser) \
  { \
    ast_t* ast = NULL; \
    rule_state_t state = { #rule, false, false, false, false, false }


/** Add a node to our AST.
 * The first node in a rule is the parent and all subsequent nodes are children
 * of the parent.
 * In the interests of efficiency when an initial node created by this macro
 * the creation of that node is deferred until the rule has matched.
 * Example:
 *    AST_NODE(TK_CASE);
 */
#define AST_NODE(ID)  add_deferrable_ast(parser, ID, &ast, &state)


/** Map our AST node ID.
 * If the current AST node ID is the first argument, it is set to the second.
 */
#define MAP_ID(FROM, TO) if(ast_id(ast) == FROM) ast_setid(ast, TO)


/** Specify that the containing rule is a scope.
 * May appear anywhere within the rule definition.
 * Example:
 *    SCOPE();
 */
#define SCOPE()   state.scope = true


/** Specify that the following token or rule is optional.
 * May be applied to TOKEN, SKIP and RULE.
 * When used in combination with TOP the order does not matter.
 * Example:
 *    OPT TOKEN("foo", TK_FOO);
 */
#define OPT state.opt = true;


/** Specify that the AST generated by the following token or rule should be
 * added to the top of out AST not the bottom.
 * May be applied to TOKEN, RULE and SEQ. For SEQ it applies to each rule
 * matched by the sequence in turn.
 * When used in combination with OPT the order does not matter.
 * Example:
 *    TOP TOKEN("foo", TK_FOO);
 */
#define TOP state.top = true;


/** Attempt to match one of the given set of tokens.
 * If an optional token is not found a default TK_NONE node is created instead,
 * unless the TOP modifier is used in which case the tree is unmodified.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * The description is used for error reports. If NULL is provided then the
 * lexer description for the first token in the set is used instead.
 * Example:
 *    TOKEN("operator", TK_PLUS, TK_MINUS, TK_FOO);
 */
#define TOKEN(desc, ...) \
  { \
    static const token_id id_set[] = { __VA_ARGS__, TK_NONE }; \
    const char* desc_str = NORMALISE_TOKEN_DESC(desc, id_set[0]); \
    ast_t* sub_ast = token_in_set(parser, &state, desc_str, id_set, true); \
    HANDLE_ERRORS(sub_ast, desc_str); \
    add_ast(parser, sub_ast, &ast, &state); \
    RESET_STATE(); \
  }


/** Attempt to match one of the given set of tokens, but do not generate an AST
 * node for it.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * The description is used for error reports. If NULL is provided then the
 * lexer description for the first token in the set is used instead.
 * Example:
 *    SKIP(NULL, TK_PLUS, TK_MINUS, TK_FOO);
 */
#define SKIP(desc, ...) \
  { \
    static const token_id id_set[] = { __VA_ARGS__, TK_NONE }; \
    const char* desc_str = NORMALISE_TOKEN_DESC(desc, id_set[0]); \
    ast_t* sub_ast = token_in_set(parser, &state, desc_str, id_set, false); \
    HANDLE_ERRORS(sub_ast, desc_str); \
    RESET_STATE(); \
  }


/** Attempt to match one of the given set of rules.
 * If an optional token is not found a default TK_NONE node is created instead,
 * unless the TOP modifier is used in which case the tree is unmodified.
 * If OPT is not specified then a match is required or a syntax error occurs.
 * The description is used for error reports. It must be present and non-NULL.
 * Example:
 *    RULE("method", constructor, behaviour, function);
 */
#define RULE(desc, ...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    ast_t* sub_ast = rule_in_set(parser, &state, desc, rule_set); \
    HANDLE_ERRORS(sub_ast, desc); \
    add_ast(parser, sub_ast, &ast, &state); \
    RESET_STATE(); \
  }


/** If the next token is the specified id consume it and parse the specified
 * block of tokens and / or rules.
 * If the condition id is not found the next token is not consumed and a
 * default AST node (with id TK_NONE) is created.
 * Example:
 *    IF(TK_COLON, RULE("foo", type));
 */
#define IF(id, body) \
  { \
    static const token_id id_set[] = { id, TK_NONE }; \
    state.opt = true; \
    const char* cond_desc = token_id_desc(id); \
    ast_t* sub_ast = token_in_set(parser, &state, cond_desc, id_set, false); \
    HANDLE_ERRORS(sub_ast, cond_desc); \
    RESET_STATE(); \
    if(sub_ast == NULL) \
    { \
      state.matched = true; \
      body; \
    } \
    else \
    { \
      add_ast(parser, RULE_NOT_FOUND, &ast, &state); \
    } \
  }


/** Repeatedly try to parse one optional token or rule and every time it
 * succeeds parse the specified block of tokens and / or rules.
 * When the condition id is not found the next token is not consumed.
 * Example:
 *    WHILE(TK_COLON, RULE("foo", type));
 */
#define WHILE(id, body) \
  { \
    static const token_id id_set[] = { id, TK_NONE }; \
    const char* cond_desc = token_id_desc(id); \
    while(true) \
    { \
      state.opt = true; \
      ast_t* sub_ast = token_in_set(parser, &state, cond_desc, id_set, false);\
      HANDLE_ERRORS(sub_ast, cond_desc); \
      RESET_STATE(); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      state.matched = true; \
      body; \
    } \
  }


/** Repeatedly try to parse any of the given set of rules as long as it
 * succeeds.
 * The description is used for error reports. It must be present and non-NULL.
 * Example:
 *    SEQ("entity", class, use);
 */
#define SEQ(desc, ...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    while(true) \
    { \
      ast_t* sub_ast = rule_in_set(parser, &state, desc, rule_set); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      HANDLE_ERRORS(sub_ast, desc); \
      add_ast(parser, sub_ast, &ast, &state); \
    } \
    RESET_STATE(); \
  }


/** Repeatedly try to parse any of the given set of binary infix rules as long
 * as it succeeds and build AST with precedence rules.
 * If OPT is not specified then at least one match is required or a syntax
 * error occurs.
 * The description is used for error reports. It must be present and non-NULL.
 * Example:
 *    BINDOP("compound type", uniontype, tupletype);
 */
#define BINDOP(desc, ...) \
  { \
    static const rule_t rule_set[] = { __VA_ARGS__, NULL }; \
    ast_t* prev_ast = ast; \
    bool had_op = false; \
    while(true) \
    { \
      ast_t* sub_ast = rule_in_set(parser, &state, desc, rule_set); \
      if(sub_ast == RULE_NOT_FOUND) break; \
      if(sub_ast == NULL) THROW_ERROR(desc); \
      HANDLE_ERRORS(sub_ast, desc); \
      add_infix_ast(sub_ast, prev_ast, &ast, precedence, associativity); \
      prev_ast = sub_ast; \
      had_op = true; \
    } \
    if(!state.opt && !had_op) THROW_ERROR(desc); \
    state.opt = false; \
  }


/// Must appear at the end of each defined rule
#define DONE() \
    process_deferred_ast(parser, &ast, &state); \
    if(state.scope && ast != NULL) ast_scope(ast); \
    if(parser->trace) printf("Rule %s: Complete\n", state.fn_name); \
    return ast; \
  }


#endif
