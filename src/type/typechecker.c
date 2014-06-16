#include "typechecker.h"
#include "scope.h"
#include "valid.h"
#include "traits.h"
#include "sugar.h"
#include "expr.h"
#include "args.h"
#include <assert.h>

bool typecheck(ast_t* ast, int verbose)
{
  if(ast_visit(ast, type_scope, NULL, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, NULL, type_valid, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, type_traits, NULL, verbose) != AST_OK)
    return false;

  // recalculate scopes in the presence of flattened traits
  ast_clear(ast);

  if(ast_visit(ast, type_scope, NULL, verbose) != AST_OK)
    return false;

  if(ast_visit(ast, type_sugar, type_expr, verbose) != AST_OK)
    return false;

  // if(ast_visit(ast, type_args, NULL, verbose) != AST_OK)
  //   return false;

  return true;
}

bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}

ast_t* nominal_def(ast_t* scope, ast_t* nominal)
{
  assert(ast_id(nominal) == TK_NOMINAL);
  ast_t* def = ast_data(nominal);

  if(def != NULL)
    return def;

  ast_t* package = ast_child(nominal);
  ast_t* type = ast_sibling(package);

  if(ast_id(package) != TK_NONE)
  {
    const char* name = ast_name(package);
    ast_t* r_package = ast_get(scope, name);

    if((r_package == NULL) || (ast_id(r_package) != TK_PACKAGE))
    {
      ast_error(package, "can't find package '%s'", name);
      return NULL;
    }

    scope = r_package;
  }

  const char* name = ast_name(type);
  def = ast_get(scope, name);

  if((def == NULL) ||
    ((ast_id(def) != TK_TYPE) &&
     (ast_id(def) != TK_TRAIT) &&
     (ast_id(def) != TK_CLASS) &&
     (ast_id(def) != TK_ACTOR) &&
     (ast_id(def) != TK_TYPEPARAM))
    )
  {
    ast_error(type, "can't find definition of '%s'", name);
    return NULL;
  }

  ast_attach(nominal, def);
  return def;
}
