#ifndef TYPES_H
#define TYPES_H

#include "../ast/ast.h"
#include <stdbool.h>

typedef struct type_t type_t;

type_t* type_name( ast_t* ast, const char* name );

type_t* type_ast( ast_t* ast );

bool type_valid( ast_t* ast, type_t* type );

// is A the same as B?
bool type_eq( type_t* a, type_t* b );

// is A a subtype of B?
bool type_sub( type_t* a, type_t* b );

void type_done();

#endif
