#include "stringtab.h"
#include "hash.h"
#include "table.h"
#include <stdlib.h>
#include <string.h>

bool str_cmp(const char* a, const char* b)
{
  return a == b;
}

uint64_t str_hash(const char* a)
{
  return ptrhash(a);
}

const char* str_dup(const char* a)
{
  return strdup(a);
}

void str_free(const char* a)
{
  free((char*)a);
}

DEFINE_LIST(strlist, const char, str_hash, str_cmp, NULL);
DEFINE_TABLE(strtable, const char, str_hash, str_cmp, str_dup, str_free);

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
