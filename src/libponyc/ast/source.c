#include "source.h"
#include "error.h"
#include "stringtab.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

source_t* source_open(const char* file, const char** error_msgp)
{
  FILE* fp = fopen(file, "rb");

  if(fp == NULL)
  {
    *error_msgp = "can't open file";
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  ssize_t size = ftell(fp);

  if(size < 0)
  {
    *error_msgp = "can't determine length of file";
    fclose(fp);
    return NULL;
  }

  fseek(fp, 0, SEEK_SET);

  source_t* source = POOL_ALLOC(source_t);
  source->file = stringtab(file);
  source->m = (char*)ponyint_pool_alloc(size + 1);
  source->len = size + 1;

  ssize_t read = fread(source->m, sizeof(char), size, fp);
  source->m[size] = '\0';

  if(read < size)
  {
    *error_msgp = "failed to read entire file";
    ponyint_pool_free(source->len, source->m);
    POOL_FREE(source_t, source);
    fclose(fp);
    return NULL;
  }

  fclose(fp);
  return source;
}


source_t* source_open_string(const char* source_code)
{
  source_t* source = POOL_ALLOC(source_t);
  source->file = NULL;
  source->len = strlen(source_code) + 1; // for null terminator
  source->m = (char*)ponyint_pool_alloc(source->len);

  memcpy(source->m, source_code, source->len);

  return source;
}


void source_close(source_t* source)
{
  if(source == NULL)
    return;

  if(source->m != NULL)
    ponyint_pool_free(source->len, source->m);

  POOL_FREE(source_t, source);
}


static void source_serialise_trace(pony_ctx_t* ctx, void* object)
{
  source_t* source = (source_t*)object;

  if(source->file != NULL)
    string_trace(ctx, source->file);

  pony_serialise_reserve(ctx, source->m, source->len);
}

static void source_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  source_t* source = (source_t*)object;
  source_t* dst = (source_t*)((uintptr_t)buf + offset);

  dst->file = (const char*)pony_serialise_offset(ctx, (char*)source->file);
  dst->m = (char*)pony_serialise_offset(ctx, source->m);
  dst->len = source->len;
}

static void source_deserialise(pony_ctx_t* ctx, void* object)
{
  source_t* source = (source_t*)object;

  source->file = string_deserialise_offset(ctx, (uintptr_t)source->file);
  source->m = (char*)pony_deserialise_block(ctx, (uintptr_t)source->m,
    source->len);
}

static pony_type_t source_pony =
{
  0,
  sizeof(source_t),
  0,
  0,
  NULL,
  NULL,
  source_serialise_trace,
  source_serialise,
  source_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* source_pony_type()
{
  return &source_pony;
}
