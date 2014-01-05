#ifndef SYMTAB_H
#define SYMTAB_H

#include "table.h"

table_t* symtab_new();
bool symtab_add(table_t* symtab, const char* name, void* value);
void* symtab_get(const table_t* symtab, const char* name);

#endif
