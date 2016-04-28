#ifndef CODEGEN_GENPRIM_H
#define CODEGEN_GENPRIM_H

#include <platform.h>
#include "codegen.h"
#include "gentype.h"

PONY_EXTERN_C_BEGIN

void genprim_pointer_methods(compile_t* c, reach_type_t* t);

void genprim_maybe_methods(compile_t* c, reach_type_t* t);

void genprim_array_trace(compile_t* c, reach_type_t* t);

void genprim_platform_methods(compile_t* c, reach_type_t* t);

void genprim_builtins(compile_t* c);

void genprim_reachable_init(compile_t* c, ast_t* program);

PONY_EXTERN_C_END

#endif
