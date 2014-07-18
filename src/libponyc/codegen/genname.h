#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include "codegen.h"

const char* codegen_typename(ast_t* ast);

const char* codegen_funname(ast_t* type, const char* name, ast_t* typeargs);

#endif
