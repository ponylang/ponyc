#ifndef TYPE_LOOKUP_H
#define TYPE_LOOKUP_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

ast_t* lookup(typecheck_t* t, ast_t* from, ast_t* type, const char* name);

PONY_EXTERN_C_END

#endif
