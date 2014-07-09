#ifndef TYPE_CAP_H
#define TYPE_CAP_H

#include "../ast/ast.h"

token_id cap_upper_bounds(token_id a, token_id b);

token_id cap_lower_bounds(token_id a, token_id b);

bool is_cap_sub_cap(token_id sub, token_id super);

/**
 * The receiver capability is ref for constructors and behaviours. For
 * functions, it is defined by the function signature. A field initialiser is
 * considered part of a constructor.
 */
token_id cap_for_receiver(ast_t* ast);

token_id cap_for_fun(ast_t* fun);

token_id cap_for_type(ast_t* type);

#endif
