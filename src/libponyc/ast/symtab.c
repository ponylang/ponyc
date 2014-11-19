#include "symtab.h"
#include "../ds/stringtab.h"
#include "../ds/hash.h"
#include "../ds/functions.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

typedef struct symbol_t
{
  const char* name;
  void* value;
  sym_status_t status;
  size_t branch_count;
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
  s->branch_count = sym->branch_count;

  return s;
}

static void sym_free(symbol_t* sym)
{
  free(sym);
}

static bool pred_inherit(symbol_t* sym, void* arg)
{
  // Only inherit symbols that were declared in an outer scope.
  return sym->value == NULL;
}

static bool resolve_inherit(symbol_t* dst, symbol_t* src, void* arg)
{
  // Propagate the source status to the destination.
  dst->status = src->status;
  return true;
}

static bool pred_branch(symbol_t* sym, void* arg)
{
  // Only inherit symbols that were declared in an outer scope.
  if(sym->value != NULL)
    return false;

  switch(sym->status)
  {
    case SYM_DEFINED:
      // If we are defined, we're really undefined with a branch count of 1.
      sym->status = SYM_UNDEFINED;
      sym->branch_count = 1;
      return true;

    case SYM_UNDEFINED:
    case SYM_CONSUMED:
      assert(sym->branch_count == 0);
      return true;

    default: {}
  }

  assert(0);
  return false;
}

static bool resolve_branch(symbol_t* dst, symbol_t* src, void* arg)
{
  // If the source is defined, increase the branch count at the destination.
  switch(src->status)
  {
    case SYM_CONSUMED:
      dst->status = SYM_CONSUMED;
      return true;

    case SYM_UNDEFINED:
      // Branch count should always be 0 or 1.
      assert(src->branch_count <= 1);

      // Should be undefined in the parent scope as well.
      assert(dst->status == SYM_UNDEFINED);

      dst->branch_count += src->branch_count;
      return true;

    default: {}
  }

  assert(0);
  return false;
}

static bool apply_branch(symbol_t* sym, void* arg)
{
  if(sym->status == SYM_UNDEFINED)
  {
    size_t count = *(size_t*)arg;
    assert(sym->branch_count <= count);

    if(sym->branch_count == count)
    {
      sym->status = SYM_DEFINED;
    } else {
      sym->branch_count = 0;
    }
  }

  return true;
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

DEFINE_TABLE(symtab, symbol_t, sym_hash, sym_cmp, sym_dup, sym_free);

bool is_type_name(const char* name)
{
  if(*name == '_')
    name++;

  return (*name >= 'A') && (*name <= 'Z');
}

symtab_t* symtab_new()
{
  return symtab_create(32);
}

bool symtab_add(symtab_t* symtab, const char* name, void* value,
  sym_status_t status)
{
  const char* no_case = name_without_case(name);
  bool present;

  if(no_case != name)
  {
    symbol_t case_marker = {no_case, value, SYM_NOCASE};
    symtab_insert(symtab, &case_marker, &present);

    if(present)
      return false;
  }

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

    if(s2->status == SYM_NOCASE)
      return NULL;

    return s2->value;
  }

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
}

void* symtab_get_case(symtab_t* symtab, const char* name, sym_status_t* status)
{
  // Same as symtab_get, but is partially case insensitive. That is, type names
  // are compared as uppercase and other symbols are compared as lowercase.
  symbol_t s1 = {name, NULL, SYM_NONE};
  symbol_t* s2 = symtab_find(symtab, &s1);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    return s2->value;
  }

  const char* no_case = name_without_case(name);

  if(no_case != name)
    return symtab_get_case(symtab, no_case, status);

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
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

void symtab_inherit_status(symtab_t* dst, symtab_t* src)
{
  symtab_merge(dst, src, pred_inherit, NULL, resolve_inherit, NULL);
}

void symtab_inherit_branch(symtab_t* dst, symtab_t* src)
{
  symtab_merge(dst, src, pred_branch, NULL, resolve_branch, NULL);
}

void symtab_consolidate_branches(symtab_t* symtab, size_t count)
{
  symtab_apply(symtab, apply_branch, &count);
}

bool symtab_no_private(symbol_t* symbol, void* arg)
{
  // Strip out private symbols.
  return symbol->name[0] != '_';
}

static bool print_symbol(symbol_t* symbol, void* arg)
{
  printf("%s\n", symbol->name);
  return true;
}

void symtab_print(symtab_t* symtab)
{
  symtab_apply(symtab, print_symbol, NULL);
}
