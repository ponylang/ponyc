#ifndef CODEGEN_GENEXPR_H
#define CODEGEN_GENEXPR_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_expr(compile_t* c, ast_t* ast);

/**
 * If the value is not a pointer, box it as the specified type. Otherwise return
 * the value.
 */
LLVMValueRef box_value(compile_t* c, LLVMValueRef value, ast_t* type);

/**
 * If the value is a pointer, unbox it as the specified type. Otherwise return
 * the value.
 */
LLVMValueRef unbox_value(compile_t* c, LLVMValueRef value, ast_t* type);

/**
 * Returns r_value cast to l_type.
 *
 * This will box primitives when necessary.
 */
LLVMValueRef gen_assign_cast(compile_t* c, LLVMTypeRef l_type,
  LLVMValueRef r_value, ast_t* type);

PONY_EXTERN_C_END

#endif
