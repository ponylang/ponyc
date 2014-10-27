#ifndef CODEGEN_GENBOX_H
#define CODEGEN_GENBOX_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

/**
 * If the value is not a pointer, box it as the specified type. Otherwise return
 * the value.
 */
LLVMValueRef gen_box(compile_t* c, ast_t* type, LLVMValueRef value);

/**
 * If the value is a pointer, unbox it as the specified type. Otherwise return
 * the value.
 */
LLVMValueRef gen_unbox(compile_t* c, ast_t* type, LLVMValueRef object);

PONY_EXTERN_C_END

#endif
