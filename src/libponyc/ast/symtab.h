#ifndef SYMTAB_H
#define SYMTAB_H

#include "../ds/table.h"

typedef struct symbol_t symbol_t;
DECLARE_TABLE(symtab, symbol_t);

symtab_t* symtab_new();
bool symtab_add(symtab_t* symtab, const char* name, void* value);
void* symtab_get(symtab_t* symtab, const char* name);

#endif
