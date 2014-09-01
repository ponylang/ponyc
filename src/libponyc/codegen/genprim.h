#ifndef CODEGEN_GENPRIM_H
#define CODEGEN_GENPRIM_H

#include "codegen.h"
#include "gentype.h"

bool genprim_pointer(compile_t* c, gentype_t* g, bool prelim);

void genprim_array_trace(compile_t* c, gentype_t* g);

void genprim_builtins(compile_t* c);

#endif
