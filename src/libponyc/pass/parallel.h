#ifndef PASS_PARALLEL_H
#define PASS_PARALLEL_H

#include <platform.h>
#include "../ast/ast.h"
#include "pass.h"

PONY_EXTERN_C_BEGIN

typedef enum
{
  PARALLEL_OK,
  PARALLEL_ERROR,
  PARALLEL_FATAL,
  PARALLEL_FALLBACK
} parallel_result_t;

parallel_result_t parallel_expr(ast_t** astp, pass_opt_t* options);

PONY_EXTERN_C_END

#endif
