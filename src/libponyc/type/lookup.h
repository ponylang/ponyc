#ifndef TYPE_LOOKUP_H
#define TYPE_LOOKUP_H

#include "../ast/ast.h"

ast_t* lookup(ast_t* scope, ast_t* type, const char* name);

#endif
