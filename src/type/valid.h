#ifndef TYPE_VALID_H
#define TYPE_VALID_H

#include "../ast/ast.h"

ast_result_t type_valid(ast_t* ast, int verbose);

bool valid_nominal(ast_t* scope, ast_t* nominal);

#endif
