#include "scope.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
#include "../ast/symtab.h"
#include "../ast/token.h"
#include "../ast/stringtab.h"
#include "../ast/astbuild.h"
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

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_STRUCT:
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
    case TK_EMBED:
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

  if(package == NULL)
  {
    ast_error(ast, "can't load package '%s'", path);
    return false;
  }

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(NULL, ast, name, package);

  // Store the package so we can import it later without having to look it up
  // again
  ast_setdata(ast, (void*)package);
  return true;
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
      case TK_EMBED:
      {
        // Mark this field as SYM_UNDEFINED.
        AST_GET_CHILDREN(member, id, type, expr);
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

static ast_result_t scope_entity(typecheck_t* t, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeparams, cap, provides, members);

  if(!set_scope(t, t->frame->package, id, ast))
    return AST_ERROR;

  // Scope fields and methods immediately, so that the contents of method
  // signatures and bodies cannot shadow fields and methods.
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(!set_scope(t, member, ast_child(member), member))
          return AST_ERROR;
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
        if(!scope_method(t, member))
          return AST_ERROR;
        break;

      default:
        assert(0);
        return AST_FATAL;
    }

    member = ast_sibling(member);
  }

  return AST_OK;
}

static bool scope_local(typecheck_t* t, ast_t* ast)
{
  // Local resolves to itself
  ast_t* id = ast_child(ast);
  return set_scope(t, ast_parent(ast), id, id);
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_USE:
      return use_command(ast, options);

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return scope_entity(t, ast);

    case TK_PARAM:
      if(!set_scope(t, ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(t, ast, ast_child(ast), ast))
        return AST_ERROR;

      ast_setdata(ast, ast);
      break;

    case TK_MATCH_CAPTURE:
    case TK_LET:
    case TK_VAR:
      if(!scope_local(t, ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
