#ifndef TYPECHECKER_H
#define TYPECHECKER_H

#include "ast.h"

bool typecheck_init( ast_t* program );
bool typecheck( ast_t* ast );

#endif
