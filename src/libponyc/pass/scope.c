#include "scope.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
#include "../ast/symtab.h"
#include "../ast/token.h"
#include "../ast/stringtab.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "ponyassert.h"
#include <string.h>

/**
 * Insert a name->AST mapping into the specified scope.
 */
static bool set_scope(pass_opt_t* opt, ast_t* scope,
  ast_t* name, ast_t* value)
{
  pony_assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  if(is_name_dontcare(s))
    return true;

  sym_status_t status = SYM_NONE;

  switch(ast_id(value))
  {
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

    case TK_VAR:
    case TK_LET:
      status = SYM_UNDEFINED;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
      status = SYM_DEFINED;
      break;

    default:
      pony_assert(0);
      return false;
  }

  if(!ast_set(scope, s, value, status, false))
  {
    ast_t* prev = ast_get(scope, s, NULL);
    ast_t* prev_nocase = ast_get_case(scope, s, NULL);

    ast_error(opt->check.errors, name, "can't reuse name '%s'", s);
    ast_error_continue(opt->check.errors, prev_nocase,
      "previous use of '%s'%s",
      s, (prev == NULL) ? " differs only by case" : "");

    return false;
  }

  return true;
}

bool use_package(ast_t* ast, const char* path, ast_t* name,
  pass_opt_t* options)
{
  pony_assert(ast != NULL);
  pony_assert(path != NULL);

  ast_t* package = package_load(ast, path, options);

  if(package == NULL)
  {
    ast_error(options->check.errors, ast, "can't load package '%s'", path);
    return false;
  }

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(options, ast, name, package);

  // Store the package so we can import it later without having to look it up
  // again
  ast_setdata(ast, (void*)package);
  return true;
}

static bool scope_method(pass_opt_t* opt, ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(opt, ast_parent(ast), id, ast))
    return false;

  return true;
}

static ast_result_t scope_entity(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeparams, cap, provides, members);

  if(!set_scope(opt, opt->check.frame->package, id, ast))
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
        if(!set_scope(opt, member, ast_child(member), member))
          return AST_ERROR;
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
        if(!scope_method(opt, member))
          return AST_ERROR;
        break;

      default:
        pony_assert(0);
        return AST_FATAL;
    }

    member = ast_sibling(member);
  }

  return AST_OK;
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
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
      return scope_entity(options, ast);

    case TK_VAR:
    case TK_LET:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
      if(!set_scope(options, ast, ast_child(ast), ast))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(options, ast, ast_child(ast), ast))
        return AST_ERROR;

      // Store the original definition of the typeparam in the data field here.
      // It will be retained later if the typeparam def is copied via ast_dup.
      ast_setdata(ast, ast);
      break;

    default: {}
  }

  return AST_OK;
}
