#ifndef MATCHTYPE_H
#define MATCHTYPE_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

bool is_match_compatible(ast_t* match, ast_t* pattern);

PONY_EXTERN_C_END

#endif
