#ifndef TYPE_CAP_H
#define TYPE_CAP_H

#include "../ast/ast.h"

bool is_cap_sub_cap(token_id sub, token_id super);

token_id cap_for_receiver(ast_t* ast);

token_id cap_for_fun(ast_t* fun);

ast_t* cap_from_rawcap(ast_t* ast, token_id rawcap);

#endif
