#ifndef EXPR_FFI_H
#define EXPR_FFI_H

#include <platform.h>
#include "../ast/ast.h"
#include "../ast/frame.h"

PONY_EXTERN_C_BEGIN

bool expr_ffi(ast_t* ast);

PONY_EXTERN_C_END

#endif
