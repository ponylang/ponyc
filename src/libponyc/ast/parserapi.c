#include "parserapi.h"
#include <stdlib.h>
#include <assert.h>

#include <stdio.h>


static token_id current_token_id(parser_t* parser)
{
  return parser->t->id;
}


ast_t* consume_token(parser_t* parser)
{
  ast_t* ast = ast_token(parser->t);
  parser->t = lexer_next(parser->lexer);
  return ast;
}


void consume_token_no_ast(parser_t* parser)
{
  token_free(parser->t);
  parser->t = lexer_next(parser->lexer);
}


/** Check whether the next token is the one specified and if it is consume it
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
void process_deferred_ast(ast_t** rule_ast, rule_state_t* state)
{
  if(!state->deferred) // No deferment to process
    return;

  ast_t* deferred = ast_new(&state->deferred_token, state->deferred_ast);

  if(*rule_ast != NULL) // Make deferred node parent of existing AST
    ast_add(deferred, *rule_ast);

  *rule_ast = deferred;
  state->deferred = false;
}


/// Add the given AST to ours, handling deferment
void add_ast(ast_t* new_ast, ast_t** rule_ast, rule_state_t* state)
{
  if(new_ast == PARSE_ERROR || new_ast == RULE_NOT_FOUND || new_ast == NULL)
    return;

  process_deferred_ast(rule_ast, state);

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
  if(!state->matched && *ast == NULL && state->deferred_ast < 0)
  {
    // This is the first AST node, defer creation
    state->deferred_ast = id;
    token_copy_pos(parser->t, &state->deferred_token);
    return;
  }

  add_ast(ast_new(parser->t, id), ast, state);
}


/// Add the given AST to ours using precedence rules, handling deferment
void add_infix_ast(ast_t* new_ast, ast_t* prev_ast, ast_t** rule_ast,
  prec_t prec, assoc_t assoc)
{
  token_id id = ast_id(new_ast);
  int new_prec = prec(id);
  bool right = assoc(id);

  printf("infix id %d, prec %d\n", id, new_prec);

  while(true)
  {
    int old_prec = prec(ast_id(prev_ast));
    printf("infix prev id %d (%d), prec %d\n", ast_id(prev_ast), TK_THISTYPE,  old_prec);

    if((new_prec > old_prec) || (right && (new_prec == old_prec)))
    {
      // Insert new AST under old AST
      printf("infix pop\n");
      ast_t* t = ast_pop(prev_ast);
      printf("infix append 1 %p %p\n", new_ast, t);
      ast_append(new_ast, t);
      printf("infix appended\n");
      //ast_append(new_ast, ast_pop(prev_ast));
      ast_add(prev_ast, new_ast);
      return;
    }

    // New AST needs to go above old AST
    if(prev_ast == *rule_ast)
    {
      // New AST needs to go above existing tree
      printf("infix append 2 %p %p\n", new_ast, prev_ast);
      ast_append(new_ast, prev_ast);
      printf("infix appended\n");
      *rule_ast = new_ast;
      return;
    }

    // Move up to parent
    prev_ast = ast_parent(prev_ast);
  }
}


/** Process the result from parsing a token or sub rule.
 * Returns:
 *    true if rule should immediately return the value in ast
 *    false if rule should continue
 */
bool sub_result(parser_t* parser, ast_t** rule_ast, rule_state_t* state,
  ast_t* sub_ast, const char* function, int line_no)
{
  if(sub_ast == PARSE_ERROR)
  {
    // Propogate error
    ast_free(*rule_ast);
    *rule_ast = PARSE_ERROR;
    return true;
  }

  if(sub_ast != RULE_NOT_FOUND)
  {
    // One of specified tokens / sub rules was found
    state->matched = true;
  }
  else
  {
    // Token / sub rule not found
    if(!state->opt)
    {
      // Required token / sub rule not found
      ast_free(*rule_ast);

      if(state->matched)
      {
        // Rule partially matched, error
        syntax_error(parser, function, line_no);
        *rule_ast = PARSE_ERROR;
      }
      else
      {
        // Rule not matched
        *rule_ast = RULE_NOT_FOUND;
      }

      return true;
    }
  }

  state->opt = false;
  return false;
}


/// Create a default AST node if we don't already have one
void make_default_ast(parser_t* parser, ast_t** ast, token_id id)
{
  if(*ast != RULE_NOT_FOUND)  // Already have an AST
    return;

  *ast = ast_new(parser->t, id);
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


/// Apply a scope to the given AST
void apply_scope(ast_t* ast)
{
  if(ast != NULL)
    ast_scope(ast);
}


/// Report a syntax error
void syntax_error(parser_t* parser, const char* func, int line)
{
  //if(!parser->t->error)
  //{
  error(parser->source, parser->t->line, parser->t->pos,
    "syntax error (%s, %d)", func, line);

  //parser->t->error = true;
  //}
}


ast_t* parse(source_t* source, rule_t start)
{
  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return PARSE_ERROR;

  // Create a parser and attach the lexer
  parser_t* parser = calloc(1, sizeof(parser_t));
  parser->source = source;
  parser->lexer = lexer;
  parser->t = lexer_next(lexer);

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
  token_free(parser->t);
  free(parser);

  return ast;
}



/*
ast_t* rulealt(parser_t* parser, const rule_t* alt, ast_t* ast, bool opt)
{
  while((*alt) != NULL)
  {
    ast_t* child = (*alt)(parser, opt);

    if(child != NULL)
    {
      if(ast != NULL)
        ast_add(ast, child);

      return child;
    }

    alt++;
  }

  return NULL;
}





void insert(parser_t* parser, token_id id, ast_t* ast)
{
  ast_t* child = ast_new(parser->t, id);
  ast_add(ast, child);
}

bool look(parser_t* parser, const token_id* id)
{
  while(*id != TK_NONE)
  {
    if(current(parser) == *id)
      return true;

    id++;
  }

  return false;
}

bool accept(parser_t* parser, const token_id* id, ast_t* ast)
{
  if(!look(parser, id))
    return false;

  if(ast != NULL)
  {
    ast_t* child = ast_token(parser->t);
    ast_add(ast, child);
  } else {
    token_free(parser->t);
  }

  parser->t = lexer_next(parser->lexer);
  return true;
}

ast_t* rulealt(parser_t* parser, const rule_t* alt, ast_t* ast, bool opt)
{
  while((*alt) != NULL)
  {
    ast_t* child = (*alt)(parser, opt);

    if(child != NULL)
    {
      if(ast != NULL)
        ast_add(ast, child);

      return child;
    }

    alt++;
  }

  return NULL;
}

ast_t* parse(source_t* source, rule_t start)
{
  // open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return NULL;

  // create a parser and attach the lexer
  parser_t* parser = calloc(1, sizeof(parser_t));
  parser->source = source;
  parser->lexer = lexer;
  parser->t = lexer_next(lexer);

  ast_t* ast = start(parser, false);

  if(ast != NULL)
  {
    ast_reverse(ast);
    assert(ast_data(ast) == NULL);
    ast_setdata(ast, source);
  }

  lexer_close(lexer);
  token_free(parser->t);
  free(parser);

  return ast;
}

ast_t* bindop(parser_t* parser, prec_t prec, assoc_t assoc, ast_t* ast,
  const rule_t* alt)
{
  ast_t* next;
  int oprec = INT_MAX;

  while((next = rulealt(parser, alt, NULL, true)) != NULL)
  {
    token_id id = ast_id(next);
    int nprec = prec(id);
    bool right = assoc(id);

    while(true)
    {
      if((nprec > oprec) || (right && (nprec == oprec)) )
      {
        // (oldop ast.1 (newop ast.2 ...))
        ast_append(next, ast_pop(ast));
        ast_add(ast, next);
        ast = next;
        break;
      } else if(ast_parent(ast) == NULL) {
        // (newop ast ...)
        ast_append(next, ast);
        ast = next;
        break;
      } else {
        // try the parent node
        ast = ast_parent(ast);
        oprec = prec(ast_id(ast));
      }
    }

    oprec = nprec;
  }

  while(ast_parent(ast) != NULL)
    ast = ast_parent(ast);

  return ast;
}

void syntax_error(parser_t* parser, const char* func, int line)
{
  if(!parser->t->error)
  {
    error(parser->source, parser->t->line, parser->t->pos,
      "syntax error (%s, %d)", func, line);

    parser->t->error = true;
  }
}

void scope(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(child == NULL)
  {
    ast_scope(ast);
  } else {
    ast_t* next = ast_sibling(child);

    while(next != NULL)
    {
      child = next;
      next = ast_sibling(child);
    }

    ast_scope(child);
  }
}

*/
