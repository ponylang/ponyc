#include "parserapi.h"
#include <stdlib.h>
#include <assert.h>

#include <stdio.h>

static token_id current_token_id(parser_t* parser)
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


/** Check whether the next token is the one specified and if so consume it
 * without generating an AST.
 */
bool consume_if_match_no_ast(parser_t* parser, token_id id)
{
  if(current_token_id(parser) != id)
    return false;

  consume_token_no_ast(parser);
  return true;
}


/// Process our deferred AST node creation, if any
void process_deferred_ast(parser_t* parser, ast_t** rule_ast,
  rule_state_t* state)
{
  if(!state->deferred) // No deferment to process
    return;

  assert(*rule_ast == NULL);
  token_t* deferred_token = token_new(state->deferred_id, parser->source);
  token_set_pos(deferred_token, state->line, state->pos);
  *rule_ast = ast_token(deferred_token);
  state->deferred = false;
}


/// Add the given AST to ours, handling deferment
void add_ast(parser_t* parser, ast_t* new_ast, ast_t** rule_ast,
  rule_state_t* state)
{
  if(new_ast == PARSE_ERROR || new_ast == RULE_NOT_FOUND || new_ast == NULL)
    return;

  process_deferred_ast(parser, rule_ast, state);

  if(*rule_ast == NULL)
  {
    // The new AST is our only AST so far
    *rule_ast = new_ast;
  }
  else
  {
    // Add the new AST to our existing AST
    ast_add(*rule_ast, new_ast);
  }
}


/// Add an AST node for the specified token, which may be deferred
void add_deferrable_ast(parser_t* parser, token_id id, ast_t** ast,
  rule_state_t* state)
{
  if(!state->matched && *ast == NULL && !state->deferred)
  {
    // This is the first AST node, defer creation
    state->deferred = true;
    state->deferred_id = id;
    state->line = token_line_number(parser->token);
    state->pos = token_line_position(parser->token);
    return;
  }

  add_ast(parser, ast_new(parser->token, id), ast, state);
}


/// Add the given AST to ours using precedence rules, handling deferment
void add_infix_ast(ast_t* new_ast, ast_t* prev_ast, ast_t** rule_ast,
  prec_t prec, assoc_t assoc)
{
  token_id id = ast_id(new_ast);
  int new_prec = prec(id);
  bool right = assoc(id);

  while(true)
  {
    int old_prec = prec(ast_id(prev_ast));

    if((new_prec > old_prec) || (right && (new_prec == old_prec)))
    {
      // Insert new AST under old AST
      ast_append(new_ast, ast_pop(prev_ast));
      ast_add(prev_ast, new_ast);
      return;
    }

    // New AST needs to go above old AST
    if(prev_ast == *rule_ast)
    {
      // New AST needs to go above existing tree
      ast_append(new_ast, prev_ast);
      *rule_ast = new_ast;
      return;
    }

    // Move up to parent
    prev_ast = ast_parent(prev_ast);
  }
}


/** Process the result from parsing a token or sub rule.
 * Returns:
 *    NULL if rule should continue
 *    non-NULL if rule should immediately return that error value
 */
ast_t* sub_result(parser_t* parser, ast_t* rule_ast, rule_state_t* state,
  ast_t* sub_ast, const char* function, int line_no)
{
  if(sub_ast == PARSE_ERROR)
  {
    // Propogate error
    ast_free(rule_ast);
    return PARSE_ERROR;
  }

  if(sub_ast == RULE_NOT_FOUND && !state->opt)
  {
    // Required token / sub rule not found
    ast_free(rule_ast);

    if(!state->matched) // Rule not matched
      return RULE_NOT_FOUND;

    // Rule partially matched, error
    syntax_error(parser, function, line_no);
    return PARSE_ERROR;
  }

  if(sub_ast != RULE_NOT_FOUND) // A token / sub rule was found
    state->matched = true;

  state->opt = false;
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
ast_t* token_in_set(parser_t* parser, const token_id* id_set, bool make_ast)
{
  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)  // propgate error
    return PARSE_ERROR;

  for(const token_id* p = id_set; *p != TK_NONE; p++)
  {
    if(id == *p)
    {
      // Current token matches one in set
      if(make_ast)
        return consume_token(parser);

      // AST not needed, discard token
      consume_token_no_ast(parser);
      return NULL;
    }
  }

  // Current token does not match any in current set
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
ast_t* rule_in_set(parser_t* parser, const rule_t* rule_set)
{
  token_id id = current_token_id(parser);

  if(id == TK_LEX_ERROR)  // propgate error
    return PARSE_ERROR;

  for(const rule_t* p = rule_set; *p != NULL; p++)
  {
    ast_t* rule_ast = (*p)(parser);

    if(rule_ast != RULE_NOT_FOUND)  // Rule found, or error
      return rule_ast;
  }

  // No rules in set can be matched
  return RULE_NOT_FOUND;
}


/// Report a syntax error
void syntax_error(parser_t* parser, const char* func, int line)
{
  error(parser->source, token_line_number(parser->token),
    token_line_position(parser->token), "syntax error (%s, %d)", func, line);
}


ast_t* parse(source_t* source, rule_t start)
{
  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return PARSE_ERROR;

  // Create a parser and attach the lexer
  parser_t* parser = (parser_t*)calloc(1, sizeof(parser_t));
  parser->source = source;
  parser->lexer = lexer;
  parser->token = lexer_next(lexer);

  // Parse given start rule
  ast_t* ast = start(parser);

  if(ast == PARSE_ERROR)
    ast = NULL;

  if(ast == RULE_NOT_FOUND)
  {
    syntax_error(parser, __FUNCTION__, __LINE__);
    ast = NULL;
  }

  if(ast != NULL)
  {
    // TODO: add last child pointer to allow building AST in order without reversing
    ast_reverse(ast);
    assert(ast_data(ast) == NULL);
    ast_setdata(ast, source);
  }

  lexer_close(lexer);
  token_free(parser->token);
  free(parser);

  return ast;
}
