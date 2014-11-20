#include "stringtab.h"
#include "table.h"
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

static const char* str_dup(const char* a)
{
  size_t len = strlen(a) + 1;
  char* n = (char*)pool_alloc_size(len);
  memcpy(n, a, len);

  return n;
}

static void str_free(const char* a)
{
  size_t len = strlen(a) + 1;
  pool_free_size(len, (char*)a);
}

DEFINE_LIST(strlist, const char, ptr_cmp, NULL);
DEFINE_TABLE(strtable, const char, hash_str, str_cmp, str_dup, str_free);

static strtable_t* table;

const char* stringtab(const char* string)
{
  if(string == NULL)
    return NULL;

  if(table == NULL)
    table = strtable_create(4096);

  bool present;
  return strtable_insert(table, string, &present);
}

void stringtab_done()
{
  strtable_free(table);
}
