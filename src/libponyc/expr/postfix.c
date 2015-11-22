#include "postfix.h"
#include "reference.h"
#include "literal.h"
#include "call.h"
#include "../pkg/package.h"
#include "../pass/expr.h"
#include "../pass/names.h"
#include "../type/reify.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include <string.h>
#include <assert.h>

static bool is_method_called(ast_t* ast)
{
  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_QUALIFY:
      return is_method_called(parent);

    case TK_CALL:
    case TK_ADDRESS:
      return true;

    default: {}
  }

  ast_error(ast, "can't reference a method without calling it");
  return false;
}

static bool constructor_type(ast_t* ast, token_id cap, ast_t* type,
  ast_t** resultp)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);

      switch(ast_id(def))
      {
        case TK_PRIMITIVE:
        case TK_CLASS:
          ast_setid(ast, TK_NEWREF);
          break;

        case TK_ACTOR:
          ast_setid(ast, TK_NEWBEREF);
          break;

        case TK_TYPE:
          ast_error(ast, "can't call a constructor on a type alias: %s",
            ast_print_type(type));
          return false;

        case TK_INTERFACE:
          ast_error(ast, "can't call a constructor on an interface: %s",
            ast_print_type(type));
          return false;

        case TK_TRAIT:
          ast_error(ast, "can't call a constructor on a trait: %s",
            ast_print_type(type));
          return false;

        default:
          assert(0);
          return false;
      }
      return true;
    }

    case TK_TYPEPARAMREF:
    {
      // Alter the return type of the method.
      type = ast_dup(type);

      AST_GET_CHILDREN(type, tid, tcap, teph);
      ast_setid(tcap, cap);
      ast_setid(teph, TK_EPHEMERAL);
      ast_replace(resultp, type);

      // This could this be an actor.
      ast_setid(ast, TK_NEWBEREF);
      return true;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      return constructor_type(ast, cap, right, resultp);
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool method_access(ast_t* ast, ast_t* method)
{
  AST_GET_CHILDREN(method, cap, id, typeparams, params, result, can_error,
    body);

  switch(ast_id(method))
  {
    case TK_NEW:
    {
      AST_GET_CHILDREN(ast, left, right);
      ast_t* type = ast_type(left);

      if(is_typecheck_error(type))
        return false;

      if(!constructor_type(ast, ast_id(cap), type, &result))
        return false;
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

  ast_settype(ast, type_for_fun(method));

  if(ast_id(can_error) == TK_QUESTION)
    ast_seterror(ast);

  return is_method_called(ast);
}

static bool package_access(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  // Left is a packageref, right is an id.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  assert(ast_id(left) == TK_PACKAGEREF);
  assert(ast_id(right) == TK_ID);

  // Must be a type in a package.
  const char* package_name = ast_name(ast_child(left));
  ast_t* package = ast_get(left, package_name, NULL);

  if(package == NULL)
  {
    ast_error(right, "can't access package '%s'", package_name);
    return false;
  }

  assert(ast_id(package) == TK_PACKAGE);
  const char* type_name = ast_name(right);
  ast_t* type = ast_get(package, type_name, NULL);

  if(type == NULL)
  {
    ast_error(right, "can't find type '%s' in package '%s'",
      type_name, package_name);
    return false;
  }

  ast_settype(ast, type_sugar(ast, package_name, type_name));
  ast_setid(ast, TK_TYPEREF);

  return expr_typeref(opt, astp);
}

static bool type_access(pass_opt_t* opt, ast_t** astp)
{
  ast_t* ast = *astp;

  // Left is a typeref, right is an id.
  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);
  ast_t* type = ast_type(left);

  if(is_typecheck_error(type))
    return false;

  assert(ast_id(left) == TK_TYPEREF);
  assert(ast_id(right) == TK_ID);

  ast_t* find = lookup(opt, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
      ast_error(right, "can't look up a typeparam on a type");
      ret = false;
      break;

    case TK_NEW:
      ret = method_access(ast, find);
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_BE:
    case TK_FUN:
    {
      // Make this a lookup on a default constructed object.
      ast_free_unattached(find);

      if(!strcmp(ast_name(right), "create"))
      {
        ast_error(right, "create is not a constructor on this type");
        return false;
      }

      ast_t* dot = ast_from(ast, TK_DOT);
      ast_add(dot, ast_from_string(ast, "create"));
      ast_swap(left, dot);
      ast_add(dot, left);

      ast_t* call = ast_from(ast, TK_CALL);
      ast_swap(dot, call);
      ast_add(call, dot); // the LHS goes at the end, not the beginning
      ast_add(call, ast_from(ast, TK_NONE)); // named
      ast_add(call, ast_from(ast, TK_NONE)); // positional

      if(!expr_dot(opt, &dot))
        return false;

      if(!expr_call(opt, &call))
        return false;

      return expr_dot(opt, astp);
    }

    default:
      assert(0);
      ret = false;
      break;
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

  if(is_typecheck_error(type))
    return false;

  // Change the lookup name to an integer index.
  if(!make_tuple_index(&right))
  {
    ast_error(right,
      "lookup on a tuple must take the form _X, where X is an integer");
    return false;
  }

  // Make sure our index is in bounds.  make_tuple_index automatically shifts
  // from one indexed to zero, so we have to use -1 and >= for our comparisons.
  size_t right_idx = (size_t)ast_int(right);
  size_t tuple_size = ast_childcount(type);
  if (right_idx == (size_t)-1)
  {
    ast_error(right, "tuples are one indexed not zero indexed.  Did you mean _1?");
    return false;
  }
  else if (right_idx >= tuple_size)
  {
    ast_error(right, "tuple index %ld is out of valid range.  "
        "Valid range is [%ld, %ld]", right_idx, (size_t)1, tuple_size);
    return false;
  }

  type = ast_childidx(type, right_idx);
  assert(type != NULL);

  ast_setid(ast, TK_FLETREF);
  ast_settype(ast, type);
  ast_inheritflags(ast);
  return true;
}

static bool member_access(pass_opt_t* opt, ast_t* ast, bool partial)
{
  // Left is a postfix expression, right is an id.
  AST_GET_CHILDREN(ast, left, right);
  assert(ast_id(right) == TK_ID);
  ast_t* type = ast_type(left);

  if(is_typecheck_error(type))
    return false;

  ast_t* find = lookup(opt, ast, type, ast_name(right));

  if(find == NULL)
    return false;

  bool ret = true;

  switch(ast_id(find))
  {
    case TK_TYPEPARAM:
      ast_error(right, "can't look up a typeparam on an expression");
      ret = false;
      break;

    case TK_FVAR:
      if(!expr_fieldref(opt, ast, find, TK_FVARREF))
        return false;
      break;

    case TK_FLET:
      if(!expr_fieldref(opt, ast, find, TK_FLETREF))
        return false;
      break;

    case TK_EMBED:
      if(!expr_fieldref(opt, ast, find, TK_EMBEDREF))
        return false;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      ret = method_access(ast, find);
      break;

    default:
      assert(0);
      ret = false;
      break;
  }

  ast_free_unattached(find);

  if(!partial)
    ast_inheritflags(ast);

  return ret;
}

bool expr_qualify(pass_opt_t* opt, ast_t** astp)
{
  // Left is a postfix expression, right is a typeargs.
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, left, right);
  ast_t* type = ast_type(left);
  assert(ast_id(right) == TK_TYPEARGS);

  if(is_typecheck_error(type))
    return false;

  switch(ast_id(left))
  {
    case TK_TYPEREF:
    {
      // Qualify the type.
      assert(ast_id(type) == TK_NOMINAL);

      // If the type isn't polymorphic or the type is already qualified,
      // sugar .apply().
      ast_t* def = names_def(type);
      ast_t* typeparams = ast_childidx(def, 1);

      if((ast_id(typeparams) == TK_NONE) ||
        (ast_id(ast_childidx(type, 2)) != TK_NONE))
      {
        if(!expr_nominal(opt, &type))
          return false;

        break;
      }

      type = ast_dup(type);
      ast_t* typeargs = ast_childidx(type, 2);
      ast_replace(&typeargs, right);
      ast_settype(ast, type);
      ast_setid(ast, TK_TYPEREF);

      return expr_typeref(opt, astp);
    }

    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_NEWAPP:
    case TK_BEAPP:
    case TK_FUNAPP:
    {
      // Qualify the function.
      assert(ast_id(type) == TK_FUNTYPE);
      ast_t* typeparams = ast_childidx(type, 1);

      if(!check_constraints(left, typeparams, right, true))
        return false;

      type = reify(left, type, typeparams, right);
      typeparams = ast_childidx(type, 1);
      ast_replace(&typeparams, ast_from(typeparams, TK_NONE));

      ast_settype(ast, type);
      ast_setid(ast, ast_id(left));
      ast_inheritflags(ast);
      return true;
    }

    default: {}
  }

  // Sugar .apply()
  ast_t* dot = ast_from(left, TK_DOT);
  ast_add(dot, ast_from_string(left, "apply"));
  ast_swap(left, dot);
  ast_add(dot, left);

  if(!expr_dot(opt, &dot))
    return false;

  return expr_qualify(opt, astp);
}

static bool dot_or_tilde(pass_opt_t* opt, ast_t** astp, bool partial)
{
  ast_t* ast = *astp;

  // Left is a postfix expression, right is an id.
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_PACKAGEREF:
      return package_access(opt, astp);

    case TK_TYPEREF:
      return type_access(opt, astp);

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

  return member_access(opt, ast, partial);
}

bool expr_dot(pass_opt_t* opt, ast_t** astp)
{
  return dot_or_tilde(opt, astp, false);
}

bool expr_tilde(pass_opt_t* opt, ast_t** astp)
{
  if(!dot_or_tilde(opt, astp, true))
    return false;

  ast_t* ast = *astp;

  if(ast_id(ast) == TK_TILDE && ast_type(ast) != NULL &&
    ast_id(ast_type(ast)) == TK_OPERATORLITERAL)
  {
    ast_error(ast, "can't do partial application on a literal number");
    return false;
  }

  switch(ast_id(ast))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
      ast_setid(ast, TK_NEWAPP);
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
    case TK_EMBEDREF:
      ast_error(ast, "can't do partial application of a field");
      return false;

    default: {}
  }

  assert(0);
  return false;
}
