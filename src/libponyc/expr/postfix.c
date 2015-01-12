#include "postfix.h"
#include "reference.h"
#include "literal.h"
#include "call.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include <assert.h>

static bool method_access(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_QUALIFY:
      return method_access(parent);

    case TK_CALL:
      return true;

    default: {}
  }

  ast_error(ast, "can't reference a method without calling it");
  return false;
}

static bool package_access(pass_opt_t* opt, ast_t* ast)
{
  // left is a packageref, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_PACKAGEREF);
  assert(ast_id(right) == TK_ID);

  // must be a type in a package
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name, NULL);

  if(package == NULL)
  {
    ast_error(right, "can't access package '%s'", package_name);
    return false;
  }

  assert(ast_id(package) == TK_PACKAGE);
  const char* type_name = ast_name(right);
  type = ast_get(package, type_name, NULL);

  if(type == NULL)
  {
    ast_error(right, "can't find type '%s' in package '%s'",
      type_name, package_name);
    return false;
  }

  ast_settype(ast, type_sugar(ast, package_name, type_name));
  ast_setid(ast, TK_TYPEREF);
  return expr_typeref(opt, ast);
}

static bool type_access(pass_opt_t* opt, ast_t* ast)
{
  typecheck_t* t = &opt->check;

  // Left is a typeref, right is an id.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_TYPEREF);
  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(t, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
    {
      ast_error(right, "can't look up a typeparam on a type");
      ret = false;
      break;
    }

    case TK_NEW:
    {
      ast_t* def = (ast_t*)ast_data(type);

      if((ast_id(type) == TK_NOMINAL) && (ast_id(def) == TK_ACTOR))
        ast_setid(ast, TK_NEWBEREF);
      else
        ast_setid(ast, TK_NEWREF);

      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);

      ret = method_access(ast);
      break;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_BE:
    case TK_FUN:
    {
      // Make this a lookup on a default constructed object.
      ast_free_unattached(find);

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(left, dot);
      ast_add(dot, left);

      ast_t* call = ast_from(ast, TK_CALL);
      ast_swap(dot, call);
      ast_add(call, dot); // the LHS goes at the end, not the beginning
      ast_add(call, ast_from(ast, TK_NONE)); // named
      ast_add(call, ast_from(ast, TK_NONE)); // positional

      if(!expr_dot(opt, dot))
        return false;

      if(!expr_call(opt, &call))
        return false;

      return expr_dot(opt, ast);
    }

    default:
    {
      assert(0);
      ret = false;
      break;
    }
  }

  ast_free_unattached(find);
  return ret;
}

static bool make_tuple_index(ast_t** astp)
{
  ast_t* ast = *astp;
  const char* name = ast_name(ast);

  if(name[0] != '_')
    return false;

  for(size_t i = 1; name[i] != '\0'; i++)
  {
    if((name[i] < '0') || (name[i] > '9'))
      return false;
  }

  size_t index = strtol(&name[1], NULL, 10) - 1;
  ast_t* node = ast_from_int(ast, index);
  ast_replace(astp, node);

  return true;
}

static bool tuple_access(ast_t* ast)
{
  // Left is a postfix expression, right is a lookup name.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  // Change the lookup name to an integer index.
  if(!make_tuple_index(&right))
  {
    ast_error(right,
      "lookup on a tuple must take the form _X, where X is an integer");
    return false;
  }

  // Make sure our index is in bounds.
  type = ast_childidx(type, (size_t)ast_int(right));

  if(type == NULL)
  {
    ast_error(right, "tuple index is out of bounds");
    return false;
  }

  ast_setid(ast, TK_FLETREF);
  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

static bool member_access(typecheck_t* t, ast_t* ast, bool partial)
{
  // Left is a postfix expression, right is an id.
  AST_GET_CHILDREN(ast, left, right);
  ast_t* type = ast_type(left);
  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(t, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
    {
      ast_error(right, "can't look up a typeparam on an expression");
      ret = false;
      break;
    }

    case TK_FVAR:
    {
      if(!expr_fieldref(t, ast, find, TK_FVARREF))
        return false;
      break;
    }

    case TK_FLET:
    {
      if(!expr_fieldref(t, ast, find, TK_FLETREF))
        return false;
      break;
    }

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      switch(ast_id(find))
      {
        case TK_NEW:
        {
          ast_t* def = (ast_t*)ast_data(type);

          if((ast_id(type) == TK_NOMINAL) && (ast_id(def) == TK_ACTOR))
            ast_setid(ast, TK_NEWBEREF);
          else
            ast_setid(ast, TK_NEWREF);
          break;
        }

        case TK_BE:
          ast_setid(ast, TK_BEREF);
          break;

        case TK_FUN:
          ast_setid(ast, TK_FUNREF);
          break;

        default:
          assert(0);
          return false;
      }

      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);

      ret = method_access(ast);
      break;
    }

    default:
    {
      assert(0);
      ret = false;
      break;
    }
  }

  ast_free_unattached(find);

  if(!partial)
    ast_inheriterror(ast);

  return ret;
}

bool expr_qualify(pass_opt_t* opt, ast_t* ast)
{
  // Left is a postfix expression, right is a typeargs.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);
  assert(ast_id(right) == TK_TYPEARGS);

  switch(ast_id(left))
  {
    case TK_TYPEREF:
    {
      // Qualify the type.
      assert(ast_id(type) == TK_NOMINAL);

      if(ast_id(ast_childidx(type, 2)) != TK_NONE)
      {
        ast_error(ast, "can't qualify an already qualified type");
        return false;
      }

      type = ast_dup(type);
      ast_t* typeargs = ast_childidx(type, 2);
      ast_replace(&typeargs, right);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(opt, ast);
    }

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_NEWBEAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    {
      // Qualify the function.
      assert(ast_id(type) == TK_FUNTYPE);
      ast_t* typeparams = ast_childidx(type, 1);

      if(!check_constraints(typeparams, right, true))
        return false;

      type = reify(type, typeparams, right);
      typeparams = ast_childidx(type, 1);
      ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

      ast_settype(ast, type);
      ast_setid(ast, ast_id(left));
      ast_inheriterror(ast);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool dot_or_tilde(pass_opt_t* opt, ast_t* ast, bool partial)
{
  typecheck_t* t = &opt->check;

  // Left is a postfix expression, right is an id.
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_PACKAGEREF:
      return package_access(opt, ast);

    case TK_TYPEREF:
      return type_access(opt, ast);

    default: {}
  }

  ast_t* type = ast_type(left);

  if(type == NULL)
  {
    ast_error(ast, "invalid left hand side");
    return false;
  }

  if(!literal_member_access(ast, opt))
    return false;

  // Type already set by literal handler
  if(ast_type(ast) != NULL)
    return true;

  type = ast_type(left); // Literal handling may have changed lhs type
  assert(type != NULL);

  if(ast_id(type) == TK_TUPLETYPE)
    return tuple_access(ast);

  return member_access(t, ast, partial);
}

bool expr_dot(pass_opt_t* opt, ast_t* ast)
{
  return dot_or_tilde(opt, ast, false);
}

bool expr_tilde(pass_opt_t* opt, ast_t* ast)
{
  if(!dot_or_tilde(opt, ast, true))
    return false;

  if(ast_id(ast) == TK_TILDE && ast_type(ast) != NULL &&
    ast_id(ast_type(ast)) == TK_OPERATORLITERAL)
  {
    ast_error(ast, "can't do partial application on a literal number");
    return false;
  }

  switch(ast_id(ast))
  {
    case TK_NEWREF:
      ast_setid(ast, TK_NEWAPP);
      return true;

    case TK_NEWBEREF:
      ast_setid(ast, TK_NEWBEAPP);
      return true;

    case TK_BEREF:
      ast_setid(ast, TK_BEAPP);
      return true;

    case TK_FUNREF:
      ast_setid(ast, TK_FUNAPP);
      return true;

    case TK_TYPEREF:
      ast_error(ast, "can't do partial application on a package");
      return false;

    case TK_FVARREF:
    case TK_FLETREF:
      ast_error(ast, "can't do partial application of a field");
      return false;

    default: {}
  }

  assert(0);
  return false;
}
