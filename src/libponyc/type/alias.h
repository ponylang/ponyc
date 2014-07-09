#ifndef TYPE_ALIAS_H
#define TYPE_ALIAS_H

#include "../ast/ast.h"

ast_t* alias(ast_t* type);

ast_t* consume_type(ast_t* type);

ast_t* recover_type(ast_t* type);

#endif
