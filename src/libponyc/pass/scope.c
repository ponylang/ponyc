#include "scope.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include <assert.h>

static bool is_type_id(const char* s)
{
  int i = 0;

  if(s[i] == '_')
    i++;

  return (s[i] >= 'A') && (s[i] <= 'Z');
}

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
static ast_t* use_package(ast_t* ast, ast_t* name, const char* path)
{
  ast_t* package = package_load(ast, path, false);

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

static bool scope_package(ast_t* ast)
{
  return use_package(ast, NULL, "builtin") != NULL;
}

static bool scope_use(ast_t* ast)
{
  ast_t* path = ast_child(ast);
  ast_t* name = ast_sibling(path);

  return use_package(ast, name, ast_name(path)) != NULL;
}

static bool scope_idseq(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // each ID resolves to itself
    if(!set_scope(ast_parent(ast), child, child, false))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

ast_result_t pass_scope(ast_t** astp)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PACKAGE:
      if(!scope_package(ast))
        return AST_ERROR;
      break;

    case TK_USE:
      if(!scope_use(ast))
        return AST_FATAL;
      break;

    case TK_TYPE:
    case TK_TRAIT:
    case TK_DATA:
    case TK_CLASS:
    case TK_ACTOR:
      if(!set_scope(ast_nearest(ast, TK_PACKAGE), ast_child(ast), ast, true))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!set_scope(ast, ast_child(ast), ast, false))
        return AST_ERROR;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!set_scope(ast_parent(ast), ast_childidx(ast, 1), ast, false))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(ast, ast_child(ast), ast, true))
        return AST_ERROR;
      break;

    case TK_IDSEQ:
      if(!scope_idseq(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
