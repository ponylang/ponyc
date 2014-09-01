#ifndef CODEGEN_GENPRIM_H
#define CODEGEN_GENPRIM_H

#include "codegen.h"

LLVMTypeRef genprim(compile_t* c, ast_t* ast, bool prelim);

void genprim_builtins(compile_t* c);

#endif
