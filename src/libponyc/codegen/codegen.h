#ifndef CODEGEN_H
#define CODEGEN_H

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Analysis.h>
#include <stdio.h>

#include "gendebug.h"
#include "../reach/reach.h"
#include "../pass/pass.h"
#include "../ast/ast.h"
#include "../ast/printbuf.h"
#include "../pass/dartsrc.h"

PONY_EXTERN_C_BEGIN

// Missing from C API.

void LLVMSetUnsafeAlgebra(LLVMValueRef inst);
void LLVMSetNoUnsignedWrap(LLVMValueRef inst);
void LLVMSetNoSignedWrap(LLVMValueRef inst);
void LLVMSetIsExact(LLVMValueRef inst);
LLVMValueRef LLVMConstNaN(LLVMTypeRef type);
LLVMValueRef LLVMConstInf(LLVMTypeRef type, bool negative);
LLVMModuleRef LLVMParseIRFileInContext(LLVMContextRef ctx, const char* file);
bool LLVMHasMetadataStr(LLVMValueRef val, const char* str);
void LLVMSetMetadataStr(LLVMValueRef val, const char* str, LLVMValueRef node);
void LLVMMDNodeReplaceOperand(LLVMValueRef parent, unsigned i,
  LLVMValueRef node);

// Intrinsics.
LLVMValueRef LLVMMemcpy(LLVMModuleRef module, bool ilp32);
LLVMValueRef LLVMMemmove(LLVMModuleRef module, bool ilp32);
LLVMValueRef LLVMLifetimeStart(LLVMModuleRef module, LLVMTypeRef type);
LLVMValueRef LLVMLifetimeEnd(LLVMModuleRef module, LLVMTypeRef type);

// Instructions
LLVMValueRef LLVMBuildAlignedLoad(LLVMBuilderRef b, LLVMTypeRef t, LLVMValueRef p, uint64_t align, const char* name);
LLVMValueRef LLVMBuildAlignedStore(LLVMBuilderRef b, LLVMValueRef v, LLVMValueRef p, uint64_t align);

// The LLVM C API has no defines for memory effects attributes
// Corresponds to MemoryEffects template class in
// llvm/include/llvm/Support/ModRef.h
#define LLVM_MEMORYEFFECTS_NONE 0
#define LLVM_MEMORYEFFECTS_READ 1
#define LLVM_MEMORYEFFECTS_WRITE 2
#define LLVM_MEMORYEFFECTS_READWRITE 3

#define LLVM_MEMORYEFFECTS_ARG(x) x
#define LLVM_MEMORYEFFECTS_INACCESSIBLEMEM(x) (x << 2)
#define LLVM_MEMORYEFFECTS_OTHER(x) (x << 4)

#define LLVM_DECLARE_ATTRIBUTEREF(decl, name, val) \
  LLVMAttributeRef decl; \
  { \
    unsigned decl##_id = \
      LLVMGetEnumAttributeKindForName(#name, sizeof(#name) - 1); \
    pony_assert(decl##_id != 0);\
    decl = LLVMCreateEnumAttribute(c->context, decl##_id, val); \
  }

#define GEN_NOVALUE ((LLVMValueRef)1)

#define GEN_NOTNEEDED ((LLVMValueRef)2)

typedef struct genned_string_t genned_string_t;
DECLARE_HASHMAP(genned_strings, genned_strings_t, genned_string_t);

typedef struct compile_local_t compile_local_t;
DECLARE_HASHMAP(compile_locals, compile_locals_t, compile_local_t);

typedef struct ffi_decl_t ffi_decl_t;
DECLARE_HASHMAP(ffi_decls, ffi_decls_t, ffi_decl_t);

typedef struct compile_frame_t
{
  LLVMValueRef fun;
  LLVMValueRef ctx;

  LLVMBasicBlockRef break_target;
  LLVMBasicBlockRef break_novalue_target;
  LLVMBasicBlockRef continue_target;
  LLVMBasicBlockRef invoke_target;

  compile_locals_t locals;
  LLVMMetadataRef di_file;
  LLVMMetadataRef di_scope;
  bool is_function;
  bool early_termination;
  bool bare_function;
  deferred_reification_t* reify;

  struct compile_frame_t* prev;
} compile_frame_t;

typedef struct compile_t
{
  pass_opt_t* opt;
  reach_t* reach;
  genned_strings_t strings;
  ffi_decls_t ffi_decls;
  const char* filename;

  DartModule moduleDart;
  
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
  const char* str_NullablePointer;
  const char* str_DoNotOptimise;
  const char* str_Array;
  const char* str_String;
  const char* str_Platform;
  const char* str_Main;
  const char* str_Env;

  const char* str_add;
  const char* str_sub;
  const char* str_mul;
  const char* str_div;
  const char* str_rem;
  const char* str_neg;
  const char* str_add_unsafe;
  const char* str_sub_unsafe;
  const char* str_mul_unsafe;
  const char* str_div_unsafe;
  const char* str_rem_unsafe;
  const char* str_neg_unsafe;
  const char* str_and;
  const char* str_or;
  const char* str_xor;
  const char* str_not;
  const char* str_shl;
  const char* str_shr;
  const char* str_shl_unsafe;
  const char* str_shr_unsafe;
  const char* str_eq;
  const char* str_ne;
  const char* str_lt;
  const char* str_le;
  const char* str_ge;
  const char* str_gt;
  const char* str_eq_unsafe;
  const char* str_ne_unsafe;
  const char* str_lt_unsafe;
  const char* str_le_unsafe;
  const char* str_ge_unsafe;
  const char* str_gt_unsafe;

  const char* str_this;
  const char* str_create;
  const char* str__create;
  const char* str__init;
  const char* str__final;
  const char* str__event_notify;
  const char* str__serialise_space;
  const char* str__serialise;
  const char* str__deserialise;

  uint32_t trait_bitmap_size;

  LLVMCallConv callconv;
  LLVMLinkage linkage;
  LLVMContextRef context;
  LLVMTargetMachineRef machine;
  LLVMTargetDataRef target_data;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMDIBuilderRef di;
  LLVMMetadataRef di_unit;
  LLVMValueRef none_instance;
  LLVMValueRef primitives_init;
  LLVMValueRef primitives_final;
  LLVMValueRef desc_table;
  LLVMValueRef desc_table_offset_lookup_fn;
  LLVMValueRef numeric_sizes;

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

  LLVMTypeRef ptr;
  LLVMTypeRef descriptor_type;
  LLVMTypeRef descriptor_offset_lookup_type;
  LLVMTypeRef descriptor_offset_lookup_fn;
  LLVMTypeRef field_descriptor;
  LLVMTypeRef object_type;
  LLVMTypeRef msg_type;
  LLVMTypeRef actor_pad;
  LLVMTypeRef trace_fn;
  LLVMTypeRef serialise_fn;
  LLVMTypeRef dispatch_fn;
#if defined(USE_RUNTIME_TRACING)
  LLVMTypeRef get_behavior_name_fn;
#endif
  LLVMTypeRef final_fn;
  LLVMTypeRef custom_serialise_space_fn;
  LLVMTypeRef custom_deserialise_fn;

  LLVMValueRef personality;

  compile_frame_t* frame;
} compile_t;

bool codegen_merge_runtime_bitcode(compile_t* c);

bool codegen_llvm_init();

void codegen_llvm_shutdown();

bool codegen_pass_init(pass_opt_t* opt);

void codegen_pass_cleanup(pass_opt_t* opt);

LLVMTargetMachineRef codegen_machine(LLVMTargetRef target, pass_opt_t* opt);

bool codegen(ast_t* program, pass_opt_t* opt);

bool codegen_gen_test(compile_t* c, ast_t* program, pass_opt_t* opt,
  pass_id last_pass);

void codegen_cleanup(compile_t* c);

LLVMValueRef codegen_addfun(compile_t* c, const char* name, LLVMTypeRef type,
  bool pony_abi);

void codegen_startfun(compile_t* c, LLVMValueRef fun, LLVMMetadataRef file,
  LLVMMetadataRef scope, deferred_reification_t* reify, bool bare);

void codegen_finishfun(compile_t* c);

void codegen_pushscope(compile_t* c, ast_t* ast);

void codegen_popscope(compile_t* c);

void codegen_local_lifetime_start(compile_t* c, const char* name);

void codegen_local_lifetime_end(compile_t* c, const char* name);

void codegen_scope_lifetime_end(compile_t* c);

LLVMMetadataRef codegen_difile(compile_t* c);

LLVMMetadataRef codegen_discope(compile_t* c);

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target, LLVMBasicBlockRef break_novalue_target);

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

LLVMValueRef codegen_call(compile_t* c, LLVMTypeRef fun_type, LLVMValueRef fun,
  LLVMValueRef* args, size_t count, bool setcc);

LLVMValueRef codegen_string(compile_t* c, const char* str, size_t len);

const char* suffix_filename(compile_t* c, const char* dir, const char* prefix,
  const char* file, const char* extension);

PONY_EXTERN_C_END

#endif
