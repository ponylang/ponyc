#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include "codegen.h"

typedef struct gentype_t
{
  ast_t* ast;
  token_id underlying;
  size_t vtable_size;

  const char* type_name;
  LLVMTypeRef structure;
  LLVMTypeRef structure_ptr;
  LLVMTypeRef primitive;
  LLVMTypeRef use_type;

  size_t field_count;
  ast_t** fields;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;
} gentype_t;

bool gentype_prelim(compile_t* c, ast_t* ast, gentype_t* g);

bool gentype(compile_t* c, ast_t* ast, gentype_t* g);

#endif
