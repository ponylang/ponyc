#ifndef CODEGEN_GENFFI_H
#define CODEGEN_GENFFI_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_ffi(compile_t* c, ast_t* ast);

PONY_EXTERN_C_END

#endif
