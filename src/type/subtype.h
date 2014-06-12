#ifndef SUBTYPE_H
#define SUBTYPE_H

#include "../ast/ast.h"

bool is_subtype(ast_t* sub, ast_t* super);

bool is_eqtype(ast_t* a, ast_t* b);

#endif
