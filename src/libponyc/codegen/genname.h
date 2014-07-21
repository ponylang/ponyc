#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include "codegen.h"

const char* codegen_typename(ast_t* ast);

const char* codegen_funname(const char* type, const char* name,
  ast_t* typeargs);

#endif
