#include "import.h"
#include "../ast/ast.h"
#include <assert.h>


static bool import_use(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* import = (ast_t*)ast_data(ast);

  if(import == NULL) // Nothing to import
    return true;

  ast_t* module = ast_parent(ast);
  assert(ast_id(module) == TK_MODULE);

  ast_t* package = ast_parent(module);
  assert(ast_id(package) == TK_PACKAGE);

  if(!ast_canmerge(package, import) || !ast_merge(module, import))
  {
    ast_t* uri_child = ast_childidx(ast, 1);
    assert(uri_child != NULL);
    const char* uri = ast_name(uri_child);
    assert(uri != NULL);

    ast_error(ast, "can't merge symbols from '%s'", uri);
    return false;
  }

  return true;
}


ast_result_t pass_import(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PROGRAM:
    case TK_PACKAGE:
    case TK_MODULE:
      return AST_OK;

    case TK_USE:
      if(!import_use(ast))
        return AST_FATAL;
      break;

    default:
      return AST_IGNORE;
  }

  return AST_OK;
}
