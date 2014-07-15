#ifndef PASS_NAMES_H
#define PASS_NAMES_H

#include "../ast/ast.h"

bool names_nominal(ast_t* scope, ast_t** astp);

ast_result_t pass_names(ast_t** astp);

#endif
