#ifndef SYMTAB_H
#define SYMTAB_H

#include "types.h"
#include <stdbool.h>

typedef struct symtab_t symtab_t;

symtab_t* symtab_new( symtab_t* parent );
void symtab_ref( symtab_t* symtab );
void symtab_free( symtab_t* symtab );

bool symtab_add( symtab_t* symtab, const char* name, const type_t* type );
const type_t* symtab_get( symtab_t* symtab, const char* name );

#endif
