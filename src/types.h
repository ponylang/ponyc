#ifndef TYPES_H
#define TYPES_H

#include "ast.h"
#include <stdbool.h>

typedef struct type_t type_t;

type_t* type_name( ast_t* ast, const char* name );
type_t* type_ast( ast_t* ast );
bool type_valid( ast_t* ast, type_t* type );
bool type_eq( const type_t* a, const type_t* b );
bool type_sub( const type_t* a, const type_t* b );
void type_print( const type_t* a );
void type_done();

#endif
