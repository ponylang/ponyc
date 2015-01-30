#ifndef PARSERAPI_H
#define PARSERAPI_H

#include <platform.h>

#include "lexer.h"
#include "ast.h"
#include "token.h"
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

PONY_EXTERN_C_BEGIN

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
typedef struct parser_t parser_t;


/// State of parsing current rule
typedef struct rule_state_t
{
  const char* fn_name;  // Name of the current function, for tracing
  ast_t* ast;           // AST built for this rule
  ast_t* last_child;    // Last child added to current ast
  const char* desc;     // Rule description (set by parent)
  token_id* restart;    // Restart token set, NULL for none
  token_id deflt_id;    // ID of node to create when an optional token or rule
                        // is not found.
                        // TK_EOF = do not create a default
                        // TL_LEX_ERROR = rule is not optional
  bool matched;         // Has the rule matched yet
  bool scope;           // Is this rule a scope
  bool deferred;        // Do we have a deferred AST node
  token_id deferred_id; // ID of deferred AST node
  size_t line, pos;     // Location to claim deferred node is from
} rule_state_t;


typedef void (*builder_fn_t)(rule_state_t* state, ast_t* new_ast);

typedef ast_t* (*rule_t)(parser_t* parser, builder_fn_t *out_builder,
  const char* rule_desc);


#define PARSE_OK        ((ast_t*)1)   // Requested parse successful
#define PARSE_ERROR     ((ast_t*)2)   // A parse error has occured
#define RULE_NOT_FOUND  ((ast_t*)3)   // Sub item was not found


// Functions used by macros

void infix_builder(rule_state_t* state, ast_t* new_ast);

void infix_reverse_builder(rule_state_t* state, ast_t* new_ast);

void add_deferrable_ast(parser_t* parser, rule_state_t* state, token_id id);

ast_t* parse_token_set(parser_t* parser, rule_state_t* state, const char* desc,
  const token_id* id_set, bool make_ast, bool* out_found);

ast_t* parse_rule_set(parser_t* parser, rule_state_t* state, const char* desc,
  const rule_t* rule_set, bool* out_found);

void parse_set_next_flags(parser_t* parser, uint64_t flags);

ast_t* parse_rule_complete(parser_t* parser, rule_state_t* state);


// External API

/// Enable or disable parsing trace output
void parse_trace(bool enable);

/** Generates a module AST and attaches it to the given package AST.
 * The expected argument is used in the generated error message if nothing is
 * found. It is not stored.
 * The given source is attached to the resulting AST on success and closed on
 * failure.
 */
bool parse(ast_t* package, source_t* source, rule_t start,
  const char* expected);


/* The API for parser rules starts here */

/// Rule forward declaration
#define DECL(rule) \
  static ast_t* rule(parser_t* parser, builder_fn_t *out_builder, \
    const char* rule_desc)


/// Rule definition
#define DEF(rule) \
  static ast_t* rule(parser_t* parser, builder_fn_t *out_builder, \
    const char* rule_desc) \
  { \
    (void)out_builder; \
    rule_state_t state = {#rule, NULL, NULL, rule_desc, NULL, TK_LEX_ERROR, \
      false, false, false, TK_NONE, 0, 0}


/** Specify a restart point.
 * Restart points are used to partially recover after a parse error. This
 * allows the remainder of the file to be parsed, potentially allowing for
 * multiple errors to be reported in a single run.
 *
 * For a restart point all possible legal tokens that could after the current
 * rule must be specified. If the rule does not parse correctly then we ignore
 * all subsequent tokens until we find one in the given set at which point we
 * attempt to continue parsing.
 *
 * If the rule does parse correctly then we check that the following token is
 * in the given following set. If it isn't then we report an error and ignore
 * all subsequent tokens until we find one in the set.
 *
 * Note:
 * 1. EOF is automatically added to the provided set.
 * 2. Must appear before any token or sub rule macros that may fail.
 * 3. May only appear once in each rule.
 *
 * Example:
 *    RESTART(TK_FUN, T_NEW, TK_BE);
 */
#define RESTART(...) \
  token_id restart_set[] = { __VA_ARGS__, TK_EOF, TK_NONE }; \
  state.restart = restart_set


/** Add a node to our AST.
 * The first node in a rule is the parent and all subsequent nodes are children
 * of the parent.
 * In the interests of efficiency when an initial node created by this macro
 * the creation of that node is deferred until the rule has matched.
 * Example:
 *    AST_NODE(TK_CASE);
 */
#define AST_NODE(ID)  add_deferrable_ast(parser, &state, ID)


/** Map our AST node ID.
 * If the current AST node ID is the first argument, it is set to the second.
 */
#define MAP_ID(FROM, TO) if(ast_id(state.ast) == FROM) ast_setid(state.ast, TO)


/** Specify that the containing rule is a scope.
 * May appear anywhere within the rule definition.
 * Example:
 *    SCOPE();
 */
#define SCOPE()   state.scope = true


/** Specify the action if the following token or rule is not found.
 * May be applied to TOKEN, SKIP and RULE.
 *
 * (none)       Default is an error if token or rule not found.
 * OPT          TK_NONE node created if token or rule not found.
 * OPT_DFLT     Specified ID created if token or rule not found.
 * OPT_NO_DFLT  No AST node created if token or rule not found.
 *
 * Examples:
 *    OPT TOKEN("foo", TK_FOO);
 *    OPT_DFLT(TK_TRUE) TOKEN("foo", TK_FOO);
 *    OPT_NO_DFLT TOKEN("foo", TK_FOO);
 */
#define OPT           state.deflt_id = TK_NONE;
#define OPT_DFLT(id)  state.deflt_id = id;
#define OPT_NO_DFLT   state.deflt_id = TK_EOF;


/** Specify the build function the parent rule should use to combine the result
 * of this rule into their AST.
 * If not specified the default of append to parent AST is used.
 * An arbitrary function (of type builder_fn_t) may be specified, but there are
 * also standard functions predefined.
 *
 * Example:
 *    CUSTOMBUILD(infix_builder);
 *
 * Predefined functions:
 *    INFIX_BUILD     For infix operators (eg +).
 *    INFIX_REVERSE   For infix operators requiring the existing parent AST to
 *                    be the last child (eg call).
 */
#define CUSTOMBUILD(builder_fn) *out_builder = builder_fn
#define INFIX_BUILD()   *out_builder = infix_builder
#define INFIX_REVERSE() *out_builder = infix_reverse_builder


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
    ast_t* r = parse_token_set(parser, &state, desc, id_set, true, NULL); \
    if(r != PARSE_OK) return r; \
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
    ast_t* r = parse_token_set(parser, &state, desc, id_set, false, NULL); \
    if(r != PARSE_OK) return r; \
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
    ast_t* r = parse_rule_set(parser, &state, desc, rule_set, NULL); \
    if(r != PARSE_OK) return r; \
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
    state.deflt_id = TK_NONE; \
    bool found = false; \
    ast_t* r = parse_token_set(parser, &state, token_id_desc(id), id_set, \
      false, &found); \
    if(r != PARSE_OK) return r; \
    if(found) { \
      body; \
    } \
  }


/** If the next token is the specified id consume it and parse the specified
 * "then" block of tokens and / or rules.
 * If the condition id is not found the next token is not consumed and the
 * specified "else" block of tokens and / or rules is parsed.
 * Example:
 *    IFELSE(TK_COLON, RULE("foo", type), RULE("bar", no_type));
 */
#define IFELSE(id, thenbody, elsebody) \
  { \
    static const token_id id_set[] = { id, TK_NONE }; \
    state.deflt_id = TK_EOF; \
    bool found = false; \
    ast_t* r = parse_token_set(parser, &state, token_id_desc(id), id_set, \
      false, &found); \
    if(r != PARSE_OK) return r; \
    if(found) { \
      thenbody; \
    } else { \
      elsebody; \
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
    bool found = true; \
    while(true) \
    { \
      state.deflt_id = TK_EOF; \
      ast_t* r = parse_token_set(parser, &state, token_id_desc(id), id_set, \
        false, &found); \
      if(r != PARSE_OK) return r; \
      if(!found) break; \
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
    bool found = true; \
    while(found) \
    { \
      state.deflt_id = TK_EOF; \
      ast_t* r = parse_rule_set(parser, &state, desc, rule_set, &found); \
      if(r != PARSE_OK) return r; \
    } \
  }


/** Change the order of the children of the current node.
 * Desired order is specified as a list of indices of the current order. All
 * indices must appear exactly once in the list or bad things may happen.
 *
 * Example:
 *    REORDER(1, 2, 0, 3);
 */
#define REORDER(...) \
  { \
    static const size_t order[] = { __VA_ARGS__ }; \
    assert(ast_childcount(state.ast) == (sizeof(order) / sizeof(size_t))); \
    ast_reorder_children(state.ast, order); \
    state.last_child = NULL; \
  }


/** Execute arbitrary C code to rewrite the AST as desired or perform any other
 * task.
 * The local variable "ast" is available as both an in and out parameter for
 * accessing the tree produced by the current rule. Any unneeded nodes should
 * be freed within the provided code.
 *
 * Example:
 *    REWRITE(ast_print(ast));
 */
#define REWRITE(body) \
  { \
    ast_t* ast = state.ast; \
    body; \
    state.ast = ast; \
    state.last_child = NULL; \
  }


/** Set a data field flag, which are used to communicate extra information
 * between the parser and syntax pass.
 * The value specified is the flag value, not the flag index.
 *
 * Example:
 *      SET_FLAG(FOO_FLAG);
 */
#define SET_FLAG(f) \
  ast_setdata(state.ast, (void*)(f | (uint64_t)ast_data(state.ast)))


/** Set the data field flags to use for the next token found in the source.
 *
 * Example:
 *    NEXT_FLAGS(FOO_FLAG);
 */
#define NEXT_FLAGS(f)  parse_set_next_flags(parser, f)


/// Must appear at the end of each defined rule
#define DONE() \
    return parse_rule_complete(parser, &state); \
  }

PONY_EXTERN_C_END

#endif
