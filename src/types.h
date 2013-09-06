#ifndef TYPES_H
#define TYPES_H

#include "ast.h"
#include <stdbool.h>

typedef struct type_t type_t;

type_t* type_ast( ast_t* ast );
bool type_valid( type_t* type );
bool type_eq( type_t* a, type_t* b );
bool type_sub( type_t* a, type_t* b );
void type_done();

#endif
