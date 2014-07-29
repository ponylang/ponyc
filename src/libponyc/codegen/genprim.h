#ifndef CODEGEN_GENPRIM_H
#define CODEGEN_GENPRIM_H

#include "codegen.h"

LLVMTypeRef genprim_array(compile_t* c, ast_t* ast);

LLVMTypeRef genprim(compile_t* c, ast_t* ast);

#endif
