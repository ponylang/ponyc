#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include "codegen.h"

const char* codegen_typename(ast_t* ast);

const char* codegen_funname(LLVMTypeRef type, ast_t* ast, ast_t* typeargs);

#endif
