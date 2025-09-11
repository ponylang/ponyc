#include "symtab.h"
#include "stringtab.h"
#include "ast.h"
#include "id.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static size_t sym_hash(symbol_t* sym)
{
  return ponyint_hash_ptr(sym->name);
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

DEFINE_HASHMAP_SERIALISE(symtab, symtab_t, symbol_t, sym_hash, sym_cmp,
  sym_free, symbol_pony_type());

static const char* name_without_case(const char* name)
{
  size_t len = strlen(name) + 1;
  char* buf = (char*)ponyint_pool_alloc_size(len);

  if(is_name_type(name))
  {
    for(size_t i = 0; i < len; i++)
      buf[i] = (char)toupper(name[i]);
  } else {
    for(size_t i = 0; i < len; i++)
      buf[i] = (char)tolower(name[i]);
  }

  return stringtab_consume(buf, len);
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

bool symtab_add(symtab_t* symtab, const char* name, ast_t* def,
  sym_status_t status)
{
  const char* no_case = name_without_case(name);

  if(no_case != name)
  {
    symbol_t s1 = {no_case, def, SYM_NOCASE, 0};
    size_t index = HASHMAP_UNKNOWN;
    symbol_t* s2 = symtab_get(symtab, &s1, &index);

    if(s2 != NULL)
      return false;

    // didn't find it in the map but index is where we can put the
    // new one without another search
    symtab_putindex(symtab, sym_dup(&s1), index);
  }

  symbol_t s1 = {name, def, status, 0};
  size_t index = HASHMAP_UNKNOWN;
  symbol_t* s2 = symtab_get(symtab, &s1, &index);

  if(s2 != NULL)
    return false;

  // didn't find it in the map but index is where we can put the
  // new one without another search
  symtab_putindex(symtab, sym_dup(&s1), index);
  return true;
}

ast_t* symtab_find(symtab_t* symtab, const char* name, sym_status_t* status)
{
  symbol_t s1 = {name, NULL, SYM_NONE, 0};
  size_t index = HASHMAP_UNKNOWN;
  symbol_t* s2 = symtab_get(symtab, &s1, &index);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    if(s2->status == SYM_NOCASE)
      return NULL;

    return s2->def;
  }

  if(status != NULL)
    *status = SYM_NONE;

  return NULL;
}

ast_t* symtab_find_case(symtab_t* symtab, const char* name,
  sym_status_t* status)
{
  // Same as symtab_get, but is partially case insensitive. That is, type names
  // are compared as uppercase and other symbols are compared as lowercase.
  symbol_t s1 = {name, NULL, SYM_NONE, 0};
  size_t index = HASHMAP_UNKNOWN;
  symbol_t* s2 = symtab_get(symtab, &s1, &index);

  if(s2 != NULL)
  {
    if(status != NULL)
      *status = s2->status;

    return s2->def;
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
  size_t index = HASHMAP_UNKNOWN;
  symbol_t* s2 = symtab_get(symtab, &s1, &index);

  if(s2 != NULL)
    s2->status = status;
  else
  {
    // didn't find it in the map but index is where we can put the
    // new one without another search
    symtab_putindex(symtab, sym_dup(&s1), index);
  }
}

void symtab_inherit_status(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;
  size_t index = HASHMAP_UNKNOWN;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    // Only inherit symbols that were declared in an outer scope.
    if(sym->def != NULL)
      continue;

    symbol_t* dsym = symtab_get(dst, sym, &index);

    if(dsym != NULL)
    {
      // Copy the source status the the destination.
      dsym->status = sym->status;
    } else {
      // Add this symbol to the destination.
      // didn't find it in the map but index is where we can put the
      // new one without another search
      symtab_putindex(dst, sym_dup(sym), index);
    }
  }
}

void symtab_inherit_branch(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;
  size_t index = HASHMAP_UNKNOWN;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    // Only inherit symbols that were declared in an outer scope.
    if(sym->def != NULL)
      continue;

    symbol_t* dsym = symtab_get(dst, sym, &index);

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
      } else if(sym->status == SYM_CONSUMED_SAME_EXPR) {
        // Consumed overrides everything.
        dsym->status = SYM_CONSUMED_SAME_EXPR;
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

      // didn't find it in the map but index is where we can put the
      // new one without another search
      symtab_putindex(dst, dsym, index);
    }
  }
}

bool symtab_can_merge_public(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    if(is_name_private(sym->name) ||
      (sym->status == SYM_NOCASE) ||
      !strcmp(sym->name, "Main"))
      continue;

    if(symtab_find_case(dst, sym->name, NULL) != NULL)
      return false;
  }

  return true;
}

bool symtab_merge_public(symtab_t* dst, symtab_t* src)
{
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(src, &i)) != NULL)
  {
    if(is_name_private(sym->name) ||
      (sym->status == SYM_NOCASE) ||
      !strcmp(sym->name, "Main"))
      continue;

    if(!symtab_add(dst, sym->name, sym->def, sym->status))
      return false;
  }

  return true;
}

bool symtab_check_all_defined(symtab_t* symtab, errors_t* errors)
{
  bool r = true;
  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(symtab, &i)) != NULL)
  {
    // Ignore entries with a NULL def field, that means it was not declared in
    // this scope
    if(sym->def != NULL && sym->status == SYM_UNDEFINED)
    {
      ast_error(errors, sym->def,
        "Local variable %s is not assigned a value in all code paths", sym->name);
      r = false;
    }
  }

  return r;
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

      case SYM_CONSUMED_SAME_EXPR:
        printf(": consumed same expression\n");
        break;

      default:
        printf(": UNKNOWN\n");
        break;
    }
  }
}

static void symbol_serialise_trace(pony_ctx_t* ctx, void* object)
{
  symbol_t* sym = (symbol_t*)object;

  string_trace(ctx, (char*)sym->name);

  if(sym->def != NULL)
    pony_traceknown(ctx, sym->def, ast_pony_type(), PONY_TRACE_MUTABLE);
}

static void symbol_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  symbol_t* sym = (symbol_t*)object;
  symbol_t* dst = (symbol_t*)((uintptr_t)buf + offset);

  dst->name = (const char*)pony_serialise_offset(ctx, (char*)sym->name);
  dst->def = (ast_t*)pony_serialise_offset(ctx, sym->def);
  dst->status = sym->status;
  dst->branch_count = sym->branch_count;
}

static void symbol_deserialise(pony_ctx_t* ctx, void* object)
{
  (void)ctx;
  symbol_t* sym = (symbol_t*)object;

  sym->name = string_deserialise_offset(ctx, (uintptr_t)sym->name);
  sym->def = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)sym->def);
}

static pony_type_t symbol_pony =
{
  0,
  sizeof(symbol_t),
  0,
  0,
  0,
  NULL,
#if defined(USE_RUNTIME_TRACING)
  NULL,
  NULL,
#endif
  NULL,
  symbol_serialise_trace,
  symbol_serialise,
  symbol_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* symbol_pony_type()
{
  return &symbol_pony;
}
