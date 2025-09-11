#include "token.h"
#include "lexer.h"
#include "stringtab.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct token_t
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

#ifndef PONY_NDEBUG
  bool frozen;
#endif
};


// Minimal token structure for signature computation.
struct token_signature_t
{
  token_id id;

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
};


token_t* token_new(token_id id)
{
  token_t* t = POOL_ALLOC(token_t);
  memset(t, 0, sizeof(token_t));
  t->id = id;
  return t;
}

token_t* token_dup(token_t* token)
{
  pony_assert(token != NULL);
  token_t* t = POOL_ALLOC(token_t);
  memcpy(t, token, sizeof(token_t));
  t->printed = NULL;
 #ifndef PONY_NDEBUG
  t->frozen = false;
#endif
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


void token_freeze(token_t* token)
{
  (void)token;
#ifndef PONY_NDEBUG
  pony_assert(token != NULL);
  token->frozen = true;
#endif
}


// Read accessors

token_id token_get_id(token_t* token)
{
  pony_assert(token != NULL);
  return token->id;
}


const char* token_string(token_t* token)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_STRING || token->id == TK_ID);
  return token->string;
}


size_t token_string_len(token_t* token)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_STRING || token->id == TK_ID);
  return token->str_length;
}


double token_float(token_t* token)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_FLOAT);
  return token->real;
}


lexint_t* token_int(token_t* token)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_INT);
  return &token->integer;
}


const char* token_print(token_t* token)
{
  pony_assert(token != NULL);

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


char* token_print_escaped(token_t* token)
{
  pony_assert(token != NULL);
  const char* str = NULL;
  size_t str_len = 0;

  if(token->id == TK_STRING)
  {
    str = token->string;
    str_len = token->str_length;
  } else {
    str = token_print(token);
    str_len = strlen(str);
  }

  // Count the number of escapes so we know the size of the new string.
  size_t escapes = 0;
  for(size_t idx = 0; idx < str_len; idx++)
  {
    char c = str[idx];
    if((c == '"') || (c == '\\') || (c == 0))
      escapes++;
  }

  // Return a simple copy of the current string if there are no escapes.
  if(escapes == 0)
  {
    char* copy = (char*)ponyint_pool_alloc_size(str_len + 1);
    memcpy(copy, str, str_len);
    copy[str_len] = 0;
    return copy;
  }

  // Allocate a new buffer for the escaped string.
  size_t escaped_len = str_len + escapes;
  char* escaped = (char*)ponyint_pool_alloc_size(escaped_len + 1);

  // Fill the buffer of the escaped string, one unescaped character at a time.
  size_t escaped_idx = 0;
  for(size_t idx = 0; idx < str_len; idx++)
  {
    char c = str[idx];
    if((c == '"') || (c == '\\'))
    {
      escaped[escaped_idx++] = '\\';
      escaped[escaped_idx++] = c;
    } else if(c == 0) {
      escaped[escaped_idx++] = '\\';
      escaped[escaped_idx++] = '0';
    } else {
      escaped[escaped_idx++] = c;
    }
  }
  escaped[escaped_idx++] = 0;

  return escaped;
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
  pony_assert(token != NULL);
  return token->source;
}


size_t token_line_number(token_t* token)
{
  pony_assert(token != NULL);
  return token->line;
}


size_t token_line_position(token_t* token)
{
  pony_assert(token != NULL);
  return token->pos;
}


// Write accessors

void token_set_id(token_t* token, token_id id)
{
  pony_assert(token != NULL);
#ifndef PONY_NDEBUG
  pony_assert(!token->frozen);
#endif
  token->id = id;
}


void token_set_string(token_t* token, const char* value, size_t length)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_STRING || token->id == TK_ID);
#ifndef PONY_NDEBUG
  pony_assert(!token->frozen);
#endif
  pony_assert(value != NULL);

  if(length == 0)
    length = strlen(value);

  token->string = stringtab_len(value, length);
  token->str_length = length;
}


void token_set_float(token_t* token, double value)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_FLOAT);
#ifndef PONY_NDEBUG
  pony_assert(!token->frozen);
#endif
  token->real = value;
}


void token_set_int(token_t* token, lexint_t* value)
{
  pony_assert(token != NULL);
  pony_assert(token->id == TK_INT);
#ifndef PONY_NDEBUG
  pony_assert(!token->frozen);
#endif
  token->integer = *value;
}


void token_set_pos(token_t* token, source_t* source, size_t line, size_t pos)
{
  pony_assert(token != NULL);
#ifndef PONY_NDEBUG
  pony_assert(!token->frozen);
#endif

  if(source != NULL)
    token->source = source;

  token->line = line;
  token->pos = pos;
}

// Serialisation

static void token_signature_serialise_trace(pony_ctx_t* ctx, void* object)
{
  token_t* token = (token_t*)object;

  if((token->id == TK_STRING) || (token->id == TK_ID))
    string_trace_len(ctx, token->string, token->str_length);
}

static void token_signature_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  token_t* token = (token_t*)object;
  token_signature_t* dst = (token_signature_t*)((uintptr_t)buf + offset);

  memset(dst, 0, sizeof(token_signature_t));

  dst->id = token->id;

  switch(token->id)
  {
    case TK_STRING:
    case TK_ID:
      dst->str_length = token->str_length;
      dst->string = (const char*)pony_serialise_offset(ctx,
        (char*)token->string);
      break;

    case TK_FLOAT:
      dst->real = token->real;
      break;

    case TK_INT:
      dst->integer = token->integer;
      break;

    default: {}
  }
}

static pony_type_t token_signature_pony =
{
  0,
  sizeof(token_signature_t),
  0,
  0,
  0,
  NULL,
#if defined(USE_RUNTIME_TRACING)
  NULL,
  NULL,
#endif
  NULL,
  token_signature_serialise_trace,
  token_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* token_signature_pony_type()
{
  return &token_signature_pony;
}

// Docstring-specific signature serialisation. We don't want docstrings to
// influence signatures so we pretend them to be TK_NONE nodes.
static void token_docstring_signature_serialise_trace(pony_ctx_t* ctx,
  void* object)
{
  (void)ctx;
  (void)object;

  token_t* token = (token_t*)object;
  (void)token;
  pony_assert(token->id == TK_STRING);
}

static void token_docstring_signature_serialise(pony_ctx_t* ctx, void* object,
  void* buf, size_t offset, int mutability)
{
  (void)ctx;
  (void)object;
  (void)mutability;

  token_signature_t* dst = (token_signature_t*)((uintptr_t)buf + offset);

  memset(dst, 0, sizeof(token_signature_t));

  dst->id = TK_NONE;
}

static pony_type_t token_docstring_signature_pony =
{
  0,
  sizeof(token_signature_t),
  0,
  0,
  0,
  NULL,
#if defined(USE_RUNTIME_TRACING)
  NULL,
  NULL,
#endif
  NULL,
  token_docstring_signature_serialise_trace,
  token_docstring_signature_serialise,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* token_docstring_signature_pony_type()
{
  return &token_docstring_signature_pony;
}

static void token_serialise_trace(pony_ctx_t* ctx, void* object)
{
  token_t* token = (token_t*)object;

  if(token->source != NULL)
    pony_traceknown(ctx, token->source, source_pony_type(), PONY_TRACE_MUTABLE);

  if((token->id == TK_STRING) || (token->id == TK_ID))
    string_trace_len(ctx, token->string, token->str_length);
}

static void token_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  token_t* token = (token_t*)object;
  token_t* dst = (token_t*)((uintptr_t)buf + offset);

  dst->id = token->id;

  dst->source = (source_t*)pony_serialise_offset(ctx, token->source);
  dst->line = token->line;
  dst->pos = token->pos;
  dst->printed = NULL;
#ifndef PONY_NDEBUG
  dst->frozen = token->frozen;
#endif

  switch(token->id)
  {
    case TK_STRING:
    case TK_ID:
      dst->str_length = token->str_length;
      dst->string = (const char*)pony_serialise_offset(ctx,
        (char*)token->string);
      break;

    case TK_FLOAT:
      dst->real = token->real;
      break;

    case TK_INT:
      dst->integer = token->integer;
      break;

    default: {}
  }
}

static void token_deserialise(pony_ctx_t* ctx, void* object)
{
  token_t* token = (token_t*)object;

  token->source = (source_t*)pony_deserialise_offset(ctx, source_pony_type(),
    (uintptr_t)token->source);

  if((token->id == TK_STRING) || (token->id == TK_ID))
    token->string = string_deserialise_offset(ctx, (uintptr_t)token->string);
}

static pony_type_t token_pony =
{
  0,
  sizeof(token_t),
  0,
  0,
  0,
  NULL,
#if defined(USE_RUNTIME_TRACING)
  NULL,
  NULL,
#endif
  NULL,
  token_serialise_trace,
  token_serialise,
  token_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* token_pony_type()
{
  return &token_pony;
}
