#ifndef PASS_PARSEFIX_H
#define PASS_PARSEFIX_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_parse_fix(ast_t** astp, pass_opt_t* options);

/// Allow all source code we normally explicitly outlaw, just fixup the tree.
/// Intended for sugar testing, etc.
void parse_fix_allow_all(bool allow);

PONY_EXTERN_C_END

#endif
