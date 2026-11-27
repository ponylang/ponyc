#ifndef CODEGEN_GENDESC_H
#define CODEGEN_GENDESC_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

// The index of each field of a type descriptor, in the order of
// _pony_type_t in pony.h. A mismatch makes the runtime read descriptors at
// the wrong offsets.
#define DESC_ID 0
#define DESC_SIZE 1
#define DESC_FIELD_COUNT 2
#define DESC_FIELD_OFFSET 3
#define DESC_INSTANCE 4
#define DESC_NAME 5
#define DESC_GET_BEHAVIOR_NAME 6
#define DESC_TRACE 7
#define DESC_DISPATCH 8
#define DESC_FINALISE 9
#define DESC_EVENT_NOTIFY 10
#define DESC_MIGHT_REFERENCE_ACTOR 11
#define DESC_EMBED_FLAGS 12
#define DESC_TRAITS 13
#define DESC_FIELDS 14
#define DESC_VTABLE 15

#define DESC_LENGTH 16

void gendesc_basetype(compile_t* c, LLVMTypeRef desc_type);

void gendesc_type(compile_t* c, reach_type_t* t);

void gendesc_init(compile_t* c, reach_type_t* t);

LLVMValueRef gendesc_fetch(compile_t* c, LLVMValueRef object);

LLVMValueRef gendesc_typeid(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_instance(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_trace(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_dispatch(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_vtable(compile_t* c, LLVMValueRef desc, size_t colour);

LLVMValueRef gendesc_ptr_to_fields(compile_t* c, LLVMValueRef object,
  LLVMValueRef desc);

LLVMValueRef gendesc_fieldcount(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_fieldinfo(compile_t* c, LLVMValueRef desc, size_t index);

LLVMValueRef gendesc_fieldptr(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef field_info);

LLVMValueRef gendesc_fieldload(compile_t* c, LLVMValueRef ptr,
  LLVMValueRef field_info);

LLVMValueRef gendesc_fielddesc(compile_t* c, LLVMValueRef field_info);

/**
 * Return the DESC_SIZE field from the descriptor (the ABI size of the boxed
 * object, as an i32).
 */
LLVMValueRef gendesc_size(compile_t* c, LLVMValueRef desc);

/**
 * Return the DESC_FIELD_OFFSET field from the descriptor (the byte offset
 * from the start of the object to the first data field, as an i32).  For
 * boxable numeric types this is the offset to the primitive value; for tuples
 * it is the offset to the first tuple element.
 */
LLVMValueRef gendesc_fieldoffset(compile_t* c, LLVMValueRef desc);

LLVMValueRef gendesc_isnominal(compile_t* c, LLVMValueRef desc, ast_t* trait);

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef desc, ast_t* type);

LLVMValueRef gendesc_isentity(compile_t* c, LLVMValueRef desc, ast_t* type);

PONY_EXTERN_C_END

#endif
