#ifndef CODEGEN_GENTAG_H
#define CODEGEN_GENTAG_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

// Pointer tagging for machine words smaller than 64 bits.
//
// Instead of heap-allocating a box (descriptor + value) for machine words in
// union types, encode the value directly in the pointer:
//
//   Bit 63:     1 = tagged machine word, 0 = real object pointer
//   Bits 62-32: type_id (31 bits)
//   Bits 31-0:  value (32 bits, enough for all taggable types)
//
#define PONY_TAG_BIT 63
#define PONY_TAG_TYPEID_SHIFT 32
#define PONY_TAG_VALUE_MASK 0xFFFFFFFFULL

// Returns true if the given reach_type_t is taggable (machine word <= 32 bits).
bool gentag_is_taggable(compile_t* c, reach_type_t* t);

// Generate code that checks whether a pointer value is a tagged machine word.
// Returns an i1.
LLVMValueRef gentag_is_tagged(compile_t* c, LLVMValueRef ptr);

// Generate code to create a tagged pointer from a machine word value.
// The value must be <= 32 bits. Returns a ptr.
LLVMValueRef gentag_box(compile_t* c, reach_type_t* t, LLVMValueRef value);

// Generate code to extract the raw value from a tagged pointer.
// Returns the value in mem_type for the given reach type.
LLVMValueRef gentag_unbox(compile_t* c, reach_type_t* t, LLVMValueRef tagged);

// Generate a guarded descriptor fetch: check for tagged pointer first,
// return the static descriptor if tagged, otherwise load from the object.
// This replaces direct calls to gendesc_fetch for values that might be tagged.
LLVMValueRef gentag_fetch_desc(compile_t* c, LLVMValueRef object);

// Create the __TagDescTable global: an array of descriptor pointers indexed
// by type_id >> 2, for looking up descriptors of tagged machine words.
LLVMValueRef gen_tag_desc_table(compile_t* c);

PONY_EXTERN_C_END

#endif
