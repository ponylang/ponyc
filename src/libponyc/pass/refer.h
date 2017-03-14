#ifndef PASS_REFER_H
#define PASS_REFER_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool refer_reference(pass_opt_t* opt, ast_t** astp);
bool refer_dot(pass_opt_t* opt, ast_t* ast);
bool refer_qualify(pass_opt_t* opt, ast_t* ast);
ast_result_t pass_refer(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
