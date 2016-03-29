#include "token.h"
#include "lexer.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct token_t
{
  token_id id;
  source_t* source;
  size_t line;
  size_t pos;
  char* printed;

  union
  {
    struct
    {
      const char* string;
      size_t str_length;
    };

    double real;
    lexint_t integer;
  };
} token_t;


token_t* token_new(token_id id)
{
  token_t* t = POOL_ALLOC(token_t);
  memset(t, 0, sizeof(token_t));
  t->id = id;
  return t;
}

token_t* token_dup(token_t* token)
{
  assert(token != NULL);
  token_t* t = POOL_ALLOC(token_t);
  memcpy(t, token, sizeof(token_t));
  t->printed = NULL;
  return t;
}


token_t* token_dup_new_id(token_t* token, token_id id)
{
  token_t* new_token = token_dup(token);
  new_token->id = id;
  return new_token;
}


void token_free(token_t* token)
{
  if(token == NULL)
    return;

  if(token->printed != NULL)
    ponyint_pool_free_size(64, token->printed);

  POOL_FREE(token_t, token);
}


// Read accessors

token_id token_get_id(token_t* token)
{
  assert(token != NULL);
  return token->id;
}


const char* token_string(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  return token->string;
}


size_t token_string_len(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  return token->str_length;
}


double token_float(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  return token->real;
}


lexint_t* token_int(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  return &token->integer;
}


const char* token_print(token_t* token)
{
  assert(token != NULL);

  switch(token->id)
  {
    case TK_EOF:
      return "EOF";

    case TK_ID:
    case TK_STRING:
      return token->string;

    case TK_INT:
      if (token->printed == NULL)
        token->printed = (char*)ponyint_pool_alloc_size(64);

      snprintf(token->printed, 64, "%llu",
        (unsigned long long)token->integer.low);
      return token->printed;

    case TK_FLOAT:
    {
      if(token->printed == NULL)
        token->printed = (char*)ponyint_pool_alloc_size(64);

      int r = snprintf(token->printed, 64, "%g", token->real);

      if(strcspn(token->printed, ".e") == (size_t)r)
        snprintf(token->printed + r, 64 - r, ".0");

      return token->printed;
    }

    case TK_LEX_ERROR:
      return "LEX_ERROR";

    default:
      break;
  }

  const char* p = lexer_print(token->id);
  if(p != NULL)
    return p;

  if(token->printed == NULL)
    token->printed = (char*)ponyint_pool_alloc_size(64);

  snprintf(token->printed, 64, "Unknown_token_%d", token->id);
  return token->printed;
}


const char* token_id_desc(token_id id)
{
  switch(id)
  {
    case TK_EOF:    return "EOF";
    case TK_ID:     return "id";
    case TK_STRING: return "string literal";
    case TK_INT:    return "int literal";
    case TK_FLOAT:  return "float literal";
    case TK_TRUE:   return "true literal";
    case TK_FALSE:  return "false literal";
    default: break;
  }

  const char* p = lexer_print(id);
  if(p != NULL)
    return p;

  return "UNKOWN";
}


source_t* token_source(token_t* token)
{
  assert(token != NULL);
  return token->source;
}


size_t token_line_number(token_t* token)
{
  assert(token != NULL);
  return token->line;
}


size_t token_line_position(token_t* token)
{
  assert(token != NULL);
  return token->pos;
}


// Write accessors

void token_set_id(token_t* token, token_id id)
{
  assert(token != NULL);
  token->id = id;
}


void token_set_string(token_t* token, const char* value, size_t length)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  assert(value != NULL);

  if(length == 0)
    length = strlen(value);

  token->string = stringtab_len(value, length);
  token->str_length = length;
}


void token_set_float(token_t* token, double value)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  token->real = value;
}


void token_set_int(token_t* token, lexint_t* value)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  token->integer = *value;
}

void token_set_pos(token_t* token, source_t* source, size_t line, size_t pos)
{
  assert(token != NULL);

  if(source != NULL)
    token->source = source;

  token->line = line;
  token->pos = pos;
}
