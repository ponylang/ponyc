#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "parserapi.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
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
  token_t* token;
  ast_token_id id;
  bool have_token;
  bool had_error;
} builder_t;


static ast_t* get_nodes(builder_t* builder, ast_token_id terminator);


static void build_error(builder_t* builder, const char* fmt, ...)
{
  if(builder->had_error)
    return;

  va_list ap;
  va_start(ap, fmt);
  token_t* token = builder->token;
  errorv(token_source(token), token_line_number(token),
    token_line_position(token), fmt, ap);
  va_end(ap);

  builder->had_error = true;
}


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

  builder->id = id;
  builder->have_token = true;
}


static ast_token_id peek_token(builder_t* builder)
{
  get_next_token(builder);
  return builder->id;
}


static ast_token_id get_token(builder_t* builder)
{
  get_next_token(builder);
  builder->have_token = false;
  return builder->id;
}


static void save_token(builder_t* builder)
{
  builder->token = NULL;
}


static ast_token_id keyword_replace(builder_t* builder)
{
  token_id keyword_id = lexer_is_abstract_keyword(token_string(builder->token));
  if(keyword_id == TK_LEX_ERROR)
    return AT_ID;

  token_set_id(builder->token, keyword_id);
  return AT_TOKEN;
}


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
  printf("Builder open\n");
  source = source_open("hmm");

  // Open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return NULL;

  builder_t builder;
  builder.lexer = lexer;
  builder.token = NULL;
  builder.had_error = false;

  // Parse given start rule
  printf("Builder parse\n");
  ast_t* ast = get_nodes(&builder, AT_EOF);

  if(ast != NULL)
    ast_reverse(ast);

  lexer_close(lexer);
  token_free(builder.token);

  if(ast != NULL)
  {
    printf("Builder print\n");
    ast_print(ast, 80);
    ast_free(ast);
  }

  printf("Builder done\n");
  return ast;
}
