#ifndef CODEGEN_GENDESC_H
#define CODEGEN_GENDESC_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

void gendesc_basetype(compile_t* c, LLVMTypeRef desc_type);

void gendesc_type(compile_t* c, reach_type_t* t);

void gendesc_init(compile_t* c, reach_type_t* t);

void gendesc_table(compile_t* c);

void gendesc_table_lookup(compile_t* c);

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

LLVMValueRef gendesc_isnominal(compile_t* c, LLVMValueRef desc, ast_t* trait);

LLVMValueRef gendesc_istrait(compile_t* c, LLVMValueRef desc, ast_t* type);

LLVMValueRef gendesc_isentity(compile_t* c, LLVMValueRef desc, ast_t* type);

PONY_EXTERN_C_END

#endif
