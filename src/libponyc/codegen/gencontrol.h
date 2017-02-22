#ifndef CODEGEN_GENCONTROL_H
#define CODEGEN_GENCONTROL_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_seq(compile_t* c, ast_t* ast);

LLVMValueRef gen_if(compile_t* c, ast_t* ast);

LLVMValueRef gen_while(compile_t* c, ast_t* ast);

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast);

LLVMValueRef gen_break(compile_t* c, ast_t* ast);

LLVMValueRef gen_continue(compile_t* c, ast_t* ast);

LLVMValueRef gen_return(compile_t* c, ast_t* ast);

LLVMValueRef gen_try(compile_t* c, ast_t* ast);

LLVMValueRef gen_error(compile_t* c, ast_t* ast);

void attach_branchweights_metadata(LLVMContextRef ctx, LLVMValueRef branch,
   unsigned int weights[], unsigned int count);

void handle_branch_prediction_default(LLVMContextRef ctx, LLVMValueRef branch,
  ast_t* ast);

// Numbers from Clang's __builtin_expect()
#if PONY_LLVM < 309
#  define PONY_BRANCHWEIGHT_LIKELY 64
#  define PONY_BRANCHWEIGHT_UNLIKELY 4
#else
#  define PONY_BRANCHWEIGHT_LIKELY 2000
#  define PONY_BRANCHWEIGHT_UNLIKELY 1
#endif

PONY_EXTERN_C_END

#endif
