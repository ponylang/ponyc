#include "postfix.h"
#include "reference.h"
#include "literal.h"
#include "../pass/expr.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/alias.h"
#include "../type/cap.h"
#include "../ast/token.h"
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
      ast_swap(dot, call);
      ast_add(call, dot); // the LHS goes at the end, not the beginning
      ast_add(call, ast_from(ast, TK_NONE)); // named
      ast_add(call, ast_from(ast, TK_NONE)); // positional

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

static bool expr_tupleaccess(ast_t* ast)
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

static bool expr_memberaccess(ast_t* ast)
{
  // Left is a postfix expression, right is an id.
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

bool expr_dot(ast_t* ast)
{
  // Left is a postfix expression, right an id.
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_PACKAGEREF:
      return expr_packageaccess(ast);

    case TK_TYPEREF:
      return expr_typeaccess(ast);

    default: {}
  }

  ast_t* type = ast_type(left);

  if(type == NULL)
  {
    ast_error(ast, "invalid left hand side");
    return false;
  }

  if(is_type_literal(type))
    return coerce_literal_member(ast);

  if(ast_id(type) == TK_TUPLETYPE)
    return expr_tupleaccess(ast);

  return expr_memberaccess(ast);
}

bool expr_call(ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, lhs);
  ast_t* type = ast_type(lhs);
  token_id token = ast_id(lhs);

  if(!coerce_literal_operator(ast))
    return false;

  if(ast_type(ast) != NULL) // Type already set by literal handler
    return true;

  if(type != NULL && is_type_literal(type))
  {
    ast_error(ast, "Cannot call a literal");
    return false;
  }

  switch(token)
  {
    case TK_STRING:
    case TK_ARRAY:
    case TK_OBJECT:
    case TK_TUPLE:
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
      ast_swap(lhs, dot);
      ast_add(dot, lhs);

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
        ast_t* p_type = ast_childidx(param, 1);
        ast_t* arg_type = ast_type(arg);

        if(arg_type == NULL)
        {
          ast_error(arg, "can't use return, break or continue in an argument");
          return false;
        }

        if(!coerce_literals(arg, p_type))
          return false;

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

          ast_error(p_type, "parameter type: %s", ast_print_type(p_type));
          ast_error(a_type, "argument type: %s", ast_print_type(a_type));

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
        arg = ast_childidx(param, 2);

        if(ast_id(arg) == TK_NONE)
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
          // We can ignore the result type if the result is not used.
          if(is_result_needed(ast))
            send &= sendable(result);

          // Check receiver cap.
          ast_t* receiver = ast_child(lhs);

          // Dig through function qualification.
          if(ast_id(receiver) == TK_FUNREF)
            receiver = ast_child(receiver);

          ast_t* r_type = ast_type(receiver);

          // If args and result are sendable, don't alias the receiver.
          if(!send)
            r_type = alias(r_type);

          token_id rcap = cap_for_type(r_type);
          token_id fcap = ast_id(cap);

          if(!is_cap_sub_cap(rcap, fcap))
          {
            ast_error(ast,
              "receiver capability is not a subtype of method capability");

            ast_error(receiver, "receiver type: %s", ast_print_type(r_type));
            ast_error(cap, "expected capability");

            if(!send)
            {
              rcap = cap_for_type(ast_type(receiver));

              if(is_cap_sub_cap(rcap, fcap))
              {
                ast_error(ast,
                  "this would be possible if the arguments and return value "
                  "were all sendable");
              }
            }

            ast_free_unattached(r_type);
            return false;
          }

          ast_free_unattached(r_type);
          break;
        }

        default: {}
      }

      ast_settype(ast, result);
      ast_inheriterror(ast);
      return true;
    }

    default: {}
  }

  assert(0);
  return false;
}


static bool expr_declared_ffi(ast_t* call, ast_t* decl)
{
  assert(call != NULL);
  assert(decl != NULL);
  assert(ast_id(decl) == TK_FFIDECL);

  AST_GET_CHILDREN(call, call_name, call_ret_typeargs, args, named_args,
    call_error);
  AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
    decl_error);

  // Check args vs params
  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(args);

  while((arg != NULL) && (param != NULL) && ast_id(param) != TK_ELLIPSIS)
  {
    ast_t* p_type = ast_childidx(param, 1);

    if(!coerce_literals(arg, p_type))
      return false;

    ast_t* a_type = ast_type(arg);

    if(!is_subtype(a_type, p_type))
    {
      ast_error(arg, "argument not a subtype of parameter");
      ast_error(p_type, "parameter type: %s", ast_print_type(p_type));
      ast_error(a_type, "argument type: %s", ast_print_type(a_type));
      return false;
    }

    arg = ast_sibling(arg);
    param = ast_sibling(param);
  }

  if(arg != NULL && param == NULL)
  {
    ast_error(arg, "too many arguments");
    return false;
  }

  if(param != NULL && ast_id(param) != TK_ELLIPSIS)
  {
    ast_error(named_args, "too few arguments");
    return false;
  }

  for(; arg != NULL; arg = ast_sibling(arg))
  {
    if(is_type_literal(ast_type(arg)))
    {
      ast_error(arg, "Cannot pass number literals as unchecked FFI arguments");
      return false;
    }
  }

  // Check return types
  ast_t* call_ret_type = ast_child(call_ret_typeargs);
  ast_t* decl_ret_type = ast_child(decl_ret_typeargs);

  if(call_ret_type != NULL && !is_eqtype(call_ret_type, decl_ret_type))
  {
    ast_error(call_ret_type, "call return type does not match declaration");
    return false;
  }

  // Check partiality
  if((ast_id(decl_error) == TK_NONE) && (ast_id(call_error) != TK_NONE))
  {
    ast_error(call_error, "call is partial but the declaration is not");
    return false;
  }

  if((ast_id(decl_error) == TK_QUESTION) ||
    (ast_id(call_error) == TK_QUESTION))
  {
    ast_seterror(call);
  }

  ast_settype(call, decl_ret_type);
  return true;
}


bool expr_ffi(ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  assert(name != NULL);

  ast_t* decl = ast_get(ast, ast_name(name), NULL);

  if(decl != NULL)  // We have a declaration
    return expr_declared_ffi(ast, decl);

  // We do not have a declaration
  for(ast_t* arg = ast_child(args); arg != NULL; arg = ast_sibling(arg))
  {
    if(is_type_literal(ast_type(arg)))
    {
      ast_error(arg, "Cannot pass number literals as unchecked FFI arguments");
      return false;
    }
  }

  ast_t* return_type = ast_child(return_typeargs);

  if(return_type == NULL)
  {
    ast_error(name, "FFIs without declarations must specify return type");
    return false;
  }

  ast_settype(ast, return_type);

  if(ast_id(question) == TK_QUESTION)
    ast_seterror(ast);

  return true;
}
