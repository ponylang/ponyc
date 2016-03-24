#ifndef CODEGEN_GENFUN_H
#define CODEGEN_GENFUN_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

bool genfun_method_sigs(compile_t* c, reachable_type_t* t);

bool genfun_method_bodies(compile_t* c, reachable_type_t* t);

PONY_EXTERN_C_END

#endif
