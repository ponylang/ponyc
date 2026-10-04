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

DEFINE_HASHMAP(strtable_inner, strtable_inner_t, stringtab_entry_t,
  stringtab_hash, stringtab_cmp, stringtab_entry_free);

static void strtable_lock(strtable_t* table)
{
#ifdef PLATFORM_IS_POSIX_BASED
  pthread_mutex_lock(&table->lock);
#else
  EnterCriticalSection(&table->lock);
#endif
}

static void strtable_unlock(strtable_t* table)
{
#ifdef PLATFORM_IS_POSIX_BASED
  pthread_mutex_unlock(&table->lock);
#else
  LeaveCriticalSection(&table->lock);
#endif
}

strtable_t* stringtab_new()
{
  strtable_t* table = POOL_ALLOC(strtable_t);
  strtable_inner_init(&table->map, 4096);
#ifdef PLATFORM_IS_POSIX_BASED
  pthread_mutex_init(&table->lock, NULL);
#else
  InitializeCriticalSection(&table->lock);
#endif
  return table;
}

void stringtab_free(strtable_t* table)
{
  if(table == NULL)
    return;

  strtable_inner_destroy(&table->map);
#ifdef PLATFORM_IS_POSIX_BASED
  pthread_mutex_destroy(&table->lock);
#else
  DeleteCriticalSection(&table->lock);
#endif
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

  strtable_lock(table);

  stringtab_entry_t* n = strtable_inner_get(&table->map, &key, &index);

  if(n != NULL)
  {
    const char* result = n->str;
    strtable_unlock(table);
    return result;
  }

  char* dst = (char*)ponyint_pool_alloc_size(len + 1);
  memcpy(dst, string, len);
  dst[len] = '\0';

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = dst;
  n->len = len;
  n->buf_size = len + 1;

  strtable_inner_putindex(&table->map, n, index);
  strtable_unlock(table);
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

  strtable_lock(table);

  stringtab_entry_t* n = strtable_inner_get(&table->map, &key, &index);

  if(n != NULL)
  {
    const char* result = n->str;
    strtable_unlock(table);
    ponyint_pool_free_size(buf_size, (void*)string);
    return result;
  }

  n = POOL_ALLOC(stringtab_entry_t);
  n->str = string;
  n->len = len;
  n->buf_size = buf_size;

  strtable_inner_putindex(&table->map, n, index);
  strtable_unlock(table);
  return n->str;
}
