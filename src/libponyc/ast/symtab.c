#include "symtab.h"
#include "../ds/hash.h"
#include "../ds/functions.h"
#include <stdlib.h>

typedef struct symbol_t
{
  const char* name;
  void* value;
  sym_status_t status;
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
  s->status = sym->status;

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

bool symtab_add(symtab_t* symtab, const char* name, void* value,
  sym_status_t status)
{
  bool present;
  symbol_t sym = {name, value, status};
  symtab_insert(symtab, &sym, &present);

  return !present;
}

void* symtab_get(symtab_t* symtab, const char* name, sym_status_t* status)
{
  symbol_t s1 = {name, NULL, SYM_NONE};
  symbol_t* s2 = symtab_find(symtab, &s1);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    return s2->value;
  }

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
}

sym_status_t symtab_get_status(symtab_t* symtab, const char* name)
{
  symbol_t s1 = {name, NULL, SYM_NONE};
  symbol_t* s2 = symtab_find(symtab, &s1);

  return s2 != NULL ? s2->status : SYM_NONE;
}

bool symtab_set_status(symtab_t* symtab, const char* name, sym_status_t status)
{
  bool present;
  symbol_t s1 = {name, NULL, status};
  symbol_t* s2 = symtab_insert(symtab, &s1, &present);

  if(present)
    s2->status = status;

  return present;
}

bool symtab_pred(symbol_t* symbol, void* arg)
{
  // Strip out private symbols.
  return symbol->name[0] != '_';
}
