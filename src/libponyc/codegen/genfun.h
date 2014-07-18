#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include "codegen.h"

LLVMValueRef codegen_function(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs);

#endif
