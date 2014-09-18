#ifndef SYMTAB_H
#define SYMTAB_H

#include "../ds/table.h"

typedef enum
{
  SYM_NONE,
  SYM_UNDEFINED,
  SYM_DEFINED,
  SYM_CONSUMED
} sym_status_t;

typedef struct symbol_t symbol_t;
DECLARE_TABLE(symtab, symbol_t);

symtab_t* symtab_new();

bool symtab_add(symtab_t* symtab, const char* name, void* value,
  sym_status_t status);

void* symtab_get(symtab_t* symtab, const char* name, sym_status_t* status);

sym_status_t symtab_get_status(symtab_t* symtab, const char* name);

bool symtab_set_status(symtab_t* symtab, const char* name, sym_status_t status);

void symtab_inherit_undefined(symtab_t* dst, symtab_t* src);

bool symtab_no_private(symbol_t* symbol, void* arg);

#endif
