#ifndef PASS_NAMES_H
#define PASS_NAMES_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_t* names_def(pass_opt_t* opt, ast_t* ast);

bool names_nominal(pass_opt_t* opt, ast_t* scope, ast_t** astp, bool expr);

/** Resolve a nominal type to a definition already found for it, as
 * names_nominal does once its lookup has found `def`.
 */
bool names_nominal_def(pass_opt_t* opt, ast_t** astp, ast_t* def, bool expr);

ast_result_t pass_names(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
