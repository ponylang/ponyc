#ifndef CODEGEN_H
#define CODEGEN_H

#include "../reach/reach.h"
#include "../pass/pass.h"
#include "../ast/ast.h"
#include "../ast/printbuf.h"
#include "../debug/dwarf.h"

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>

PONY_EXTERN_C_BEGIN

// Missing from C API.
char* LLVMGetHostCPUName();
void LLVMSetUnsafeAlgebra(LLVMValueRef inst);

// In case we need to change the internal calling convention.
#define GEN_CALLCONV LLVMFastCallConv

#define GEN_NOVALUE ((LLVMValueRef)1)

#define GEN_NOTNEEDED (LLVMConstInt(c->i1, 1, false))

typedef struct compile_local_t compile_local_t;
DECLARE_HASHMAP(compile_locals, compile_local_t);

typedef struct compile_frame_t
{
  LLVMValueRef fun;
  LLVMBasicBlockRef restore_builder;

  LLVMBasicBlockRef break_target;
  LLVMBasicBlockRef continue_target;
  LLVMBasicBlockRef invoke_target;

  compile_locals_t locals;

  struct compile_frame_t* prev;
} compile_frame_t;

typedef struct compile_t
{
  pass_opt_t* opt;

  LLVMPassManagerBuilderRef pmb;
  LLVMPassManagerRef mpm;
  LLVMPassManagerRef lpm;
  reachable_types_t* reachable;
  const char* filename;
  uint32_t next_type_id;

  const char* str_1;
  const char* str_Bool;
  const char* str_I8;
  const char* str_I16;
  const char* str_I32;
  const char* str_I64;
  const char* str_I128;
  const char* str_U8;
  const char* str_U16;
  const char* str_U32;
  const char* str_U64;
  const char* str_U128;
  const char* str_F32;
  const char* str_F64;
  const char* str_Pointer;
  const char* str_Array;
  const char* str_Platform;

  const char* str_add;
  const char* str_sub;
  const char* str_mul;
  const char* str_div;
  const char* str_mod;
  const char* str_neg;
  const char* str_and;
  const char* str_or;
  const char* str_xor;
  const char* str_not;
  const char* str_shl;
  const char* str_shr;
  const char* str_eq;
  const char* str_ne;
  const char* str_lt;
  const char* str_le;
  const char* str_ge;
  const char* str_gt;

  dwarf_t dwarf;

  LLVMContextRef context;
  LLVMTargetMachineRef machine;
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
  LLVMTypeRef field_descriptor;
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

bool codegen(ast_t* program, pass_opt_t* opt);

LLVMValueRef codegen_addfun(compile_t* c, const char* name, LLVMTypeRef type);

void codegen_startfun(compile_t* c, LLVMValueRef fun);

void codegen_pausefun(compile_t* c);

void codegen_finishfun(compile_t* c);

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target);

void codegen_poploop(compile_t* c);

void codegen_pushtry(compile_t* c, LLVMBasicBlockRef invoke_target);

void codegen_poptry(compile_t* c);

LLVMValueRef codegen_getlocal(compile_t* c, const char* name);

void codegen_setlocal(compile_t* c, const char* name, LLVMValueRef alloca);

LLVMValueRef codegen_fun(compile_t* c);

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name);

LLVMValueRef codegen_call(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  size_t count);

const char* suffix_filename(const char* dir, const char* file,
  const char* extension);

// Implemented in host.cc.
void stack_alloc(compile_t* c);

PONY_EXTERN_C_END

#endif
