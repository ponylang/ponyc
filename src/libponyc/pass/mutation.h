#ifndef PASS_MUTATION_H
#define PASS_MUTATION_H

#include "pass.h"

PONY_EXTERN_C_BEGIN

typedef struct mutation_type_policy_t
{
  ast_t* limit;
  const char* type_name;
  uint16_t categories;
} mutation_type_policy_t;

uint16_t mutation_category_bit(const char* name, size_t len);
bool mutation_check_selector(pass_opt_t* opt, ast_t* annotation);
void mutation_inherit_body_selector(ast_t* method, ast_t* adopted_body);
mutation_type_policy_t mutation_type_policy(ast_t* entity);
bool mutation_resolve_method(pass_opt_t* opt,
  const mutation_type_policy_t* policy, ast_t* method);

PONY_EXTERN_C_END

#endif
