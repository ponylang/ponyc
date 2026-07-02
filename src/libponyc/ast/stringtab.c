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

DEFINE_LIST(strlist, strlist_t, const char, ptr_cmp, NULL);

struct stringtab_entry_t
{
  const char* str;
  size_t len;
  size_t buf_size;
};

static size_t stringtab_hash(stringtab_entry_t* a)
{
  return ponyint_hash_block(a->str, a->len);
}

static bool stringtab_cmp(stringtab_entry_t* a, stringtab_entry_t* b)
{
  return (a->len == b->len) && (memcmp(a->str, b->str, a->len) == 0);
}

static void stringtab_entry_free(stringtab_entry_t* a)
{
  ponyint_pool_free_size(a->buf_size, (char*)a->str);
  POOL_FREE(stringtab_entry_t, a);
}

DEFINE_HASHMAP(strtable, strtable_t, stringtab_entry_t, stringtab_hash,
  stringtab_cmp, stringtab_entry_free);

strtable_t* stringtab_new()
{
  strtable_t* table = POOL_ALLOC(strtable_t);
  strtable_init(table, 4096);
  return table;
}

void stringtab_free(strtable_t* table)
{
  if(table == NULL)
    return;

  strtable_destroy(table);
  POOL_FREE(strtable_t, table);
}

const char* stringtab(strtable_t* table, const char* string)
{
  if(string == NULL)
    return NULL;

  return stringtab_len(table, string, strlen(string));
}

const char* stringtab_len(strtable_t* table, const char* string, size_t len)
{
  if(string == NULL)
    return NULL;

  stringtab_entry_t key = {string, len, 0};
  size_t index = HASHMAP_UNKNOWN;
  stringtab_entry_t* n = strtable_get(table, &key, &index);

  if(n != NULL)
    return n->str;

  char* dst = (char*)ponyint_pool_alloc_size(len + 1);
  memcpy(dst, string, len);
  dst[len] = '\0';

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = dst;
  n->len = len;
  n->buf_size = len + 1;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  strtable_putindex(table, n, index);
  return n->str;
}

const char* stringtab_consume(strtable_t* table, const char* string,
  size_t buf_size)
{
  if(string == NULL)
    return NULL;

  size_t len = strlen(string);
  stringtab_entry_t key = {string, len, 0};
  size_t index = HASHMAP_UNKNOWN;
  stringtab_entry_t* n = strtable_get(table, &key, &index);

  if(n != NULL)
  {
    ponyint_pool_free_size(buf_size, (void*)string);
    return n->str;
  }

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = string;
  n->len = len;
  n->buf_size = buf_size;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  strtable_putindex(table, n, index);
  return n->str;
}
