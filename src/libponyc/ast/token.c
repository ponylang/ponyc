#include "token.h"
#include "lexer.h"
#include "../ds/stringtab.h"
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

  union
  {
    const char* string;
    double real;
    __uint128_t integer;
  };
} token_t;


token_t* token_new(token_id id, source_t* source)
{
  token_t* t = calloc(1, sizeof(token_t));
  t->id = id;
  t->source = source;
  return t;
}


token_t* token_dup(token_t* token)
{
  assert(token != NULL);
  token_t* t = malloc(sizeof(token_t));
  memcpy(t, token, sizeof(token_t));
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
  if(token != NULL)
    free(token);
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


double token_float(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  return token->real;
}


__uint128_t token_int(token_t* token)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  return token->integer;
}


const char* token_print(token_t* token)
{
  static char buf[1000];

  assert(token != NULL);

  switch(token->id)
  {
    //case TK_NONE:
    //  return "x";

    case TK_ID:
    case TK_STRING:
      return token->string;

    case TK_INT:
      snprintf(buf, sizeof(buf), "%zu", (size_t)token->integer);
      return buf;

    case TK_FLOAT:
    {
      int r = snprintf(buf, sizeof(buf), "%g", token->real);
      if(strcspn(buf, ".e") == r)
        snprintf(buf + r, sizeof(buf)-r, ".0");

      return buf;
    }

    default:
      break;
  }

  const char* p = lexer_print(token->id);
  if(p != NULL)
    return p;

  snprintf(buf, sizeof(buf), "Unknown_token_%d", token->id);
  return buf;
}


source_t* token_source(token_t* token)
{
  assert(token != NULL);
  return token->source;
}


int token_line_number(token_t* token)
{
  assert(token != NULL);
  return token->line;
}


int token_line_position(token_t* token)
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


void token_set_string(token_t* token, const char* value)
{
  assert(token != NULL);
  assert(token->id == TK_STRING || token->id == TK_ID);
  assert(value != NULL);
  token->string = stringtab(value);
}


void token_set_float(token_t* token, double value)
{
  assert(token != NULL);
  assert(token->id == TK_FLOAT);
  token->real = value;
}


void token_set_int(token_t* token, __uint128_t value)
{
  assert(token != NULL);
  assert(token->id == TK_INT);
  token->integer = value;
}


void token_set_pos(token_t* token, int line, int pos)
{
  assert(token != NULL);
  token->line = line;
  token->pos = pos;
}
