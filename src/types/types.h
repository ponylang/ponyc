#ifndef TYPES_H
#define TYPES_H

#include "../ast/ast.h"
#include "../ds/list.h"
#include <stdbool.h>

typedef struct type_t type_t;

DECLARE_LIST(typelist, type_t);

bool typelist_sub(typelist_t* a, typelist_t* b);

// FIX: is this needed? type_t for an unqualified nominal type?
type_t* type_name(ast_t* ast, const char* name);

// type_t for a TK_TYPEDEF
type_t* type_ast(ast_t* ast);

// is A the same as B?
bool type_eq(type_t* a, type_t* b);

// is A a subtype of B?
bool type_sub(type_t* a, type_t* b);

uint64_t type_hash(type_t* t);

void type_done();

#endif
