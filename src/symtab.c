#include "symtab.h"
#include "hash.h"
#include "functions.h"
#include <stdlib.h>

typedef struct symbol_t
{
  const char* name;
  void* value;
} symbol_t;

static uint64_t sym_hash(const void* data)
{
  const symbol_t* sym = data;
  return ptrhash(sym->name);
}

static bool sym_cmp(const void* a, const void* b)
{
  const symbol_t* s1 = a;
  const symbol_t* s2 = b;

  return s1->name == s2->name;
}

static void* sym_dup(const void* data)
{
  const symbol_t* s1 = data;
  symbol_t* s2 = malloc(sizeof(symbol_t));
  s2->name = s1->name;
  s2->value = s1->value;

  return s2;
}

table_t* symtab_new()
{
  return table_create(32, sym_hash, sym_cmp, sym_dup, free);
}

bool symtab_add(table_t* symtab, const char* name, void* value)
{
  bool present;
  symbol_t sym = {name, value};
  table_insert(symtab, &sym, &present);

  return !present;
}

void* symtab_get(const table_t* symtab, const char* name)
{
  symbol_t s1 = {name, NULL};
  symbol_t* s2 = table_find(symtab, &s1);

  return s2 != NULL ? s2->value : NULL;
}
