#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include "codegen.h"

typedef struct gentype_t
{
  ast_t* ast;
  token_id underlying;
  size_t vtable_size;

  const char* type_name;
  LLVMTypeRef type;
  LLVMTypeRef primitive;

  size_t field_count;
  ast_t** fields;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;
} gentype_t;

LLVMTypeRef gentype_prelim(compile_t* c, ast_t* ast);

LLVMTypeRef gentype(compile_t* c, ast_t* ast);

#endif
