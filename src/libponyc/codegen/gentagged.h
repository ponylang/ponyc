#ifndef CODEGEN_GENTAGGED_H
#define CODEGEN_GENTAGGED_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

// On 64-bit platforms, machine words that fit in 32 bits or fewer are encoded
// directly in the pointer value instead of heap-allocated when they appear in
// union types.  Bit 63 distinguishes a tagged value from a real pointer
// (user-space pointers have bit 63 = 0 on all 64-bit targets Pony supports).
// Bits 48-62 carry the numeric type_id from the reach pass.  Bits 0-47 carry
// the zero-extended value payload.
//
// Layout of a tagged 64-bit value:
//
//   63  62        48  47                    0
//   [1] [  type_id  ] [      payload       ]
//
// Types whose primitive fits in 32 bits need no heap allocation, descriptor
// fetch, or GC tracing: Bool, U8, I8, U16, I16, U32, I32, F32.

/**
 * Return true if the given reach type can be tagged (its primitive
 * LLVM type fits in 32 bits).  Returns false on 32-bit platforms.
 */
bool gentagged_is_taggable(compile_t* c, reach_type_t* t);

/**
 * Return true if the given reach type (typically a union) contains at least
 * one taggable subtype.  Use this to gate the tagged/heap branch at call
 * sites.
 */
bool gentagged_has_taggable_subs(compile_t* c, reach_type_t* t);

/**
 * Return true if any reachable type in the program is taggable.
 */
bool gentagged_has_any_taggable(compile_t* c);

/**
 * Encode a primitive value and its type_id into a tagged pointer (i.e. an
 * LLVMValueRef of type ptr).  The caller must have verified
 * gentagged_is_taggable.
 */
LLVMValueRef gentagged_box(compile_t* c, reach_type_t* t, LLVMValueRef value);

/**
 * Build an LLVM i1 test: is this ptr value a tagged pointer?
 * (Test: bit 63 is set.)
 */
LLVMValueRef gentagged_is_tagged(compile_t* c, LLVMValueRef ptr_value);

/**
 * Extract the numeric type_id from a tagged pointer.  The caller must have
 * verified the value IS tagged (via gentagged_is_tagged).
 * Returns an i32.
 */
LLVMValueRef gentagged_typeid(compile_t* c, LLVMValueRef tagged);

/**
 * Extract the raw payload from a tagged pointer, zero-extended to the given
 * LLVM type.  The caller must have verified the value IS tagged and that
 * the type matches.
 */
LLVMValueRef gentagged_payload(compile_t* c, LLVMValueRef tagged,
  LLVMTypeRef target_type);

/**
 * Fetch the descriptor for a value that may be tagged or heap-allocated.
 * If `t` has taggable subtypes, emits a branch: tagged values get their
 * descriptor from the embedded type_id, heap values load it from the object
 * header.  Otherwise just loads the heap descriptor.  `prefix` names the
 * emitted basic blocks.
 */
LLVMValueRef gentagged_fetch_desc_or_heap(compile_t* c, LLVMValueRef value,
  reach_type_t* t, const char* prefix);

/**
 * Like gentagged_fetch_desc_or_heap but checks all reachable types instead
 * of a specific union's subtypes.
 */
LLVMValueRef gentagged_fetch_desc_or_heap_all(compile_t* c,
  LLVMValueRef value, const char* prefix);

PONY_EXTERN_C_END

#endif
