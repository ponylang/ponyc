#ifndef CODEGEN_H
#define CODEGEN_H

#include "../ast/ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>

typedef struct compile_t
{
  const char* filename;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMPassManagerRef fpm;
  LLVMPassManagerBuilderRef pmb;
} compile_t;

bool codegen(ast_t* program);

#endif
