#include "postfix.h"
#include "reference.h"
#include "../ast/token.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/alias.h"
#include "../type/cap.h"
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
      ast_t* def = (ast_t*)ast_data(type);

      if((ast_id(type) == TK_NOMINAL) && (ast_id(def) == TK_ACTOR))
        ast_setid(ast, TK_NEWBEREF);
      else
        ast_setid(ast, TK_NEWREF);

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
      // Make this a lookup on a default constructed object.
      ast_free_unattached(find);

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(dot))
        return false;

      ast_t* call = ast_from(ast, TK_CALL);
      ast_add(call, ast_from(ast, TK_NONE)); // named
      ast_add(call, ast_from(ast, TK_NONE)); // positional
      ast_swap(dot, call);
      ast_add(call, dot);

      if(!expr_call(call))
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
      if(!expr_fieldref(ast, left, find, TK_FVARREF))
        return false;
      break;
    }

    case TK_FLET:
    {
      if(!expr_fieldref(ast, left, find, TK_FLETREF))
        return false;
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
      if(ast_id(find) == TK_BE)
        ast_setid(ast, TK_BEREF);
      else
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

  type = ast_childidx(type, ast_int(right));

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

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    {
      // qualify the function
      assert(ast_id(type) == TK_FUNTYPE);
      ast_t* typeparams = ast_childidx(type, 1);

      if(!check_constraints(typeparams, right))
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
  token_id token = ast_id(left);

  switch(token)
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
      ast_add(dot, ast_from_string(ast, "apply"));
      ast_swap(left, dot);
      ast_add(dot, left);

      if(!expr_dot(dot))
        return false;

      return expr_call(ast);
    }

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    {
      // TODO: use args to decide unbound type parameters
      assert(ast_id(type) == TK_FUNTYPE);

      bool sending = (token == TK_NEWBEREF) || (token == TK_BEREF);
      bool send = true;

      ast_t* cap = ast_child(type);
      ast_t* typeparams = ast_sibling(cap);
      ast_t* params = ast_sibling(typeparams);
      ast_t* result = ast_sibling(params);

      if(ast_id(typeparams) != TK_NONE)
      {
        ast_error(ast,
          "can't call a function with unqualified type parameters");
        return false;
      }

      ast_t* positional = ast_sibling(left);
      ast_t* named = ast_sibling(positional);

      // TODO: named arguments
      if(ast_id(named) != TK_NONE)
      {
        ast_error(named, "not implemented (named arguments)");
        return false;
      }

      // check positional args vs params
      ast_t* param = ast_child(params);
      ast_t* arg = ast_child(positional);

      while((arg != NULL) && (param != NULL))
      {
        // param can be a type or an actual param
        ast_t* p_type;

        if(ast_id(param) == TK_PARAM)
          p_type = ast_childidx(param, 1);
        else
          p_type = param;

        ast_t* a_type = alias(ast_type(arg));
        send &= sendable(a_type);

        // If we are sending, we need to drop to a sending capability before
        // checking subtyping.
        if(sending)
        {
          ast_t* s_type = send_type(a_type);

          if(s_type != a_type)
          {
            ast_free_unattached(a_type);
            a_type = s_type;
          }
        }

        if(!is_subtype(a_type, p_type))
        {
          if(sending)
            ast_error(arg, "sent argument not a subtype of parameter");
          else
            ast_error(arg, "argument not a subtype of parameter");

          ast_free_unattached(a_type);
          return false;
        }

        ast_free_unattached(a_type);
        arg = ast_sibling(arg);
        param = ast_sibling(param);
      }

      if(arg != NULL)
      {
        ast_error(arg, "too many arguments");
        return false;
      }

      // pick up default args
      while(param != NULL)
      {
        if(ast_id(param) == TK_PARAM)
          arg = ast_childidx(param, 2);
        else
          arg = NULL;

        if((arg == NULL) || (ast_id(arg) == TK_NONE))
        {
          ast_error(ast, "not enough arguments");
          return false;
        }

        // TODO: the meaning of 'this' in the default arg is the receiver, not
        // the caller. can't just copy it.
        ast_error(positional, "not implemented (default arguments)");
        return false;

        // param = ast_sibling(param);
      }

      switch(token)
      {
        case TK_FUNREF:
        {
          // if args and result are sendable, don't alias the receiver
          send &= sendable(result);

          // check receiver cap
          ast_t* receiver = ast_child(left);

          // dig through function qualification
          if(ast_id(receiver) == TK_FUNREF)
            receiver = ast_child(receiver);

          ast_t* r_type = ast_type(receiver);

          if(!send)
            r_type = alias(r_type);

          token_id rcap = cap_for_type(r_type);
          token_id fcap = ast_id(cap);
          ast_free_unattached(r_type);

          if(!is_cap_sub_cap(rcap, fcap))
          {
            ast_error(ast,
              "receiver capability is not a subtype of method capability");
            return false;
          }

          break;
        }

        default: {}
      }

      ast_settype(ast, result);
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

bool expr_ffi(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeargs, args);
  ast_t* type = ast_child(typeargs);

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}
