#include "codegen.h"
#include <llvm-c/Core.h>

ast_result_t pass_codegen(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    default: {}
  }

  return AST_OK;
}
