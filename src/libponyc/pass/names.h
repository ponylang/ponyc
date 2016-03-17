#ifndef PASS_NAMES_H
#define PASS_NAMES_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_t* names_def(ast_t* ast);

bool is_name_private(const char* name);

bool is_name_type(const char* name);

bool is_name_ffi(const char* name);

bool is_name_internal_test(const char* name);

bool names_nominal(pass_opt_t* opt, ast_t* scope, ast_t** astp, bool expr);

ast_result_t pass_names(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
