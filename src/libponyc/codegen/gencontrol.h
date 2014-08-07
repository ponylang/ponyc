#ifndef CODEGEN_GENCONTROL_H
#define CODEGEN_GENCONTROL_H

#include "codegen.h"

LLVMValueRef gen_seq(compile_t* c, ast_t* ast);

LLVMValueRef gen_if(compile_t* c, ast_t* ast);

LLVMValueRef gen_while(compile_t* c, ast_t* ast);

LLVMValueRef gen_repeat(compile_t* c, ast_t* ast);

LLVMValueRef gen_break(compile_t* c, ast_t* ast);

LLVMValueRef gen_continue(compile_t* c, ast_t* ast);

LLVMValueRef gen_return(compile_t* c, ast_t* ast);

LLVMValueRef gen_try(compile_t* c, ast_t* ast);

LLVMValueRef gen_error(compile_t* c, ast_t* ast);

#endif
