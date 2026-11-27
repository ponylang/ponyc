#ifndef EXPR_LAMBDA_H
#define EXPR_LAMBDA_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

bool expr_lambda(pass_opt_t* opt, ast_t** astp);

bool expr_object(pass_opt_t* opt, ast_t** astp);

/** Bring a provided trait or interface (or each in an intersection or an
 * alias of one) through its own check, unless that check is running; while a
 * default argument is being checked, only to the refer pass.
 */
bool catch_up_provided(pass_opt_t* opt, ast_t* type);

/** Finish the anonymous classes that waited for trait checks, once no trait
 * check is running.
 */
bool drain_deferred_classes(pass_opt_t* opt, ast_t* ast);

PONY_EXTERN_C_END

#endif
