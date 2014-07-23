#ifndef CODEGEN_GENNAME_H
#define CODEGEN_GENNAME_H

#include "codegen.h"

const char* genname_type(ast_t* ast);

const char* genname_fun(const char* type, const char* name, ast_t* typeargs);

const char* genname_handler(const char* type, const char* name,
  ast_t* typeargs);

#endif
