#ifndef CODEGEN_H
#define CODEGEN_H

#include "gendebug.h"
#include "../reach/reach.h"
#include "../pass/pass.h"
#include "../ast/ast.h"
#include "../ast/printbuf.h"

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>

PONY_EXTERN_C_BEGIN

#define PONY_LLVM ((LLVM_VERSION_MAJOR * 100) + LLVM_VERSION_MINOR)

// Missing from C API.
char* LLVMGetHostCPUName();
void LLVMSetUnsafeAlgebra(LLVMValueRef inst);
void LLVMSetReturnNoAlias(LLVMValueRef fun);
void LLVMSetDereferenceable(LLVMValueRef fun, uint32_t i, size_t size);

#define GEN_NOVALUE ((LLVMValueRef)1)

#define GEN_NOTNEEDED (LLVMConstInt(c->i1, 1, false))

typedef struct compile_local_t compile_local_t;
DECLARE_HASHMAP(compile_locals, compile_locals_t, compile_local_t);

typedef struct compile_frame_t
{
  LLVMValueRef fun;
  LLVMValueRef ctx;

  LLVMBasicBlockRef break_target;
  LLVMBasicBlockRef continue_target;
  LLVMBasicBlockRef invoke_target;

  compile_locals_t locals;
  LLVMMetadataRef di_file;
  LLVMMetadataRef di_scope;
  bool is_function;

  struct compile_frame_t* prev;
} compile_frame_t;

typedef struct compile_t
{
  pass_opt_t* opt;
  reach_t* reach;
  const char* filename;

  const char* str_builtin;
  const char* str_Bool;
  const char* str_I8;
  const char* str_I16;
  const char* str_I32;
  const char* str_I64;
  const char* str_I128;
  const char* str_ILong;
  const char* str_ISize;
  const char* str_U8;
  const char* str_U16;
  const char* str_U32;
  const char* str_U64;
  const char* str_U128;
  const char* str_ULong;
  const char* str_USize;
  const char* str_F32;
  const char* str_F64;
  const char* str_Pointer;
  const char* str_Maybe;
  const char* str_Array;
  const char* str_String;
  const char* str_Platform;
  const char* str_Main;
  const char* str_Env;

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

  const char* str_this;
  const char* str_create;
  const char* str__create;
  const char* str__init;
  const char* str__final;
  const char* str__event_notify;

  LLVMCallConv callconv;
  LLVMContextRef context;
  LLVMTargetMachineRef machine;
  LLVMTargetDataRef target_data;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMDIBuilderRef di;
  LLVMMetadataRef di_unit;

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

void codegen_startfun(compile_t* c, LLVMValueRef fun, LLVMMetadataRef file,
  LLVMMetadataRef scope);

void codegen_finishfun(compile_t* c);

void codegen_pushscope(compile_t* c, ast_t* ast);

void codegen_popscope(compile_t* c);

LLVMMetadataRef codegen_difile(compile_t* c);

LLVMMetadataRef codegen_discope(compile_t* c);

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target);

void codegen_poploop(compile_t* c);

void codegen_pushtry(compile_t* c, LLVMBasicBlockRef invoke_target);

void codegen_poptry(compile_t* c);

void codegen_debugloc(compile_t* c, ast_t* ast);

LLVMValueRef codegen_getlocal(compile_t* c, const char* name);

void codegen_setlocal(compile_t* c, const char* name, LLVMValueRef alloca);

LLVMValueRef codegen_ctx(compile_t* c);

void codegen_setctx(compile_t* c, LLVMValueRef ctx);

LLVMValueRef codegen_fun(compile_t* c);

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name);

LLVMValueRef codegen_call(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  size_t count);

const char* suffix_filename(compile_t* c, const char* dir, const char* prefix,
  const char* file, const char* extension);

PONY_EXTERN_C_END

#endif
