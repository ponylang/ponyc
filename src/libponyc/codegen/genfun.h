#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef genfun_proto(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs);

LLVMValueRef genfun_fun(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs);

LLVMValueRef genfun_be(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index);

LLVMValueRef genfun_new(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs);

LLVMValueRef genfun_newbe(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index);

bool genfun_methods(compile_t* c, gentype_t* g);

uint32_t genfun_behaviour_index(gentype_t* g, const char* name);

PONY_EXTERN_C_END

#endif
