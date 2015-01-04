#include "parserapi.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>


static bool trace_enable = false;


token_id current_token_id(parser_t* parser)
{
  return token_get_id(parser->token);
}


static ast_t* consume_token(parser_t* parser)
{
  ast_t* ast = ast_token(parser->token);
  parser->token = lexer_next(parser->lexer);
  return ast;
}


static void consume_token_no_ast(parser_t* parser)
{
  token_free(parser->token);
  parser->token = lexer_next(parser->lexer);
}


/// Process our deferred AST node creation, if any
void process_deferred_ast(parser_t* parser, rule_state_t* state)
{
  if(!state->deferred) // No deferment to process
    return;

  assert(state->ast == NULL);
  token_t* deferred_token = token_new(state->deferred_id, parser->source);
  token_set_pos(deferred_token, state->line, state->pos);
  state->ast = ast_token(deferred_token);
  state->deferred = false;
}


/// Add the given AST to ours, handling deferment
void add_ast(parser_t* parser, ast_t* new_ast, rule_state_t* state)
{
  if(new_ast == PARSE_ERROR || new_ast == NULL)
    return;

  if(new_ast == RULE_NOT_FOUND)
  {
    if(state->deflt_id == TK_EOF) // Not found, but no default needed
      return;

    // Rule wasn't found and NO_DFLT is not set, so make a default
    new_ast = ast_new(parser->token, state->deflt_id);
  }

  process_deferred_ast(parser, state);

  if(state->ast == NULL)
  {
    // The new AST is our only AST so far
    state->ast = new_ast;
  }
  else
  {
    // Add the new AST to our existing AST
    assert(state->builder != NULL);
    state->ast = state->builder(state->ast, new_ast, state);
  }
}


ast_t* default_builder(ast_t* existing, ast_t* new_ast, rule_state_t* state)
{
  assert(state != NULL);

  // Existing AST goes at the top

  if(state->last_child == NULL)  // New AST is the first child
    ast_add(existing, new_ast);
  else  // Add new AST to end of children
    ast_add_sibling(state->last_child, new_ast);

  state->last_child = new_ast;
  return existing;
}


/// Add an AST node for the specified token, which may be deferred
void add_deferrable_ast(parser_t* parser, token_id id, rule_state_t* state)
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

  add_ast(parser, ast_new(parser->token, id), state);
}


/** Process the result from parsing a token or sub rule.
 * Returns:
 *    NULL if rule should continue
 *    non-NULL if rule should immediately return that error value
 */
ast_t* sub_result(parser_t* parser, rule_state_t* state, ast_t* sub_ast,
  const char* desc)
{
  if(sub_ast == PARSE_ERROR)
  {
    // Propogate error
    ast_free(state->ast);
    return PARSE_ERROR;
  }

  if(sub_ast == RULE_NOT_FOUND && state->deflt_id == TK_LEX_ERROR)
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

  if(sub_ast != RULE_NOT_FOUND && !state->matched)
  {
    // First token / sub rule in rule was found
    if(parser->trace)
      printf("Rule %s: Matched\n", state->fn_name);

    state->matched = true;
  }

  return NULL;
}


/** Check if current token matches any in given set and consume on match.
 * Args:
 *    id_set is a TK_NONE terminated list.
 *    make_ast specifies whether to construct an AST node on match or discard
 *      consumed token
 *
 * Returns:
 *    New AST node if match found and make_ast is true
 *    NULL if match found and make_ast is false
 *    PARSE_ERROR to propogate a lexer error
 *    RULE_NOT_FOUND if current token is not is specified set
 */
ast_t* token_in_set(parser_t* parser, rule_state_t* state, const char* desc,
  const token_id* id_set, bool make_ast)
{
  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)  // propgate error
    return PARSE_ERROR;

  if(parser->trace)
  {
    printf("Rule %s: Looking for %s token%s %s. Found %s. ",
      state->fn_name,
      (state->deflt_id == TK_LEX_ERROR) ? "required" : "optional",
      (id_set[1] == TK_NONE) ? "" : "s", desc, token_print(parser->token));
  }

  for(const token_id* p = id_set; *p != TK_NONE; p++)
  {
    if(id == *p)
    {
      // Current token matches one in set
      if(parser->trace)
        printf("Compatible\n");

      parser->last_matched = token_print(parser->token);

      if(make_ast)
        return consume_token(parser);

      // AST not needed, discard token
      consume_token_no_ast(parser);
      return NULL;
    }
  }

  // Current token does not match any in current set
  if(parser->trace)
    printf("Not compatible\n");

  return RULE_NOT_FOUND;
}


/** Check if any of the specified rules can be matched.
 * Args:
 *    rule_set is a NULL terminated list.
 *
 * Returns:
 *    Matched rule AST if match found
 *    PARSE_ERROR to propogate an error
 *    RULE_NOT_FOUND if no rules in given set can be matched
 */
ast_t* rule_in_set(parser_t* parser, rule_state_t* state, const char* desc,
  const rule_t* rule_set)
{
  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)  // propgate error
    return PARSE_ERROR;

  if(parser->trace)
  {
    printf("Rule %s: Looking for %s rule%s \"%s\"\n",
      state->fn_name,
      (state->deflt_id == TK_LEX_ERROR) ? "required" : "optional",
      (rule_set[1] == NULL) ? "" : "s", desc);
  }

  for(const rule_t* p = rule_set; *p != NULL; p++)
  {
    ast_t* rule_ast = (*p)(parser);

    if(rule_ast == PARSE_ERROR)
      return rule_ast;

    if(rule_ast != RULE_NOT_FOUND)
    {
      // Rule found
      parser->last_matched = desc;
      return rule_ast;
    }
  }

  // No rules in set can be matched
  return RULE_NOT_FOUND;
}


/// Report a syntax error
void syntax_error(parser_t* parser, const char* expected)
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


void parse_trace(bool enable)
{
  trace_enable = enable;
}


ast_t* parse(source_t* source, rule_t start, const char* expected)
{
  assert(source != NULL);
  assert(expected != NULL);

  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return PARSE_ERROR;

  // Create a parser and attach the lexer
  parser_t* parser = POOL_ALLOC(parser_t);
  parser->source = source;
  parser->lexer = lexer;
  parser->token = lexer_next(lexer);
  parser->last_matched = NULL;
  parser->trace = trace_enable;

  // Parse given start rule
  ast_t* ast = start(parser);

  if(ast == PARSE_ERROR)
    ast = NULL;

  if(ast == RULE_NOT_FOUND)
  {
    syntax_error(parser, expected);
    ast = NULL;
  }

  if(ast != NULL)
  {
    assert(ast_data(ast) == NULL);
    ast_setdata(ast, source);
  }

  lexer_close(lexer);
  token_free(parser->token);
  POOL_FREE(parser_t, parser);

  return ast;
}
