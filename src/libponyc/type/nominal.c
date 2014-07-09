#include "nominal.h"
#include "typealias.h"
#include "subtype.h"
#include "reify.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool is_typeparam(ast_t* scope, ast_t* typeparam, ast_t* typearg)
{
  if(ast_id(typearg) != TK_NOMINAL)
    return false;

  ast_t* def = nominal_def(scope, typearg);
  return def == typeparam;
}

static bool check_constraints(ast_t* scope, ast_t* def, ast_t* typeargs)
{
  if(ast_id(def) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }

    return true;
  }

  // reify the type parameters with the typeargs
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* r_typeparams = reify(typeparams, typeparams, typeargs);

  if(r_typeparams == NULL)
    return false;

  ast_t* r_typeparam = ast_child(r_typeparams);
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((r_typeparam != NULL) && (typearg != NULL))
  {
    ast_t* constraint = ast_childidx(r_typeparam, 1);

    // compare the typearg to the typeparam and constraint
    if(!is_typeparam(scope, typeparam, typearg) &&
      !is_subtype(scope, typearg, constraint)
      )
    {
      ast_error(typearg, "type argument is outside its constraint");
      ast_error(typeparam, "constraint is here");
      ast_free_unattached(r_typeparams);
      return false;
    }

    r_typeparam = ast_sibling(r_typeparam);
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  ast_free_unattached(r_typeparams);
  return true;
}

static ast_t* nominal_with_args(ast_t* from, const char* package,
  const char* name, ast_t* typeargs)
{
  ast_t* ast = ast_from(from, TK_NOMINAL);
  ast_add(ast, ast_from(from, TK_NONE)); // ephemerality
  ast_add(ast, ast_from(from, TK_NONE)); // capability
  ast_add(ast, typeargs);
  ast_add(ast, ast_from_string(from, name)); // name
  ast_add(ast, ast_from_string(from, package));

  return ast;
}

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

ast_t* nominal_builtin1(ast_t* from, const char* name, ast_t* typearg0)
{
  return nominal_type1(from, NULL, stringtab(name), typearg0);
}

ast_t* nominal_type(ast_t* from, const char* package, const char* name)
{
  return nominal_type1(from, package, name, NULL);
}

ast_t* nominal_type1(ast_t* from, const char* package, const char* name,
  ast_t* typearg0)
{
  ast_t* typeargs;

  if(typearg0 != NULL)
  {
    typeargs = ast_from(from, TK_TYPEARGS);
    ast_add(typeargs, typearg0);
  } else {
    typeargs = ast_from(from, TK_NONE);
  }

  ast_t* ast = nominal_with_args(from, package, name, typeargs);

  if(!nominal_valid(from, &ast))
  {
    ast_error(from, "unable to validate %s.%s", package, name);
    ast_free(ast);
    return NULL;
  }

  return ast;
}

ast_t* nominal_sugar(ast_t* from, const char* package, const char* name)
{
  return nominal_with_args(from, package, name, ast_from(from, TK_NONE));
}

bool nominal_valid(ast_t* scope, ast_t** ast)
{
  ast_t* nominal = *ast;
  ast_t* def = nominal_def(scope, nominal);

  // resolve type alias if it is one
  if(ast_id(def) == TK_TYPE)
    return typealias_nominal(scope, ast);

  // make sure our typeargs are subtypes of our constraints
  ast_t* typeargs = ast_childidx(nominal, 2);

  if(!check_constraints(nominal, def, typeargs))
    return false;

  // use our default capability if we need to
  ast_t* cap = ast_sibling(typeargs);

  if(ast_id(cap) == TK_NONE)
  {
    switch(ast_id(def))
    {
      case TK_TYPEPARAM:
      {
        ast_t* constraint = ast_childidx(def, 1);
        ast_t* defcap;

        switch(ast_id(constraint))
        {
          case TK_NOMINAL:
            defcap = ast_childidx(constraint, 3);
            break;

          case TK_STRUCTURAL:
            defcap = ast_childidx(constraint, 1);
            break;

          default:
            defcap = ast_from(constraint, TK_TAG);
            break;
        }

        ast_replace(&cap, defcap);
        return true;
      }

      case TK_TRAIT:
      case TK_CLASS:
      case TK_ACTOR:
      {
        ast_t* defcap;

        if(ast_nearest(nominal, TK_TYPEPARAM) != NULL)
          defcap = ast_from(cap, TK_TAG);
        else
          defcap = ast_childidx(def, 2);

        ast_replace(&cap, defcap);
        break;
      }

      default:
        assert(0);
        return false;
    }
  }

  return true;
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

  ast_setdata(nominal, def);
  return def;
}
