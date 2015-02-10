#ifndef SYMTAB_H
#define SYMTAB_H

#include <platform.h>
#include "../../libponyrt/ds/hash.h"

PONY_EXTERN_C_BEGIN

typedef enum
{
  SYM_NONE,
  SYM_NOCASE,
  SYM_DEFINED,
  SYM_UNDEFINED,
  SYM_CONSUMED
} sym_status_t;

typedef struct symbol_t
{
  const char* name;
  void* value;
  sym_status_t status;
  size_t branch_count;
} symbol_t;

DECLARE_HASHMAP(symtab, symbol_t);

bool is_type_name(const char* name);

symtab_t* symtab_new();

symtab_t* symtab_dup(symtab_t* symtab);

void symtab_free(symtab_t* symtab);

bool symtab_add(symtab_t* symtab, const char* name, void* value,
  sym_status_t status);

void* symtab_find(symtab_t* symtab, const char* name, sym_status_t* status);

void* symtab_find_case(symtab_t* symtab, const char* name,
  sym_status_t* status);

sym_status_t symtab_get_status(symtab_t* symtab, const char* name);

void symtab_set_status(symtab_t* symtab, const char* name,
  sym_status_t status);

void symtab_inherit_status(symtab_t* dst, symtab_t* src);

void symtab_inherit_branch(symtab_t* dst, symtab_t* src);

bool symtab_merge_public(symtab_t* dst, symtab_t* src);

PONY_EXTERN_C_END

#endif
