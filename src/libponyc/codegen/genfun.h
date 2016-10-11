#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

void genfun_param_attrs(compile_t* c, reach_type_t* t, reach_method_t* m,
  LLVMValueRef fun);

bool genfun_method_sigs(compile_t* c, reach_type_t* t);

bool genfun_method_bodies(compile_t* c, reach_type_t* t);

PONY_EXTERN_C_END

#endif
