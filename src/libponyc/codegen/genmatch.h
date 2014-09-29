#ifndef CODEGEN_GENMATCH_H
#define CODEGEN_GENMATCH_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_match(compile_t* c, ast_t* ast);

PONY_EXTERN_C_END

#endif
