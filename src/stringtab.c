#include "stringtab.h"
#include "hash.h"
#include "table.h"
#include <stdlib.h>
#include <string.h>

static table_t* table;

static bool string_cmp(const void* a, const void* b)
{
  return strcmp(a, b) == 0;
}

static void* string_dup(const void* a)
{
  return strdup(a);
}

const char* stringtab(const char* string)
{
  if(string == NULL)
    return NULL;

  if(table == NULL)
    table = table_create(4096, strhash, string_cmp, string_dup, free);

  bool present;
  return table_insert(table, (void*)string, &present);
}

void stringtab_done()
{
  table_free(table);
}
