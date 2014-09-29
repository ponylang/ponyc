#include "scope.h"
#include "../ast/token.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
#include "../ds/stringtab.h"
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
static bool set_scope(ast_t* scope, ast_t* name, ast_t* value)
{
  assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  sym_status_t status = SYM_NONE;
  bool type = false;

  switch(ast_id(value))
  {
    case TK_ID:
    {
      if(ast_enclosing_pattern(value))
        status = SYM_DEFINED;
      else
        status = SYM_UNDEFINED;
      break;
    }

    case TK_TYPE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TYPEPARAM:
      type = true;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      status = SYM_DEFINED;
      break;

    case TK_PACKAGE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      break;

    default:
      assert(0);
      return false;
  }

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

  if(!ast_set(scope, s, value, status))
  {
    ast_error(name, "can't reuse name '%s'", s);

    ast_t* prev = ast_get(scope, s, NULL);

    if(prev != NULL)
      ast_error(prev, "previous use of '%s'", s);

    return false;
  }

  return true;
}

bool use_package(ast_t* ast, const char* path, ast_t* name,
  pass_opt_t* options)
{
  assert(ast != NULL);
  assert(path != NULL);

  ast_t* package = package_load(ast, path, options);

  if(package == ast)  // Stop builtin recursing
    return true;

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return false;
  }

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(ast, name, package);

  // We do not have an alias
  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return false;
  }

  return true;
}

static bool scope_package(ast_t* ast)
{
  // Every package has an implicit use "builtin" command
  return use_package(ast, stringtab("builtin"), NULL, NULL);
}

static bool scope_method(ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(ast_parent(ast), id, ast))
    return false;

  ast_t* members = ast_parent(ast);
  ast_t* type = ast_parent(members);

  switch(ast_id(type))
  {
    case TK_PRIMITIVE:
    case TK_TRAIT:
      return true;

    case TK_CLASS:
    case TK_ACTOR:
      break;

    case TK_STRUCTURAL:
    {
      // Disallow private members in structural types.
      const char* name = ast_name(id);

      if(name[0] == '_')
      {
        ast_error(id, "private method can't appear in structural types");
        return false;
      }

      return true;
    }

    default:
      assert(0);
      return false;
  }

  // If this isn't a constructor, we accept SYM_DEFINED for our fields.
  if(ast_id(ast) != TK_NEW)
    return true;

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        AST_GET_CHILDREN(member, id, type, expr);

        // If this field has an initialiser, we accept SYM_DEFINED for it.
        if(ast_id(expr) != TK_NONE)
          break;

        // Mark this field as SYM_UNDEFINED.
        ast_setstatus(ast, ast_name(id), SYM_UNDEFINED);
        break;
      }

      default:
        return true;
    }

    member = ast_sibling(member);
  }

  return true;
}

static bool scope_idseq(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // Each ID resolves to itself.
    if(!set_scope(ast_parent(ast), child, child))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_PACKAGE:
      if(!scope_package(ast))
        return AST_ERROR;
      break;

    case TK_USE:
      if(!use_command(ast, options, false))
        return AST_FATAL;
      break;

    case TK_TYPE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      if(!set_scope(ast_nearest(ast, TK_PACKAGE), ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!set_scope(ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!scope_method(ast))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(ast, ast_child(ast), ast))
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


ast_result_t pass_scope2(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  if(ast_id(ast) == TK_USE)
  {
    if(!use_command(ast, options, true))
      return AST_FATAL;

    return AST_OK;
  }

  return pass_scope(astp, options);
}
