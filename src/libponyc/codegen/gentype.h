#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

typedef struct gentype_t
{
  ast_t* ast;
  token_id underlying;

  const char* type_name;
  const char* desc_name;

  LLVMTypeRef structure;
  LLVMTypeRef structure_ptr;
  LLVMTypeRef primitive;
  LLVMTypeRef use_type;

  int field_count;
  ast_t** fields;
  token_id* field_keys;

  LLVMTypeRef desc_type;
  LLVMValueRef desc;
  LLVMValueRef instance;

  LLVMValueRef dispatch_fn;
  LLVMValueRef dispatch_switch;

  bool done;
} gentype_t;

bool gentype_prelim(compile_t* c, ast_t* ast, gentype_t* g);

bool gentype(compile_t* c, ast_t* ast, gentype_t* g);

PONY_EXTERN_C_END

#endif
