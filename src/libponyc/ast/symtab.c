#include "symtab.h"
#include "../ds/hash.h"
#include "../ds/functions.h"
#include <stdlib.h>

typedef struct symbol_t
{
  const char* name;
  void* value;
} symbol_t;

static uint64_t sym_hash(symbol_t* sym)
{
  return ptrhash(sym->name);
}

static bool sym_cmp(symbol_t* a, symbol_t* b)
{
  return a->name == b->name;
}

static symbol_t* sym_dup(symbol_t* sym)
{
  symbol_t* s = (symbol_t*)malloc(sizeof(symbol_t));
  s->name = sym->name;
  s->value = sym->value;

  return s;
}

static void sym_free(symbol_t* sym)
{
  free(sym);
}

DEFINE_TABLE(symtab, symbol_t, sym_hash, sym_cmp, sym_dup, sym_free);

symtab_t* symtab_new()
{
  return symtab_create(32);
}

bool symtab_add(symtab_t* symtab, const char* name, void* value)
{
  bool present;
  symbol_t sym = {name, value};
  symtab_insert(symtab, &sym, &present);

  return !present;
}

void* symtab_get(symtab_t* symtab, const char* name)
{
  symbol_t s1 = {name, NULL};
  symbol_t* s2 = symtab_find(symtab, &s1);

  return s2 != NULL ? s2->value : NULL;
}

bool symtab_pred(symbol_t* symbol, void* arg)
{
  // Strip out private symbols.
  return symbol->name[0] != '_';
}
