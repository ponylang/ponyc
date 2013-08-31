#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdbool.h>

typedef struct symtab_t symtab_t;

symtab_t* symtab_new();
void symtab_free( symtab_t* symtab );

bool symtab_add( symtab_t* symtab, const char* name, void* value );
void* symtab_get( symtab_t* symtab, const char* name );
bool symtab_merge( symtab_t* dst, symtab_t* src );

#endif
