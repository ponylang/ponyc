#include "symtab.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

static uint64_t sym_hash(symbol_t* sym)
{
  return hash_ptr(sym->name);
}

static bool sym_cmp(symbol_t* a, symbol_t* b)
{
  return a->name == b->name;
}

static symbol_t* sym_dup(symbol_t* sym)
{
  symbol_t* s = POOL_ALLOC(symbol_t);
  memcpy(s, sym, sizeof(symbol_t));
  return s;
}

static void sym_free(symbol_t* sym)
{
  POOL_FREE(symbol_t, sym);
}

static const char* name_without_case(const char* name)
{
  size_t len = strlen(name) + 1;
  VLA(char, buf, len);

  if(is_type_name(name))
  {
    for(size_t i = 0; i < len; i++)
      buf[i] = (char)toupper(name[i]);
  } else {
    for(size_t i = 0; i < len; i++)
      buf[i] = (char)tolower(name[i]);
  }

  return stringtab(buf);
}

DEFINE_HASHMAP(symtab, symbol_t, sym_hash, sym_cmp, pool_alloc_size,
  pool_free_size, sym_free);

bool is_type_name(const char* name)
{
  if(*name == '_')
    name++;

  return (*name >= 'A') && (*name <= 'Z');
}

symtab_t* symtab_new()
{
  symtab_t* symtab = POOL_ALLOC(symtab_t);
  symtab_init(symtab, 8);
  return symtab;
}

symtab_t* symtab_dup(symtab_t* symtab)
{
  symtab_t* n = POOL_ALLOC(symtab_t);
  symtab_init(n, symtab_size(symtab));

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(symtab, &i)) != NULL)
    symtab_put(n, sym_dup(sym));

  return n;
}

void symtab_free(symtab_t* symtab)
{
  if(symtab == NULL)
    return;

  symtab_destroy(symtab);
  POOL_FREE(symtab_t, symtab);
}

bool symtab_add(symtab_t* symtab, const char* name, void* value,
  sym_status_t status)
{
  const char* no_case = name_without_case(name);

  if(no_case != name)
  {
    symbol_t s1 = {no_case, value, SYM_NOCASE, 0};
    symbol_t* s2 = symtab_get(symtab, &s1);

    if(s2 != NULL)
      return false;

    symtab_put(symtab, sym_dup(&s1));
  }

  symbol_t s1 = {name, value, status, 0};
  symbol_t* s2 = symtab_get(symtab, &s1);

  if(s2 != NULL)
    return false;

  symtab_put(symtab, sym_dup(&s1));
  return true;
}

void* symtab_find(symtab_t* symtab, const char* name, sym_status_t* status)
{
  symbol_t s1 = {name, NULL, SYM_NONE, 0};
  symbol_t* s2 = symtab_get(symtab, &s1);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    if(s2->status == SYM_NOCASE)
      return NULL;

    return s2->value;
  }

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
}

void* symtab_find_case(symtab_t* symtab, const char* name,
  sym_status_t* status)
{
  // Same as symtab_get, but is partially case insensitive. That is, type names
  // are compared as uppercase and other symbols are compared as lowercase.
  symbol_t s1 = {name, NULL, SYM_NONE, 0};
  symbol_t* s2 = symtab_get(symtab, &s1);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    return s2->value;
  }

  const char* no_case = name_without_case(name);

  if(no_case != name)
    return symtab_find_case(symtab, no_case, status);

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
}

void symtab_set_status(symtab_t* symtab, const char* name, sym_status_t status)
{
  symbol_t s1 = {name, NULL, status, 0};
  symbol_t* s2 = symtab_get(symtab, &s1);

  if(s2 != NULL)
    s2->status = status;
  else
    symtab_put(symtab, sym_dup(&s1));
}

void symtab_inherit_status(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    // Only inherit symbols that were declared in an outer scope.
    if(sym->value != NULL)
      continue;

    symbol_t* dsym = symtab_get(dst, sym);

    if(dsym != NULL)
    {
      // Copy the source status the the destination.
      dsym->status = sym->status;
    } else {
      // Add this symbol to the destination.
      symtab_put(dst, sym_dup(sym));
    }
  }
}

void symtab_inherit_branch(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    // Only inherit symbols that were declared in an outer scope.
    if(sym->value != NULL)
      continue;

    symbol_t* dsym = symtab_get(dst, sym);

    if(dsym != NULL)
    {
      if(sym->status == SYM_DEFINED)
      {
        // Treat defined as adding one to the undefined branch count.
        if(dsym->status == SYM_UNDEFINED)
          dsym->branch_count++;
      } else if(sym->status == SYM_CONSUMED) {
        // Consumed overrides everything.
        dsym->status = SYM_CONSUMED;
        dsym->branch_count = 0;
      }
    } else {
      // Add this symbol to the destination.
      dsym = sym_dup(sym);

      if(dsym->status == SYM_DEFINED)
      {
        // Treat defined as undefined with a branch count of one.
        dsym->status = SYM_UNDEFINED;
        dsym->branch_count = 1;
      }

      symtab_put(dst, dsym);
    }
  }
}

bool symtab_merge_public(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    if((sym->name[0] == '_') || (sym->status == SYM_NOCASE))
      continue;

    if(!symtab_add(dst, sym->name, sym->value, sym->status))
      return false;
  }

  return true;
}

void symtab_print(symtab_t* symtab)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(symtab, &i)) != NULL)
  {
    printf("%s", sym->name);

    switch(sym->status)
    {
      case SYM_UNDEFINED:
        printf(": undefined\n");
        break;

      case SYM_DEFINED:
        printf(": defined\n");
        break;

      case SYM_CONSUMED:
        printf(": consumed\n");
        break;

      default:
        printf(": UNKNOWN\n");
        break;
    }
  }
}
