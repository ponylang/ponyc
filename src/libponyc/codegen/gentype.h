#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

void get_fieldinfo(ast_t* l_type, ast_t* right, ast_t** l_def,
  ast_t** field, uint32_t* index);

typedef struct compile_type_t
{
  compile_opaque_free_fn free_fn;

  size_t abi_size;

  LLVMTypeRef structure;
  LLVMTypeRef structure_ptr;
  LLVMTypeRef primitive;
  LLVMTypeRef use_type;
  LLVMTypeRef mem_type;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;
  LLVMValueRef trace_fn;
  LLVMValueRef serialise_trace_fn;
  LLVMValueRef serialise_fn;
  LLVMValueRef deserialise_fn;
  LLVMValueRef custom_serialise_space_fn;
  LLVMValueRef custom_serialise_fn;
  LLVMValueRef custom_deserialise_fn;
  LLVMValueRef final_fn;
  LLVMValueRef dispatch_fn;
  LLVMValueRef dispatch_switch;

  LLVMMetadataRef di_file;
  LLVMMetadataRef di_type;
  LLVMMetadataRef di_type_embed;
} compile_type_t;

bool gentypes(compile_t* c);

PONY_EXTERN_C_END

#endif
