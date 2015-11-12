#include "stringtab.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <platform.h>

static bool ptr_cmp(const char* a, const char* b)
{
  return a == b;
}

DEFINE_LIST(strlist, const char, ptr_cmp, NULL);

typedef struct stringtab_entry_t
{
  const char* str;
  size_t len;
  size_t buf_size;
} stringtab_entry_t;

static uint64_t stringtab_hash(stringtab_entry_t* a)
{
  return hash_block(a->str, a->len);
}

static bool stringtab_cmp(stringtab_entry_t* a, stringtab_entry_t* b)
{
  return (a->len == b->len) && (memcmp(a->str, b->str, a->len) == 0);
}

static void stringtab_free(stringtab_entry_t* a)
{
  pool_free_size(a->buf_size, (char*)a->str);
  POOL_FREE(stringtab_entry_t, a);
}

DECLARE_HASHMAP(strtable, stringtab_entry_t);
DEFINE_HASHMAP(strtable, stringtab_entry_t, stringtab_hash, stringtab_cmp,
  pool_alloc_size, pool_free_size, stringtab_free);

static strtable_t table;

void stringtab_init()
{
  strtable_init(&table, 4096);
}

const char* stringtab(const char* string)
{
  if(string == NULL)
    return NULL;

  return stringtab_len(string, strlen(string));
}

const char* stringtab_len(const char* string, size_t len)
{
  if(string == NULL)
    return NULL;

  stringtab_entry_t key = {string, len, 0};
  stringtab_entry_t* n = strtable_get(&table, &key);

  if(n != NULL)
    return n->str;

  char* dst = (char*)pool_alloc_size(len + 1);
  memcpy(dst, string, len);
  dst[len] = '\0';

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = dst;
  n->len = len;
  n->buf_size = len + 1;

  strtable_put(&table, n);
  return n->str;
}

const char* stringtab_consume(const char* string, size_t buf_size)
{
  if(string == NULL)
    return NULL;

  size_t len = strlen(string);
  stringtab_entry_t key = {string, len, 0};
  stringtab_entry_t* n = strtable_get(&table, &key);

  if(n != NULL)
  {
    pool_free_size(buf_size, (void*)string);
    return n->str;
  }

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = string;
  n->len = len;
  n->buf_size = buf_size;

  strtable_put(&table, n);
  return n->str;
}

void stringtab_done()
{
  strtable_destroy(&table);
  memset(&table, 0, sizeof(strtable_t));
}
