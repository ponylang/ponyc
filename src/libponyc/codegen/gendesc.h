#ifndef CODEGEN_GENDESC_H
#define CODEGEN_GENDESC_H

#include "codegen.h"
#include "gentype.h"

LLVMTypeRef gendesc_type(compile_t* c, const char* name, int vtable_size);

void gendesc_init(compile_t* c, gentype_t* g);

#endif
