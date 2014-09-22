#ifndef PASS_PARSEFIX_H
#define PASS_PARSEFIX_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_parse_fix(ast_t** astp);

PONY_EXTERN_C_END

#endif
