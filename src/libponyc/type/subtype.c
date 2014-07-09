#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "nominal.h"
#include "../ds/stringtab.h"
#include <assert.h>

bool is_throws_sub_throws(ast_t* sub, ast_t* super)
{
  assert(
    (ast_id(sub) == TK_NONE) ||
    (ast_id(sub) == TK_QUESTION)
    );
  assert(
    (ast_id(super) == TK_NONE) ||
    (ast_id(super) == TK_QUESTION)
    );

  switch(ast_id(sub))
  {
    case TK_NONE:
      return true;

    case TK_QUESTION:
      return ast_id(super) == TK_QUESTION;

    default: {}
  }

  assert(0);
  return false;
}

static bool is_eq_typeargs(ast_t* scope, ast_t* a, ast_t* b)
{
  assert(ast_id(a) == TK_NOMINAL);
  assert(ast_id(b) == TK_NOMINAL);

  // check typeargs are the same
  ast_t* a_arg = ast_child(ast_childidx(a, 2));
  ast_t* b_arg = ast_child(ast_childidx(b, 2));

  while((a_arg != NULL) && (b_arg != NULL))
  {
    if(!is_eqtype(scope, a_arg, b_arg))
      return false;

    a_arg = ast_sibling(a_arg);
    b_arg = ast_sibling(b_arg);
  }

  // make sure we had the same number of typeargs
  return (a_arg == NULL) && (b_arg == NULL);
}

static bool is_fun_sub_fun(ast_t* scope, ast_t* sub, ast_t* super)
{
  // must be the same type of function
  // TODO: could relax this
  if(ast_id(sub) != ast_id(super))
    return false;

  ast_t* sub_params = ast_childidx(sub, 3);
  ast_t* sub_result = ast_sibling(sub_params);
  ast_t* sub_throws = ast_sibling(sub_result);

  ast_t* super_params = ast_childidx(super, 3);
  ast_t* super_result = ast_sibling(super_params);
  ast_t* super_throws = ast_sibling(super_result);

  // TODO: reify with our own constraints?

  // contravariant receiver
  if(!is_cap_sub_cap(cap_for_fun(super), cap_for_fun(sub)))
    return false;

  // contravariant parameters
  ast_t* sub_param = ast_child(sub_params);
  ast_t* super_param = ast_child(super_params);

  while((sub_param != NULL) && (super_param != NULL))
  {
    // TODO: this is wrong. parameter types might not exist, as they may be
    // inferred from initialisers.

    // extract the type if this is a parameter
    // otherwise, this is already a type
    ast_t* sub_type = (ast_id(sub_param) == TK_PARAM) ?
      ast_childidx(sub_param, 1) : sub_param;

    ast_t* super_type = (ast_id(super_param) == TK_PARAM) ?
      ast_childidx(super_param, 1) : super_param;

    if(!is_subtype(scope, super_type, sub_type))
      return false;

    sub_param = ast_sibling(sub_param);
    super_param = ast_sibling(super_param);
  }

  if((sub_param != NULL) || (super_param != NULL))
    return false;

  // covariant results
  if(!is_subtype(scope, sub_result, super_result))
    return false;

  // covariant throws
  if(!is_throws_sub_throws(sub_throws, super_throws))
    return false;

  return true;
}

static bool is_structural_sub_fun(ast_t* scope, ast_t* sub, ast_t* fun)
{
  // must have some function that is a subtype of fun
  ast_t* members = ast_child(sub);
  ast_t* sub_fun = ast_child(members);

  while(sub_fun != NULL)
  {
    if(is_fun_sub_fun(scope, sub_fun, fun))
      return true;

    sub_fun = ast_sibling(sub_fun);
  }

  return false;
}

static bool is_structural_sub_structural(ast_t* scope, ast_t* sub, ast_t* super)
{
  // must be a subtype of every function in super
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

  while(fun != NULL)
  {
    if(!is_structural_sub_fun(scope, sub, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_member_sub_fun(ast_t* scope, ast_t* member, ast_t* typeparams,
  ast_t* typeargs, ast_t* fun)
{
  switch(ast_id(member))
  {
    case TK_FVAR:
    case TK_FLET:
      return false;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    {
      ast_t* r_fun = reify(member, typeparams, typeargs);
      bool is_sub = is_fun_sub_fun(scope, r_fun, fun);
      ast_free_unattached(r_fun);
      return is_sub;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_type_sub_fun(ast_t* scope, ast_t* def, ast_t* typeargs,
  ast_t* fun)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if(is_member_sub_fun(scope, member, typeparams, typeargs, fun))
      return true;

    member = ast_sibling(member);
  }

  return false;
}

static bool is_nominal_sub_structural(ast_t* scope, ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_STRUCTURAL);

  ast_t* def = nominal_def(scope, sub);
  assert(def != NULL);

  // TODO: cap and ephemeral

  // must be a subtype of every function in super
  ast_t* typeargs = ast_childidx(sub, 2);
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

  while(fun != NULL)
  {
    if(!is_type_sub_fun(scope, def, typeargs, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_nominal_sub_nominal(ast_t* scope, ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_NOMINAL);

  ast_t* sub_def = nominal_def(scope, sub);
  ast_t* super_def = nominal_def(scope, super);
  assert(sub_def != NULL);
  assert(super_def != NULL);

  // TODO: cap and ephemeral

  // if we are the same nominal type, our typeargs must be the same
  if(sub_def == super_def)
    return is_eq_typeargs(scope, sub, super);

  // typeparams have no traits to check
  if(ast_id(sub_def) == TK_TYPEPARAM)
    return false;

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // check traits, depth first
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // TODO: use our cap and ephemeral
    ast_t* r_trait = reify(trait, typeparams, typeargs);
    bool is_sub = is_subtype(scope, r_trait, super);
    ast_free_unattached(r_trait);

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  if(ast_id(super_def) == TK_TYPEPARAM)
  {
    // we can also be a subtype of a typeparam if its constraint is concrete
    ast_t* constraint = ast_childidx(super_def, 1);

    if(ast_id(constraint) == TK_NOMINAL)
    {
      ast_t* constraint_def = nominal_def(scope, constraint);
      assert(constraint_def != NULL);

      switch(ast_id(constraint_def))
      {
        case TK_CLASS:
        case TK_ACTOR:
          return is_eqtype(scope, sub, constraint);

        // TODO: what if it is a type alias to a concrete type?

        default: {}
      }
    }
  }

  return false;
}

static bool is_builtin(ast_t* ast, const char* name)
{
  if(ast_id(ast) != TK_NOMINAL)
    return false;

  ast_t* package = ast_child(ast);
  ast_t* typename = ast_sibling(package);

  return (ast_id(package) == TK_NONE) &&
    (ast_name(typename) == stringtab(name));
}

static ast_t* typealias(ast_t* ast, ast_t* def)
{
  ast_t* aliases = ast_childidx(def, 3);
  assert(ast_id(aliases) == TK_TYPES);

  ast_t* alias = ast_child(aliases);
  assert(ast_sibling(alias) == NULL);

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);

  // TODO: use our cap and ephemeral
  ast_t* r_alias = reify(alias, typeparams, typeargs);
  return r_alias;
}

bool is_subtype(ast_t* scope, ast_t* sub, ast_t* super)
{
  // check if the subtype is a union, isect, type param, type alias or viewpoint
  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, left, super) && is_subtype(scope, right, super);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, left, super) || is_subtype(scope, right, super);
    }

    case TK_NOMINAL:
    {
      ast_t* def = nominal_def(scope, sub);
      assert(def != NULL);

      switch(ast_id(def))
      {
        case TK_TYPEPARAM:
        {
          // check if our constraint is a subtype of super
          ast_t* constraint = ast_childidx(def, 1);

          // if it isn't, keep trying with our nominal type
          if((ast_id(constraint) != TK_NONE) &&
            is_subtype(scope, constraint, super))
            return true;
          break;
        }

        case TK_TYPE:
        {
          // we're a type alias
          ast_t* alias = typealias(sub, def);

          if(alias != sub)
          {
            bool ok = is_subtype(scope, alias, super);
            ast_free_unattached(alias);
            return ok;
          }
          break;
        }

        default: {}
      }
      break;
    }

    case TK_ARROW:
    {
      // TODO: actually do viewpoint adaptation
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, right, super);
    }

    default: {}
  }

  // check if the supertype is a union, isect, type alias or viewpoint
  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, sub, left) || is_subtype(scope, sub, right);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, sub, left) && is_subtype(scope, sub, right);
    }

    case TK_NOMINAL:
    {
      ast_t* def = nominal_def(scope, super);
      assert(def != NULL);

      switch(ast_id(def))
      {
        case TK_TYPE:
        {
          // we're a type alias
          ast_t* alias = typealias(super, def);

          if(alias != super)
          {
            bool ok = is_subtype(scope, sub, alias);
            ast_free_unattached(alias);
            return ok;
          }
          break;
        }

        default: {}
      }
      break;
    }

    case TK_ARROW:
    {
      // TODO: actually do viewpoint adaptation
      ast_t* left = ast_child(super);
      ast_t* right = ast_sibling(left);
      return is_subtype(scope, sub, right);
    }

    default: {}
  }

  switch(ast_id(sub))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return is_fun_sub_fun(scope, sub, super);

    case TK_TUPLETYPE:
    {
      if(ast_id(super) != TK_TUPLETYPE)
        return false;

      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      ast_t* super_left = ast_child(super);
      ast_t* super_right = ast_sibling(super_left);

      return is_subtype(scope, left, super_left) &&
        is_subtype(scope, right, super_right);
    }

    case TK_NOMINAL:
    {
      // check for numeric literals and special case them
      if(is_builtin(sub, "IntLiteral"))
      {
        if(is_builtin(super, "IntLiteral") ||
          is_builtin(super, "FloatLiteral")
          )
          return true;

        // an integer literal is a subtype of any arithmetic type
        ast_t* math = nominal_builtin(scope, "Arithmetic");
        bool ok = is_subtype(scope, super, math);
        ast_free(math);

        if(ok)
          return true;
      } else if(is_builtin(sub, "FloatLiteral")) {
        if(is_builtin(super, "FloatLiteral"))
          return true;

        // a float literal is a subtype of any float type
        ast_t* float_type = nominal_builtin(scope, "Float");
        bool ok = is_subtype(scope, super, float_type);
        ast_free(float_type);

        if(ok)
          return true;
      }

      switch(ast_id(super))
      {
        case TK_NOMINAL:
          return is_nominal_sub_nominal(scope, sub, super);

        case TK_STRUCTURAL:
          return is_nominal_sub_structural(scope, sub, super);

        default: {}
      }

      return false;
    }

    case TK_STRUCTURAL:
    {
      if(ast_id(super) != TK_STRUCTURAL)
        return false;

      return is_structural_sub_structural(scope, sub, super);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* scope, ast_t* a, ast_t* b)
{
  return is_subtype(scope, a, b) && is_subtype(scope, b, a);
}
