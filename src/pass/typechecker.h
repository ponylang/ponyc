#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "../ast/ast.h"

/// Type check
bool typecheck(ast_t* ast, int verbose);

#endif
