#include "stringtab.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>

#include <platform.h>

static bool ptr_cmp(const char* a, const char* b)
{
  return a == b;
}

static bool str_cmp(const char* a, const char* b)
{
  return !strcmp(a, b);
}

static void str_free(const char* a)
{
  size_t len = strlen(a) + 1;
  pool_free_size(len, (char*)a);
}

DEFINE_LIST(strlist, const char, ptr_cmp, NULL);

DECLARE_HASHMAP(strtable, const char);
DEFINE_HASHMAP(strtable, const char, hash_str, str_cmp, pool_alloc_size,
  pool_free_size, str_free, NULL);

static strtable_t table;

void stringtab_init()
{
  strtable_init(&table, 4096);
}

const char* stringtab(const char* string)
{
  if(string == NULL)
    return NULL;

  const char* prev = strtable_get(&table, string);

  if(prev != NULL)
    return prev;

  size_t len = strlen(string) + 1;
  char* n = (char*)pool_alloc_size(len);
  memcpy(n, string, len);

  strtable_put(&table, n);
  return n;
}

void stringtab_done()
{
  strtable_destroy(&table);
}
