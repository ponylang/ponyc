#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "error.h"
#include "ast.h"

bool typecheck( ast_t* ast, errorlist_t* errors );

#endif
