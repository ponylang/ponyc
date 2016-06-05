#include "reference.h"
#include "../ast/id.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static const char* suggest_alt_name(ast_t* ast, const char* name)
{
  assert(ast != NULL);
  assert(name != NULL);

  size_t name_len = strlen(name);

  if(is_name_private(name))
  {
    // Try without leading underscore
    const char* try_name = stringtab(name + 1);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }
  else
  {
    // Try with a leading underscore
    char* buf = (char*)ponyint_pool_alloc_size(name_len + 2);
    buf[0] = '_';
    strncpy(buf + 1, name, name_len + 1);
    const char* try_name = stringtab_consume(buf, name_len + 2);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Try with a different case (without crossing type/value boundary)
  ast_t* case_ast = ast_get_case(ast, name, NULL);
  if(case_ast != NULL)
  {
    ast_t* id = case_ast;

    if(ast_id(id) != TK_ID)
      id = ast_child(id);

    assert(ast_id(id) == TK_ID);
    const char* try_name = ast_name(id);

    if(ast_get(ast, try_name, NULL) != NULL)
      return try_name;
  }

  // Give up
  return NULL;
}

static bool resolve_reference(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  // Resolve the def associated with this reference - it must be in scope.
  sym_status_t status;
  const char* name = ast_name(ast_child(ast));
  ast_t* def = ast_get(ast, name, &status);

  if(def == NULL)
  {
    const char* alt_name = suggest_alt_name(ast, name);

    if(alt_name == NULL)
      ast_error(opt->check.errors, ast, "can't find declaration of '%s'", name);
    else
      ast_error(opt->check.errors, ast,
        "can't find declaration of '%s', did you mean '%s'?", name, alt_name);

    return false;
  }

  return true;
}

ast_result_t pass_reference(ast_t** astp, pass_opt_t* options)
{
  bool r = true;

  switch(ast_id(*astp))
  {
    case TK_REFERENCE: r = resolve_reference(options, astp); break;

    default: {}
  }

  if(!r)
  {
    assert(errors_get_count(options->check.errors) > 0);
    return AST_ERROR;
  }

  return AST_OK;
}
