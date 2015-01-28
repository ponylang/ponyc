#include "scope.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
#include "../ast/symtab.h"
#include "../ast/token.h"
#include "../ast/stringtab.h"
#include <string.h>
#include <assert.h>

/**
 * Insert a name->AST mapping into the specified scope.
 */
static bool set_scope(typecheck_t* t, ast_t* scope, ast_t* name, ast_t* value)
{
  assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  sym_status_t status = SYM_NONE;

  switch(ast_id(value))
  {
    case TK_ID:
    {
      if((t != NULL) && (t->frame->pattern != NULL))
        status = SYM_DEFINED;
      else
        status = SYM_UNDEFINED;

      break;
    }

    case TK_FFIDECL:
    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TYPEPARAM:
    case TK_PACKAGE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      break;

    case TK_FVAR:
    case TK_FLET:
      status = SYM_DEFINED;
      break;

    case TK_PARAM:
      status = SYM_DEFINED;
      break;

    default:
      assert(0);
      return false;
  }

  if(!ast_set(scope, s, value, status))
  {
    ast_t* prev = ast_get(scope, s, NULL);
    ast_t* prev_nocase = ast_get_case(scope, s, NULL);

    ast_error(name, "can't reuse name '%s'", s);
    ast_error(prev_nocase, "previous use of '%s'%s",
      s, (prev == NULL) ? " differs only by case" : "");

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
    return set_scope(NULL, ast, name, package);

  // We do not have an alias
  if(!ast_merge(ast, package))
  {
    ast_error(ast, "can't merge symbols from '%s'", path);
    return false;
  }

  return true;
}

static bool scope_module(ast_t* ast, pass_opt_t* options)
{
  // Every module has an implicit use "builtin" command
  return use_package(ast, stringtab("builtin"), NULL, options);
}

static void set_fields_undefined(ast_t* ast)
{
  assert(ast_id(ast) == TK_NEW);

  ast_t* members = ast_parent(ast);
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

      default: {}
    }

    member = ast_sibling(member);
  }
}

static bool scope_method(typecheck_t* t, ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(t, ast_parent(ast), id, ast))
    return false;

  if(ast_id(ast) == TK_NEW)
    set_fields_undefined(ast);

  return true;
}

static bool scope_idseq(typecheck_t* t, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  bool r = true;

  while(child != NULL)
  {
    // Each ID resolves to itself.
    if(!set_scope(t, ast_parent(ast), child, child))
      r = false;

    child = ast_sibling(child);
  }

  return r;
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_MODULE:
      if(!scope_module(ast, options))
        return AST_FATAL;
      break;

    case TK_USE:
      if(!use_command(ast, options))
        return AST_FATAL;
      break;

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_FFIDECL:
      if(!set_scope(t, t->frame->package, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
      if(!set_scope(t, ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      if(!scope_method(t, ast))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(t, ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_IDSEQ:
      if(!scope_idseq(t, ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
