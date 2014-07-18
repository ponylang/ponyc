#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include "codegen.h"

LLVMValueRef codegen_function(compile_t* c, ast_t* ast, ast_t* typeargs);

#endif
