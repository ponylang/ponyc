#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

typedef struct compile_method_t
{
  compile_opaque_free_fn free_fn;

  LLVMTypeRef func_type;
  LLVMTypeRef msg_type;
  LLVMValueRef func;
  LLVMValueRef func_handler;
  LLVMMetadataRef di_method;
  LLVMMetadataRef di_file;
} compile_method_t;

void genfun_param_attrs(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMValueRef fun);

void genfun_allocate_compile_methods(compile_t* c, reach_type_t* t);

bool genfun_method_sigs(compile_t* c, reach_type_t* t);

bool genfun_method_bodies(compile_t* c, reach_type_t* t);

void genfun_primitive_calls(compile_t* c);

PONY_EXTERN_C_END

#endif
