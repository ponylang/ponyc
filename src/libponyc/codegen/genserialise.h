#ifndef CODEGEN_GENSERIALISE_H
#define CODEGEN_GENSERIALISE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

void genserialise_typeid(compile_t* c, reach_type_t* t, LLVMValueRef offset);

bool genserialise(compile_t* c, reach_type_t* t);

PONY_EXTERN_C_END

#endif
