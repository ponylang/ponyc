#ifndef CODEGEN_GENIDENT_H
#define CODEGEN_GENIDENT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_is(compile_t* c, ast_t* ast);

LLVMValueRef gen_isnt(compile_t* c, ast_t* ast);

void gen_is_tuple_fun(compile_t* c, reach_type_t* t);

PONY_EXTERN_C_END

#endif
