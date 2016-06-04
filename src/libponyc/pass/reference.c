#include "reference.h"
#include <assert.h>

ast_result_t pass_reference(ast_t** astp, pass_opt_t* options)
{
  bool r = true;

  switch(ast_id(*astp))
  {
    default: {}
  }

  if(!r)
  {
    assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
