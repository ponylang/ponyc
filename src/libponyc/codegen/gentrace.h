#ifndef CODEGEN_GENTRACE_H
#define CODEGEN_GENTRACE_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

bool gentrace(compile_t* c, LLVMValueRef actor, LLVMValueRef value,
  ast_t* type);

PONY_EXTERN_C_END

#endif
