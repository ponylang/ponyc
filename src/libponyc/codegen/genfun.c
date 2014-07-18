#include "genfun.h"
#include "../type/reify.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

LLVMValueRef codegen_function(compile_t* c, ast_t* ast, ast_t* typeargs)
{
  // TODO:
  ast_error(ast, "not implemented (codegen for function)");
  return NULL;
}
