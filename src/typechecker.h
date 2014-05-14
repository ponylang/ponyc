#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "ast.h"

/// Type check
bool typecheck(ast_t* ast);

/// Checks if an identifier is a valid type name.
bool is_type_id(const char* s);

#endif
