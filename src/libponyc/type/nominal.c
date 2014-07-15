#include "nominal.h"
#include "typealias.h"
#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "../ast/token.h"
#include "../ds/stringtab.h"
#include <assert.h>

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

  if(def == NULL)
    return false;

  // resolve type alias if it is one
  if(ast_id(def) == TK_TYPE)
    return typealias_nominal(scope, ast);

  // make sure our typeargs are subtypes of our constraints
  ast_t* typeargs = ast_childidx(nominal, 2);

  if(ast_id(def) == TK_TYPEPARAM)
  {
    if(ast_id(typeargs) != TK_NONE)
    {
      ast_error(typeargs, "type parameters cannot have type arguments");
      return false;
    }
  } else {
    ast_t* typeparams = ast_childidx(def, 1);

    if(!check_constraints(scope, typeparams, typeargs))
      return false;
  }

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

void nominal_applycap(ast_t* from, ast_t** to)
{
  assert(ast_id(from) == TK_NOMINAL);
  token_id cap = ast_id(ast_childidx(from, 3));
  token_id ephemeral = ast_id(ast_childidx(from, 4));

  if((cap == TK_NONE) && (ephemeral == TK_NONE))
    return;

  ast_t* ast = *to;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      ast = ast_dup(ast);
      *to = ast;

      if(cap != TK_NONE)
      {
        ast_t* tcap = ast_childidx(ast, 3);
        token_id fcap = ast_id(tcap);

        if((fcap == TK_NONE) || is_cap_sub_cap(cap, fcap))
          ast_setid(tcap, cap);
      }

      if(ephemeral != TK_NONE)
      {
        ast_t* eph = ast_childidx(ast, 4);
        ast_setid(eph, ephemeral);
      }

      break;
    }

    case TK_STRUCTURAL:
    {
      ast = ast_dup(ast);
      *to = ast;

      if(cap != TK_NONE)
      {
        ast_t* tcap = ast_childidx(ast, 1);
        token_id fcap = ast_id(tcap);

        if((fcap == TK_NONE) || is_cap_sub_cap(cap, fcap))
          ast_setid(tcap, cap);
      }

      if(ephemeral != TK_NONE)
      {
        ast_t* eph = ast_childidx(ast, 2);
        ast_setid(eph, ephemeral);
      }

      break;
    }

    default: {}
  }
}
