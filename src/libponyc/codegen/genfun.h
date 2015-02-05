#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef genfun_proto(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs);

bool genfun_methods(compile_t* c, gentype_t* g);

uint32_t genfun_vtable_size(compile_t* c, gentype_t* g);

uint32_t genfun_vtable_index(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs);

PONY_EXTERN_C_END

#endif
