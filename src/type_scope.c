#include "type_scope.h"
#include "package.h"
#include "typechecker.h"
#include <assert.h>

/**
 * Insert a String->AST mapping into the specified scope. The string is the
 * string representation of the token of the name ast.
 */
static bool set_scope(ast_t* scope, ast_t* name, ast_t* value, bool type)
{
  const char* s = ast_name(name);

  if(type)
  {
    if(!is_type_id(s))
    {
      ast_error(name, "type name '%s' must start A-Z or _(A-Z)", s);
      return false;
    }
  } else {
    if(is_type_id(s))
    {
      ast_error(name, "identifier '%s' can't start A-Z or _(A-Z)", s);
      return false;
    }
  }

  if(!ast_set(scope, s, value))
  {
    ast_error(name, "can't reuse name '%s'", s);

    ast_t* prev = ast_get(scope, s);

    if(prev != NULL)
      ast_error(prev, "previous use of '%s'", s);

    return false;
  }

  return true;
}

/**
 * Import a package, either with a qualifying name or by merging it into the
 * current scope.
 */
static ast_t* use_package(ast_t* ast, const char* path, ast_t* name)
{
  ast_t* package = package_load(ast, path);

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return NULL;
  }

  if(name)
  {
    assert(ast_id(name) == TK_ID);

    if(set_scope(ast, name, package, false))
    {
      return package;
    } else {
      return NULL;
    }
  }

  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return NULL;
  }

  return package;
}

/**
 * Inserts entries in scopes. When this is complete, all scopes are fully
 * populated, including package imports, type names, type parameters in types,
 * field names, method names, type parameters in methods, parameters in methods,
 * and local variables.
 */
bool type_scope(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_PROGRAM:
      return use_package(ast, "builtin", NULL) != NULL;

    case TK_USE:
    {
      ast_t* path = ast_child(ast);
      ast_t* name = ast_sibling(path);

      return use_package(ast, ast_name(path), name) != NULL;
    }

    case TK_TYPEDECL:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return set_scope(ast_nearest(ast, TK_PACKAGE),
        ast_child(ast), ast, true);

    case TK_FVAR:
    case TK_FVAL:
    {
      if(ast_nearest(ast, TK_TRAIT) != NULL)
      {
        ast_error(ast, "can't have a field in a trait");
        return false;
      }

      return set_scope(ast, ast_child(ast), ast, false);
    }

    case TK_NEW:
    case TK_FUN:
      return set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false);

    case TK_BE:
    {
      if(ast_nearest(ast, TK_CLASS) != NULL)
      {
        ast_error(ast, "can't have a behaviour in a class");
        return false;
      }

      return set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false);
    }

    case TK_TYPEPARAM:
      return set_scope(ast, ast_child(ast), ast, true);

    case TK_PARAM:
      return set_scope(ast, ast_child(ast), ast, false);

    case TK_VAR:
    case TK_VAL:
      // FIX: child is an IDSEQ
      return set_scope(ast, ast_child(ast), ast, false);

    default: {}
  }

  return true;
}
