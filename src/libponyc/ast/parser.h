#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "source.h"

ast_t* parser(source_t* source);

/// Report whether the specified token is an infix operator
bool is_expr_infix(token_id id);

#endif
