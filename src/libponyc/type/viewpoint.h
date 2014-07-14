#ifndef TYPE_VIEWPOINT_H
#define TYPE_VIEWPOINT_H

#include "../ast/ast.h"

ast_t* viewpoint(ast_t* left, ast_t* right);

ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type);

void flatten_thistype(ast_t** astp, ast_t* type);

bool safe_to_write(ast_t* ast, ast_t* type);

#endif
