#ifndef CODEGEN_GENDESC_H
#define CODEGEN_GENDESC_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

LLVMTypeRef gendesc_type(compile_t* c, const char* name, int vtable_size);

void gendesc_init(compile_t* c, gentype_t* g);

PONY_EXTERN_C_END

#endif
