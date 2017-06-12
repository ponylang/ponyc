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
static bool set_scope(pass_opt_t* opt, ast_t* scope, ast_t* name, ast_t* value,
  bool allow_shadowing)
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

  if(!ast_set(scope, s, value, status, allow_shadowing))
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

  // Store the package so we can import it later without having to look it up
  // again
  ast_setdata(ast, (void*)package);

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(options, ast, name, package, false);

  ast_setflag(ast, AST_FLAG_IMPORT);

  return true;
}

static bool scope_method(pass_opt_t* opt, ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(opt, ast_parent(ast), id, ast, false))
    return false;

  return true;
}

static ast_result_t scope_entity(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeparams, cap, provides, members);

  if(!set_scope(opt, opt->check.frame->package, id, ast, false))
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
        if(!set_scope(opt, member, ast_child(member), member, false))
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

static ast_t* make_iftype_typeparam(pass_opt_t* opt, ast_t* subtype,
  ast_t* supertype, ast_t* scope)
{
  pony_assert(ast_id(subtype) == TK_NOMINAL);

  const char* name = ast_name(ast_childidx(subtype, 1));
  ast_t* def = ast_get(scope, name, NULL);
  if(def == NULL)
  {
    ast_error(opt->check.errors, ast_child(subtype),
      "can't find definition of '%s'", name);
    return NULL;
  }

  if(ast_id(def) != TK_TYPEPARAM)
  {
    ast_error(opt->check.errors, subtype, "the subtype in an iftype condition "
      "must be a type parameter or a tuple of type parameters");
    return NULL;
  }

  ast_t* current_constraint = ast_childidx(def, 1);
  ast_t* new_constraint = ast_dup(supertype);
  if((ast_id(current_constraint) != TK_NOMINAL) ||
    (ast_name(ast_childidx(current_constraint, 1)) != name))
  {
    // If the constraint is the type parameter itself, there is no constraint.
    // We can't use type_isect to build the new constraint because we don't have
    // full type information yet.
    BUILD(isect, new_constraint,
      NODE(TK_ISECTTYPE,
        TREE(ast_dup(current_constraint))
        TREE(new_constraint)));

    new_constraint = isect;
  }

  BUILD(typeparam, def,
    NODE(TK_TYPEPARAM,
      ID(name)
      TREE(new_constraint)
      NONE));

  // keep data pointing to the original def
  ast_setdata(typeparam, ast_data(def));

  return typeparam;
}

static ast_result_t scope_iftype(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, subtype, supertype, then_clause);

  ast_t* typeparams = ast_from(ast, TK_TYPEPARAMS);

  switch(ast_id(subtype))
  {
    case TK_NOMINAL:
    {
      ast_t* typeparam = make_iftype_typeparam(opt, subtype, supertype,
        then_clause);
      if(typeparam == NULL)
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(!set_scope(opt, then_clause, ast_child(typeparam), typeparam, true))
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_add(typeparams, typeparam);
      break;
    }

    case TK_TUPLETYPE:
    {
      if(ast_id(supertype) != TK_TUPLETYPE)
      {
        ast_error(opt->check.errors, subtype, "iftype subtype is a tuple but "
          "supertype isn't");
        ast_error_continue(opt->check.errors, supertype, "Supertype is %s",
          ast_print_type(supertype));
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(ast_childcount(subtype) != ast_childcount(supertype))
      {
        ast_error(opt->check.errors, subtype, "the subtype and the supertype "
          "in an iftype condition must have the same cardinality");
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_t* sub_child = ast_child(subtype);
      ast_t* super_child = ast_child(supertype);
      while(sub_child != NULL)
      {
        ast_t* typeparam = make_iftype_typeparam(opt, sub_child, super_child,
          then_clause);
        if(typeparam == NULL)
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        if(!set_scope(opt, then_clause, ast_child(typeparam), typeparam, true))
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        ast_add(typeparams, typeparam);
        sub_child = ast_sibling(sub_child);
        super_child = ast_sibling(super_child);
      }

      break;
    }

    default:
      ast_error(opt->check.errors, subtype, "the subtype in an iftype "
        "condition must be a type parameter or a tuple of type parameters");
      ast_free_unattached(typeparams);
      return AST_ERROR;
  }

  // We don't want the scope pass to run on typeparams. The compiler would think
  // that type parameters are declared twice.
  ast_pass_record(typeparams, PASS_SCOPE);
  ast_append(ast, typeparams);
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
      if(!set_scope(options, ast, ast_child(ast), ast, false))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(options, ast, ast_child(ast), ast, false))
        return AST_ERROR;

      // Store the original definition of the typeparam in the data field here.
      // It will be retained later if the typeparam def is copied via ast_dup.
      ast_setdata(ast, ast);
      break;

    case TK_IFTYPE:
      return scope_iftype(options, ast);

    default: {}
  }

  return AST_OK;
}
