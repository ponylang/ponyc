#ifndef CODEGEN_GENPRIM_H
#define CODEGEN_GENPRIM_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim);

void genprim_array_trace(compile_t* c, gentype_t* g);

void genprim_builtins(compile_t* c);

PONY_EXTERN_C_END

#endif
