#include "parserapi.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


static bool trace_enable = false;


static token_id current_token_id(parser_t* parser)
{
  return token_get_id(parser->token);
}


static ast_t* consume_token(parser_t* parser)
{
  ast_t* ast = ast_token(parser->token);
  ast_setdata(ast, parser->next_flags);
  parser->next_flags = NULL;
  parser->token = lexer_next(parser->lexer);
  return ast;
}


static void consume_token_no_ast(parser_t* parser)
{
  token_free(parser->token);
  parser->token = lexer_next(parser->lexer);
}


static void syntax_error(parser_t* parser, const char* expected)
{
  assert(parser != NULL);
  assert(expected != NULL);

  if(parser->last_matched == NULL)
  {
    error(parser->source, token_line_number(parser->token),
      token_line_position(parser->token), "syntax error: no code found");
  }
  else
  {
    error(parser->source, token_line_number(parser->token),
      token_line_position(parser->token),
      "syntax error: expected %s after %s", expected, parser->last_matched);
  }
}


// Standard build functions

void default_builder(rule_state_t* state, ast_t* new_ast)
{
  assert(state != NULL);
  assert(new_ast != NULL);

  // Existing AST goes at the top

  if(state->last_child == NULL)  // No valid last pointer
    ast_append(state->ast, new_ast);
  else  // Add new AST to end of children
    ast_add_sibling(state->last_child, new_ast);

  state->last_child = new_ast;
}


void infix_builder(rule_state_t* state, ast_t* new_ast)
{
  assert(state != NULL);
  assert(new_ast != NULL);

  // New AST goes at the top
  ast_add(new_ast, state->ast);
  state->ast = new_ast;
  state->last_child = NULL;
}


void infix_reverse_builder(rule_state_t* state, ast_t* new_ast)
{
  assert(state != NULL);
  assert(new_ast != NULL);

  // New AST goes at the top, existing goes on the right
  ast_append(new_ast, state->ast);
  state->ast = new_ast;

  // state->last_child is actually still valid, so leave it
}


// Functions called by macros

// Process any deferred token we have
static void process_deferred_ast(parser_t* parser, rule_state_t* state)
{
  assert(parser != NULL);
  assert(state != NULL);

  if(state->deferred)
  {
    token_t* deferred_token = token_new(state->deferred_id, parser->source);
    token_set_pos(deferred_token, state->line, state->pos);
    state->ast = ast_token(deferred_token);
    state->deferred = false;
  }
}


// Add the given AST to ours, handling deferment
static void add_ast(parser_t* parser, rule_state_t* state, ast_t* new_ast,
  builder_fn_t build_fn)
{
  assert(parser != NULL);
  assert(state != NULL);
  assert(new_ast != NULL && new_ast != PARSE_ERROR);
  assert(build_fn != NULL);

  process_deferred_ast(parser, state);

  if(state->ast == NULL)
  {
    // The new AST is our only AST so far
    state->ast = new_ast;
    state->last_child = NULL;
  }
  else
  {
    // Add the new AST to our existing AST
    build_fn(state, new_ast);
  }
}


// Add an AST node for the specified token, which may be deferred
void add_deferrable_ast(parser_t* parser, rule_state_t* state, token_id id)
{
  if(!state->matched && state->ast == NULL && !state->deferred)
  {
    // This is the first AST node, defer creation
    state->deferred = true;
    state->deferred_id = id;
    state->line = token_line_number(parser->token);
    state->pos = token_line_position(parser->token);
    return;
  }

  add_ast(parser, state, ast_new(parser->token, id), default_builder);
}


/* Process the result from finding a token or sub rule.
 * Args:
 *    new_ast AST generate from found token or sub rule, NULL for none.
 *    out_found reports whether an optional token was found. Only set on
 *      success. May be set to NULL if this information is not needed.
 *
 * Returns:
 *    PARSE_OK
 */
static ast_t* handle_found(parser_t* parser, rule_state_t* state,
  ast_t* new_ast, builder_fn_t build_fn, bool* out_found)
{
  assert(parser != NULL);
  assert(state != NULL);

  if(out_found != NULL)
    *out_found = true;

  if(!state->matched)
  {
    // First token / sub rule in rule was found
    if(parser->trace)
      printf("Rule %s: Matched\n", state->fn_name);

    state->matched = true;
  }

  if(new_ast != NULL)
    add_ast(parser, state, new_ast, build_fn);

  state->deflt_id = TK_LEX_ERROR;
  return PARSE_OK;
}


/* Process the result from not finding a token or sub rule.
* Args:
*    out_found reports whether an optional token was found. Only set on
*      success. May be set to NULL if this information is not needed.
*
* Returns:
 *    PARSE_OK if not error.
 *    PARSE_ERROR to propogate a lexer error.
 *    RULE_NOT_FOUND if current token is not is specified set.
*/
static ast_t* handle_not_found(parser_t* parser, rule_state_t* state,
  const char* desc, bool* out_found)
{
  assert(parser != NULL);
  assert(state != NULL);
  assert(desc != NULL);

  if(out_found != NULL)
    *out_found = false;

  if(state->deflt_id == TK_LEX_ERROR)
  {
    // Required token / sub rule not found
    ast_free(state->ast);

    if(!state->matched)
    {
      // Rule not matched
      if(parser->trace)
        printf("Rule %s: Not matched\n", state->fn_name);

      return RULE_NOT_FOUND;
    }

    // Rule partially matched, error
    if(parser->trace)
      printf("Rule %s: Error\n", state->fn_name);

    syntax_error(parser, desc);
    return PARSE_ERROR;
  }

  if(state->deflt_id != TK_EOF) // Default node is specified
    add_deferrable_ast(parser, state, state->deflt_id);

  state->deflt_id = TK_LEX_ERROR;
  return PARSE_OK;
}


/* Check if current token matches any in given set and consume on match.
 * Args:
 *    id_set is a TK_NONE terminated list.
 *    make_ast specifies whether to construct an AST node on match or discard
 *      consumed token.
 *    out_found reports whether an optional token was found. Only set on
 *      success. May be set to NULL if this information is not needed.
 *
 * Returns:
 *    PARSE_OK on success.
 *    PARSE_ERROR to propogate a lexer error.
 *    RULE_NOT_FOUND if current token is not is specified set.
 */
ast_t* parse_token_set(parser_t* parser, rule_state_t* state, const char* desc,
  const token_id* id_set, bool make_ast, bool* out_found)
{
  assert(parser != NULL);
  assert(state != NULL);
  assert(id_set != NULL);

  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)
  {
    // Propgate error
    ast_free(state->ast);
    return PARSE_ERROR;
  }

  if(desc == NULL)
    desc = token_id_desc(id_set[0]);

  if(parser->trace)
  {
    printf("Rule %s: Looking for %s token%s %s. Found %s. ",
      state->fn_name,
      (state->deflt_id == TK_LEX_ERROR) ? "required" : "optional",
      (id_set[1] == TK_NONE) ? "" : "s", desc,
      token_print(parser->token));
  }

  for(const token_id* p = id_set; *p != TK_NONE; p++)
  {
    // Match new line if the next token is the first on a line
    if(*p == TK_NEWLINE && token_is_first_on_line(parser->token))
      return handle_found(parser, state, NULL, NULL, out_found);

    if(id == *p)
    {
      // Current token matches one in set
      if(parser->trace)
        printf("Compatible\n");

      parser->last_matched = token_print(parser->token);

      if(make_ast)
        return handle_found(parser, state, consume_token(parser),
          default_builder, out_found);

      // AST not needed, discard token
      consume_token_no_ast(parser);
      return handle_found(parser, state, NULL, NULL, out_found);
    }
  }

  // Current token does not match any in current set
  if(parser->trace)
    printf("Not compatible\n");

  return handle_not_found(parser, state, desc, out_found);
}


/* Check if any of the specified rules can be matched.
 * Args:
 *    rule_set is a NULL terminated list.
 *    out_found reports whether an optional token was found. Only set on
 *      success. May be set to NULL if this information is not needed.
 *
 * Returns:
 *    PARSE_OK on success.
 *    PARSE_ERROR to propogate an error.
 *    RULE_NOT_FOUND if no rules in given set can be matched.
 */
ast_t* parse_rule_set(parser_t* parser, rule_state_t* state, const char* desc,
  const rule_t* rule_set, bool* out_found)
{
  assert(parser != NULL);
  assert(state != NULL);
  assert(desc != NULL);
  assert(rule_set != NULL);

  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)
  {
    // Propgate error
    ast_free(state->ast);
    return PARSE_ERROR;
  }

  if(parser->trace)
  {
    printf("Rule %s: Looking for %s rule%s \"%s\"\n",
      state->fn_name,
      (state->deflt_id == TK_LEX_ERROR) ? "required" : "optional",
      (rule_set[1] == NULL) ? "" : "s", desc);
  }

  for(const rule_t* p = rule_set; *p != NULL; p++)
  {
    builder_fn_t build_fn = default_builder;
    ast_t* rule_ast = (*p)(parser, &build_fn);

    if(rule_ast == PARSE_ERROR)
    {
      // Propgate error
      ast_free(state->ast);
      return PARSE_ERROR;
    }

    if(rule_ast != RULE_NOT_FOUND)
    {
      // Rule found
      parser->last_matched = desc;
      return handle_found(parser, state, rule_ast, build_fn, out_found);
    }
  }

  // No rules in set can be matched
  return handle_not_found(parser, state, desc, out_found);
}


// Set the data flags to use for the next token consumed from the source
void parse_set_next_flags(parser_t* parser, uint64_t flags)
{
  assert(parser != NULL);
  parser->next_flags = (void*)flags;
}


/* Tidy up a successfully parsed rule.
 * Args:
 *    rule_set is a NULL terminated list.
 *    out_found reports whether an optional token was found. Only set on
 *      success. May be set to NULL if this information is not needed.
 *
 * Returns:
 *    AST created, NULL for none.
 */
ast_t* parse_rule_complete(parser_t* parser, rule_state_t* state)
{
  assert(parser != NULL);
  assert(state != NULL);

  process_deferred_ast(parser, state);

  if(state->scope && state->ast != NULL)
    ast_scope(state->ast);

  if(parser->trace)
    printf("Rule %s: Complete\n", state->fn_name);

  return state->ast;
}


// Top level functions

void parse_trace(bool enable)
{
  trace_enable = enable;
}


bool parse(ast_t* package, source_t* source, rule_t start, const char* expected)
{
  assert(package != NULL);
  assert(source != NULL);
  assert(expected != NULL);

  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return false;

  // Create a parser and attach the lexer
  parser_t* parser = POOL_ALLOC(parser_t);
  parser->source = source;
  parser->lexer = lexer;
  parser->token = lexer_next(lexer);
  parser->last_matched = NULL;
  parser->trace = trace_enable;

  // Parse given start rule
  builder_fn_t build_fn;
  ast_t* ast = start(parser, &build_fn);

  if(ast == PARSE_ERROR)
    ast = NULL;

  if(ast == RULE_NOT_FOUND)
  {
    syntax_error(parser, expected);
    ast = NULL;
  }

  lexer_close(lexer);
  token_free(parser->token);
  POOL_FREE(parser_t, parser);

  if(ast == NULL)
  {
    source_close(source);
    return false;
  }

  assert(ast_id(ast) == TK_MODULE);
  assert(ast_data(ast) == NULL);
  ast_setdata(ast, source);
  ast_add(package, ast);
  return true;
}
