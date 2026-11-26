#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

typedef struct compile_type_t
{
  compile_opaque_free_fn free_fn;

  size_t abi_size;
  size_t module_index;

  LLVMTypeRef structure;
  LLVMTypeRef structure_ptr;
  LLVMTypeRef primitive;
  LLVMTypeRef use_type;
  LLVMTypeRef mem_type;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;
  LLVMValueRef trace_fn;
  LLVMValueRef final_fn;
  LLVMValueRef dispatch_fn;
  LLVMValueRef dispatch_switch;
  LLVMValueRef get_behavior_name_fn;
  LLVMValueRef get_behavior_name_switch;

  LLVMMetadataRef di_file;
  LLVMMetadataRef di_type;
  LLVMMetadataRef di_type_embed;

  // PONY_EMBED_* flags, set before any trace is generated.
  uint8_t embed_flags;
} compile_type_t;

bool gentypes(compile_t* c);

// The address of an embedded field of t in object. A field with no size has
// the address of the object that holds it.
LLVMValueRef gentype_embed_ptr(compile_t* c, reach_type_t* t,
  LLVMValueRef object, uint32_t index);

// For a struct that starts with an embedded field with a non-zero size, that
// field's type: the object that shares the struct's address. NULL otherwise.
reach_type_t* gentype_first_embed(compile_t* c, reach_type_t* t);

PONY_EXTERN_C_END

#endif
