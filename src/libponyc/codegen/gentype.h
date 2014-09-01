#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include "codegen.h"

LLVMTypeRef gentype_prelim(compile_t* c, ast_t* ast);

LLVMTypeRef gentype(compile_t* c, ast_t* ast);

#endif
