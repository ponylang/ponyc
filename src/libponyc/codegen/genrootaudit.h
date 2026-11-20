#ifndef CODEGEN_GENROOTAUDIT_H
#define CODEGEN_GENROOTAUDIT_H

#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_root_audit_body(compile_t* c, reach_type_t* t,
  reach_method_t* m, ast_t* body);

void gen_root_audit_site(compile_t* c, ast_t* sequence, ast_t* previous,
  ast_t* next);

PONY_EXTERN_C_END

#endif
