#ifndef PASS_SUGAR_H
#define PASS_SUGAR_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

// Report whether the given member list contains a member with the given name.
bool has_member(ast_t* members, const char* name);

// Apply default receiver capability and return value, if needed, for the given
// function.
void fun_defaults(ast_t* ast);

// Create expanded location information about the given node.
// The resulting tree must be caught up with passes and freed by the caller.
ast_t* expand_location(ast_t* location);

ast_result_t pass_sugar(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
