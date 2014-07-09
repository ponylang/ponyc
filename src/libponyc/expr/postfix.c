#include "postfix.h"
#include "reference.h"
#include "../type/assemble.h"
#include "../type/nominal.h"
#include "../type/lookup.h"
#include "../type/cap.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool expr_packageaccess(ast_t* ast)
{
  // left is a packageref, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_PACKAGEREF);
  assert(ast_id(right) == TK_ID);

  // must be a type in a package
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name);

  if(package == NULL)
  {
    ast_error(right, "can't find package '%s'", package_name);
    return false;
  }

  assert(ast_id(package) == TK_PACKAGE);
  const char* typename = ast_name(right);
  type = ast_get(package, typename);

  if(type == NULL)
  {
    ast_error(right, "can't find type '%s' in package '%s'",
      typename, package_name);
    return false;
  }

  ast_settype(ast, nominal_sugar(ast, package_name, typename));
  ast_setid(ast, TK_TYPEREF);
  return expr_typeref(ast);
}

static bool expr_typeaccess(ast_t* ast)
{
  // left is a typeref, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(left) == TK_TYPEREF);
  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(ast, type, ast_name(right));

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
      ast_setid(ast, TK_FUNREF);
      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);
      break;
    }

    case TK_FVAR:
    case TK_FLET:
    case TK_BE:
    case TK_FUN:
    {
      // make this a lookup on a default constructed object
      ast_free_unattached(find);

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, stringtab("create")));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(dot))
        return false;

      return expr_dot(ast);
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

static bool expr_memberaccess(ast_t* ast)
{
  // left is a postfix expression, right is an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(ast, type, ast_name(right));

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
      // TODO: viewpoint adapted type of the field
      ast_setid(ast, TK_FVARREF);
      ast_settype(ast, ast_type(find));
      break;
    }

    case TK_FLET:
    {
      // TODO: viewpoint adapted type of the field
      ast_setid(ast, TK_FLETREF);
      ast_settype(ast, ast_type(find));
      break;
    }

    case TK_NEW:
    {
      ast_error(right, "can't look up a constructor on an expression");
      ret = false;
      break;
    }

    case TK_BE:
    case TK_FUN:
    {
      // check receiver cap
      token_id rcap = cap_for_type(ast_type(left));
      token_id fcap = cap_for_fun(find);

      if(!is_cap_sub_cap(rcap, fcap))
      {
        ast_error(ast,
          "receiver capability is not a subtype of method capability");
        return false;
      }

      ast_setid(ast, TK_FUNREF);
      ast_settype(ast, type_for_fun(find));

      if(ast_id(ast_childidx(find, 5)) == TK_QUESTION)
        ast_seterror(ast);
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
  ast_inheriterror(ast);
  return ret;
}

static bool expr_tupleaccess(ast_t* ast)
{
  // left is a postfix expression, right is an integer
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  assert(ast_id(right) == TK_INT);

  // element of a tuple
  if((type == NULL) || (ast_id(type) != TK_TUPLETYPE))
  {
    ast_error(right, "member by position can only be used on a tuple");
    return false;
  }

  type = lookup_tuple(type, ast_int(right));

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

bool expr_qualify(ast_t* ast)
{
  // left is a postfix expression, right is a typeargs
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);
  assert(ast_id(right) == TK_TYPEARGS);

  switch(ast_id(left))
  {
    case TK_TYPEREF:
    {
      // qualify the type
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

      return expr_typeref(ast);
    }

    case TK_FUNREF:
    {
      // TODO: qualify the function
      assert(ast_id(type) == TK_FUNTYPE);
      ast_error(ast, "not implemented (qualify a function)");
      ast_inheriterror(ast);
      return false;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool expr_dot(ast_t* ast)
{
  // left is a postfix expression, right is an integer or an id
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  switch(ast_id(right))
  {
    case TK_ID:
    {
      switch(ast_id(left))
      {
        case TK_PACKAGEREF:
          return expr_packageaccess(ast);

        case TK_TYPEREF:
          return expr_typeaccess(ast);

        default: {}
      }

      return expr_memberaccess(ast);
    }

    case TK_INT:
      return expr_tupleaccess(ast);

    default: {}
  }

  assert(0);
  return false;
}

bool expr_call(ast_t* ast)
{
  ast_t* left = ast_child(ast);
  ast_t* type = ast_type(left);

  switch(ast_id(left))
  {
    case TK_INT:
    case TK_FLOAT:
    case TK_STRING:
    case TK_ARRAY:
    case TK_OBJECT:
    case TK_THIS:
    case TK_FVARREF:
    case TK_FLETREF:
    case TK_VARREF:
    case TK_LETREF:
    case TK_PARAMREF:
    case TK_CALL:
    {
      // apply sugar
      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, stringtab("apply")));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(dot))
        return false;

      return expr_call(ast);
    }

    case TK_FUNREF:
    {
      // TODO: use args to decide unbound type parameters
      assert(ast_id(type) == TK_FUNTYPE);
      ast_t* typeparams = ast_child(type);

      if(ast_id(typeparams) != TK_NONE)
      {
        ast_error(ast,
          "can't call a function with unqualified type parameters");
        return false;
      }

      ast_settype(ast, ast_childidx(type, 2));
      ast_inheriterror(ast);
      return true;
    }

    case TK_TUPLE:
    {
      ast_error(ast, "can't call a tuple");
      return false;
    }

    default: {}
  }

  assert(0);
  return false;
}
