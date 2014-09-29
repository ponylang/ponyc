#ifndef CODEGEN_H
#define CODEGEN_H

#include "colour.h"
#include "../pass/pass.h"
#include "../ast/ast.h"

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Analysis.h>

// Missing from C API.
char* LLVMGetHostCPUName();

PONY_EXTERN_C_BEGIN

#define GEN_NOVALUE ((LLVMValueRef)1)

typedef struct compile_frame_t
{
  LLVMValueRef fun;
  LLVMBasicBlockRef restore_builder;

  struct compile_frame_t* prev;
} compile_frame_t;

typedef struct compile_t
{
  painter_t* painter;
  const char* filename;
  uint32_t next_type_id;
  bool release;

  LLVMContextRef context;
  LLVMTargetDataRef target_data;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMPassManagerRef fpm;

  LLVMTypeRef void_type;
  LLVMTypeRef i1;
  LLVMTypeRef i8;
  LLVMTypeRef i16;
  LLVMTypeRef i32;
  LLVMTypeRef i64;
  LLVMTypeRef i128;
  LLVMTypeRef f32;
  LLVMTypeRef f64;
  LLVMTypeRef intptr;

  LLVMTypeRef void_ptr;
  LLVMTypeRef descriptor_type;
  LLVMTypeRef descriptor_ptr;
  LLVMTypeRef object_type;
  LLVMTypeRef object_ptr;
  LLVMTypeRef msg_type;
  LLVMTypeRef msg_ptr;
  LLVMTypeRef actor_pad;
  LLVMTypeRef trace_type;
  LLVMTypeRef trace_fn;
  LLVMTypeRef dispatch_type;
  LLVMTypeRef dispatch_fn;
  LLVMTypeRef final_fn;

  LLVMValueRef personality;

  compile_frame_t* frame;
} compile_t;

bool codegen_init(pass_opt_t* opt);

void codegen_shutdown(pass_opt_t* opt);

bool codegen(ast_t* program, pass_opt_t* opt, pass_id pass_limit);

LLVMValueRef codegen_addfun(compile_t*c, const char* name, LLVMTypeRef type);

void codegen_startfun(compile_t* c, LLVMValueRef fun);

void codegen_pausefun(compile_t* c);

void codegen_finishfun(compile_t* c);

LLVMValueRef codegen_fun(compile_t* c);

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name);

PONY_EXTERN_C_END

#endif
