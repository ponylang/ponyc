#include "nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}

ast_t* nominal_builtin(ast_t* from, const char* name)
{
  return nominal_type(from, NULL, stringtab(name));
}

ast_t* nominal_type(ast_t* from, const char* package, const char* name)
{
  ast_t* ast = ast_from(from, TK_NOMINAL);
  ast_add(ast, ast_from(from, TK_NONE)); // ephemerality
  ast_add(ast, ast_from(from, TK_NONE)); // capability
  ast_add(ast, ast_from(from, TK_NONE)); // typeargs
  ast_add(ast, ast_from_string(from, name)); // name
  ast_add(ast, ast_from_string(from, package));

  return ast;
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
