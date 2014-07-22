#ifndef CODEGEN_H
#define CODEGEN_H

#include "../ast/ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Analysis.h>

typedef struct compile_t
{
  const char* filename;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMPassManagerRef fpm;
  LLVMPassManagerBuilderRef pmb;
  LLVMTargetDataRef target;
  LLVMTypeRef void_ptr;
  LLVMTypeRef actor_ptr;
  LLVMTypeRef trace_type;
  LLVMTypeRef trace_fn;
  LLVMTypeRef dispatch_fn;
  LLVMTypeRef final_fn;
  LLVMTypeRef descriptor_ptr;
  LLVMTypeRef object_ptr;
} compile_t;

bool codegen(ast_t* program, int opt, bool print_llvm);

bool codegen_finishfun(compile_t* c, LLVMValueRef fun);

#endif
