#ifndef CODEGEN_GENEXE_H
#define CODEGEN_GENEXE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_main(compile_t* c, reach_type_t* t_main, reach_type_t* t_env);

bool genexe(compile_t* c, ast_t* program);

PONY_EXTERN_C_END

#endif
