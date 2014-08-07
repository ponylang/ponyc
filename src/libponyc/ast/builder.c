#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "parserapi.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


typedef enum ast_token_id
{
  AT_LPAREN,
  AT_RPAREN,
  AT_LSQUARE,
  AT_RSQUARE,
  AT_COLON,
  AT_ID,      // Pony name
  AT_TOKEN,   // Any other Pony token
  AT_EOF,
  AT_ERROR
} ast_token_id;


typedef struct builder_t
{
  lexer_t* lexer;
  source_t* source;
  token_t* token;
  ast_token_id id;
  int line;
  int pos;
  bool have_token;
  bool had_error;
} builder_t;


static ast_t* get_nodes(builder_t* builder, ast_token_id terminator);


// Report an error
static void build_error(builder_t* builder, const char* fmt, ...)
{
  if(builder->had_error)
    return;

  va_list ap;
  va_start(ap, fmt);
  errorv(builder->source, builder->line, builder->pos, fmt, ap);
  va_end(ap);

  builder->had_error = true;
}


// Get the next token ready for when we need it
static void get_next_token(builder_t* builder)
{
  if(builder->have_token)
    return;

  if(builder->token != NULL)
    token_free(builder->token);

  builder->token = lexer_next(builder->lexer);
  ast_token_id id;

  switch(token_get_id(builder->token))
  {
    case TK_LPAREN_NEW:
    case TK_LPAREN:     id = AT_LPAREN;  break;
    case TK_RPAREN:     id = AT_RPAREN;  break;
    case TK_LSQUARE_NEW:
    case TK_LSQUARE:    id = AT_LSQUARE; break;
    case TK_RSQUARE:    id = AT_RSQUARE; break;
    case TK_COLON:      id = AT_COLON;   break;
    case TK_EOF:        id = AT_EOF;     break;
    case TK_LEX_ERROR:  id = AT_ERROR;   break;
    case TK_ID:         id = AT_ID;      break;
    default:            id = AT_TOKEN;   break;
  }

  //printf("Got token %d -> %d\n", token_get_id(builder->token), id);
  builder->id = id;
  builder->have_token = true;
  builder->line = token_line_number(builder->token);
  builder->pos = token_line_position(builder->token);
}


// Peek at our next token without consuming it
static ast_token_id peek_token(builder_t* builder)
{
  get_next_token(builder);
  return builder->id;
}


// Get the next token, consuming it
static ast_token_id get_token(builder_t* builder)
{
  get_next_token(builder);
  builder->have_token = false;
  return builder->id;
}


// Prevent the token associated with the current token from being freed
static void save_token(builder_t* builder)
{
  builder->token = NULL;
}


// Replace the current ID token with an abstract keyword, if it is one
static ast_token_id keyword_replace(builder_t* builder)
{
  token_id keyword_id = lexer_is_abstract_keyword(token_string(builder->token));
  if(keyword_id == TK_LEX_ERROR)
    return AT_ID;

  token_set_id(builder->token, keyword_id);
  return AT_TOKEN;
}


/* Load a type description.
 * A type description is the type AST node contained in square brackets.
 * The leading [ must have been loaded before this is called.
 */
static ast_t* get_type(builder_t* builder, ast_t* parent)
{
  if(parent == NULL)
  {
    build_error(builder, "Type with no containing node");
    return NULL;
  }

  if(ast_type(parent) != NULL)
  {
    build_error(builder, "Node has multiple types");
    return NULL;
  }

  return get_nodes(builder, AT_RSQUARE);
}


/* Load node attributes, if any.
 * Attributes are a list of keywords, each prefixed with a colon, following a
 * node name. For example:
 *    seq:scope
 *
 * The node name must have been parsed before calling this, but not the colon.
 */
static ast_t* get_attributes(builder_t* builder, ast_t* node)
{
  if(peek_token(builder) != AT_COLON)
    return node;

  get_token(builder);

  if(get_token(builder) != AT_ID)
  {
    build_error(builder, "Expected attribute after :");
    return NULL;
  }

  if(strcmp("scope", token_string(builder->token))==0)
  {
    ast_scope(node);
  }
  else
  {
    build_error(builder, "Unrecognised attribute \"%s\"",
      token_string(builder->token));
    return NULL;
  }

  return node;
}


/* Load an ID node.
 * IDs are indicated by the keyword id followed by the ID name, all contained
 * within parentheses. For example:
 *    (id foo)
 *
 * The ( and id keyword must have been parsed before this is called.
 */
static ast_t* get_id(builder_t* builder, ast_t* existing_ast)
{
  if(existing_ast != NULL)
  {
    ast_free(existing_ast);
    build_error(builder, "Seen ID not first in node");
    return NULL;
  }

  if(get_token(builder) != AT_ID)
  {
    build_error(builder, "ID name expected");
    return NULL;
  }

  ast_t* ast = ast_token(builder->token);
  ast_setid(ast, TK_ID);
  save_token(builder);

  if(get_token(builder) != AT_RPAREN)
  {
    build_error(builder, "Close paren expected for ID");
    ast_free(ast);
    return NULL;
  }

  return ast;
}


// Load a sequence of nodes until the specified terminator is found
static ast_t* get_nodes(builder_t* builder, ast_token_id terminator)
{
  ast_t* ast = NULL;

  while(true)
  {
    ast_token_id id = get_token(builder);
    ast_t* child = NULL;
    bool is_type = false;

    if(id == terminator)
    {
      if(ast == NULL)
        build_error(builder, "Syntax error");

      return ast;
    }

    if(id == AT_ID)
      id = keyword_replace(builder);

    switch(id)
    {
      case AT_LPAREN:
        child = get_nodes(builder, AT_RPAREN);
        break;

      case AT_LSQUARE:
        child = get_type(builder, ast);
        is_type = true;
        break;

      case AT_ERROR:  // Propogate
        break;

      case AT_TOKEN:
        child = ast_token(builder->token);
        save_token(builder);
        get_attributes(builder, child);
        break;

      case AT_ID:
        if(strcmp("id", token_string(builder->token)) == 0)
          return get_id(builder, ast);

        build_error(builder, "Unrecognised identifier \"%s\"",
          token_string(builder->token));
        break;

      default:
        build_error(builder, "Syntax error");
        break;
    }

    if(child == NULL)
    {
      // An error occurred and should already have been reported
      ast_free(ast);
      return NULL;
    }

    if(ast == NULL)
      ast = child;
    else if(is_type)
      ast_settype(ast, child);
    else
      ast_add(ast, child);
  }
}


ast_t* build_ast(source_t* source)
{
  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return NULL;

  builder_t builder;
  builder.lexer = lexer;
  builder.source = source;
  builder.token = NULL;
  builder.have_token = false;
  builder.had_error = false;
  builder.line = 1;
  builder.pos = 1;

  // Parse given start rule
  ast_t* ast = get_nodes(&builder, AT_EOF);

  if(ast != NULL)
  {
    ast_reverse(ast);
  }
  else
  {
    print_errors();
    free_errors();
  }

  lexer_close(lexer);
  token_free(builder.token);
  return ast;
}


static bool compare_asts(ast_t* prev, ast_t* expected, ast_t* actual,
  bool check_siblings)
{
  assert(prev != NULL);

  if(expected == NULL && actual == NULL)
    return true;

  if(actual == NULL)
  {
    ast_error(expected, "Expected AST %s not found", ast_get_print(expected));
    return false;
  }

  if(expected == NULL)
  {
    ast_error(prev, "Unexpected AST node found, %s", ast_get_print(actual));
    return false;
  }

  if(ast_id(expected) != ast_id(actual))
  {
    ast_error(expected, "AST ID mismatch, got %d, expected %d",
      ast_id(actual), ast_id(expected));
    return false;
  }

  if(ast_id(expected) == TK_ID && ast_name(actual)[0] == '$' &&
    strcmp(ast_name(expected), "hygid") == 0)
  {
    // Allow expected "hygid" to match any hygenic ID
  }
  else if(strcmp(ast_get_print(expected), ast_get_print(actual)) != 0)
  {
    ast_error(expected, "AST text mismatch, got %s, expected %s",
      ast_get_print(actual), ast_get_print(expected));
    return false;
  }

  if(ast_has_scope(expected) && !ast_has_scope(actual))
  {
    ast_error(expected, "AST missing scope");
    return false;
  }

  if(!ast_has_scope(expected) && ast_has_scope(actual))
  {
    ast_error(actual, "Unexpected AST scope");
    return false;
  }

  if(!compare_asts(expected, ast_child(expected), ast_child(actual), true) ||
    !compare_asts(expected, ast_type(expected), ast_type(actual), true))
    return false;

  return !check_siblings ||
    compare_asts(expected, ast_sibling(expected), ast_sibling(actual), true);
}


bool build_compare_asts(ast_t* expected, ast_t* actual)
{
  assert(expected != NULL);
  assert(actual != NULL);

  return compare_asts(expected, expected, actual, true);
}


bool build_compare_asts_no_sibling(ast_t* expected, ast_t* actual)
{
  assert(expected != NULL);
  assert(actual != NULL);

  return compare_asts(expected, expected, actual, false);
}
