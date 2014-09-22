#ifndef PASS_SUGAR_H
#define PASS_SUGAR_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

ast_result_t pass_sugar(ast_t** astp);

PONY_EXTERN_C_END

#endif
