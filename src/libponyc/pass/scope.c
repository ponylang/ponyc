#include "scope.h"
#include "../type/nominal.h"
#include "../pkg/package.h"
#include <assert.h>

/**
 * Insert a String->AST mapping into the specified scope. The string is the
 * string representation of the token of the name ast.
 */
static bool set_scope(ast_t* scope, ast_t* name, ast_t* value, bool type)
{
  assert(ast_id(name) == TK_ID);
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
static ast_t* use_package(ast_t* ast, ast_t* name, const char* path,
  int verbose)
{
  ast_t* package = package_load(ast, path, verbose);

  if(package == ast)
    return package;

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return NULL;
  }

  if(name && ast_id(name) == TK_ID)
  {
    if(!set_scope(ast, name, package, false))
      return NULL;

    return package;
  }

  assert((name == NULL) || (ast_id(name) == TK_NONE));

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
ast_result_t pass_scope(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_PACKAGE:
      if(use_package(ast, NULL, "builtin", verbose) == NULL)
        return AST_ERROR;
      break;

    case TK_USE:
    {
      ast_t* path = ast_child(ast);
      ast_t* name = ast_sibling(path);

      if(use_package(ast, name, ast_name(path), verbose) == NULL)
        return AST_ERROR;
      break;
    }

    case TK_TYPE:
    {
      // can't have a default capability
      ast_t* typeparams = ast_childidx(ast, 1);
      ast_t* cap = ast_sibling(typeparams);
      ast_t* alias = ast_child(ast_sibling(cap));

      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap, "can't specify a default capability for a type alias");
        return AST_ERROR;
      }

      if((alias != NULL) && (ast_sibling(alias) != NULL))
      {
        ast_error(alias, "a type alias can only alias to a single type");
        return AST_ERROR;
      }

      if(!set_scope(ast_nearest(ast, TK_PACKAGE),
        ast_child(ast), ast, true))
        return AST_ERROR;
      break;
    }

    case TK_ACTOR:
    {
      // can't have a default capability
      ast_t* cap = ast_childidx(ast, 2);

      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap, "can't specify a default for an actor");
        return AST_ERROR;
      }

      if(!set_scope(ast_nearest(ast, TK_PACKAGE),
        ast_child(ast), ast, true))
        return AST_ERROR;
      break;
    }

    case TK_TRAIT:
    case TK_CLASS:
      if(!set_scope(ast_nearest(ast, TK_PACKAGE),
        ast_child(ast), ast, true))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    {
      if(ast_nearest(ast, TK_TRAIT) != NULL)
      {
        ast_error(ast, "can't have a field in a trait");
        return false;
      }

      if(!set_scope(ast, ast_child(ast), ast, false))
        return AST_ERROR;
      break;
    }

    case TK_NEW:
    {
      ast_t* id = ast_childidx(ast, 1);

      if(ast_id(id) == TK_NONE)
      {
        ast_t* r_id = ast_from_string(id, "create");
        ast_replace(id, r_id);
      }

      if(!set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false))
        return AST_ERROR;
      break;
    }

    case TK_BE:
    {
      if(ast_nearest(ast, TK_CLASS) != NULL)
      {
        ast_error(ast, "can't have a behaviour in a class");
        return false;
      }

      if(!set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false))
        return AST_ERROR;
      break;
    }

    case TK_FUN:
    {
      ast_t* id = ast_childidx(ast, 1);

      if(ast_id(id) == TK_NONE)
      {
        ast_t* r_id = ast_from_string(id, "apply");
        ast_replace(id, r_id);
      }

      if(!set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false))
        return AST_ERROR;
      break;
    }

    case TK_TYPEPARAM:
      if(!set_scope(ast, ast_child(ast), ast, true))
        return AST_ERROR;
      break;

    case TK_PARAM:
      if(!set_scope(ast, ast_child(ast), ast, false))
        return AST_ERROR;
      break;

    case TK_NOMINAL:
    {
      // if we didn't have a package, this will have ID NONE [TYPEARGS]
      // change it to NONE ID [TYPEARGS] so the package is always first
      ast_t* package = ast_child(ast);
      ast_t* type = ast_sibling(package);

      if(ast_id(type) == TK_NONE)
      {
        ast_pop(ast);
        ast_pop(ast);
        ast_add(ast, package);
        ast_add(ast, type);
      }
      break;
    }

    case TK_IDSEQ:
    {
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        // each ID resolves to itself
        if(!set_scope(ast_parent(ast), child, child, false))
          return false;

        child = ast_sibling(child);
      }
      break;
    }

    default: {}
  }

  return AST_OK;
}
