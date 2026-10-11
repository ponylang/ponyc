#ifndef SUBTYPE_FFI_H
#define SUBTYPE_FFI_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool is_subtype_for_defs(ast_t* sub_def, token_id sub_cap,
  ast_t* super_def, token_id super_cap, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
