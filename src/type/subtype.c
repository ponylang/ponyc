#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

static bool is_cap_sub_cap(ast_t* sub, ast_t* super)
{
  switch(ast_id(sub))
  {
    case TK_NONE:
      switch(ast_id(super))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
        case TK_REF:
        case TK_BOX:
        case TK_TAG:
          return false;

        case TK_NONE:
          return true;

        default: {}
      }
      break;

    case TK_ISO:
      switch(ast_id(super))
      {
        case TK_NONE:
          return false;

        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
        case TK_REF:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_TRN:
      switch(ast_id(super))
      {
        case TK_NONE:
        case TK_ISO:
          return false;

        case TK_TRN:
        case TK_VAL:
        case TK_REF:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_REF:
      switch(ast_id(super))
      {
        case TK_NONE:
        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
          return false;

        case TK_REF:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_VAL:
      switch(ast_id(super))
      {
        case TK_NONE:
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
          return false;

        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_BOX:
      switch(ast_id(super))
      {
        case TK_NONE:
        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
        case TK_REF:
          return false;

        case TK_BOX:
        case TK_TAG:
          return true;

        default: {}
      }
      break;

    case TK_TAG:
      switch(ast_id(super))
      {
        case TK_NONE:
        case TK_ISO:
        case TK_TRN:
        case TK_VAL:
        case TK_REF:
        case TK_BOX:
          return false;

        case TK_TAG:
          return true;

        default: {}
      }
      break;

    default:{}
  }

  assert(0);
  return false;
}

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

static bool is_eq_typeargs(ast_t* a, ast_t* b)
{
  assert(ast_id(a) == TK_NOMINAL);
  assert(ast_id(b) == TK_NOMINAL);

  // check typeargs are the same
  ast_t* a_arg = ast_child(ast_childidx(a, 2));
  ast_t* b_arg = ast_child(ast_childidx(b, 2));

  while((a_arg != NULL) && (b_arg != NULL))
  {
    if(!is_eqtype(a_arg, b_arg))
      return false;

    a_arg = ast_sibling(a_arg);
    b_arg = ast_sibling(b_arg);
  }

  // make sure we had the same number of typeargs
  return (a_arg == NULL) && (b_arg == NULL);
}

static bool is_fun_sub_fun(ast_t* sub, ast_t* super)
{
  // must be the same type of function
  if(ast_id(sub) != ast_id(super))
    return false;

  // contravariant receiver
  if(!is_cap_sub_cap(ast_child(super), ast_child(sub)))
    return false;

  ast_t* sub_params = ast_childidx(sub, 3);
  ast_t* sub_result = ast_sibling(sub_params);
  ast_t* sub_throws = ast_sibling(sub_result);

  ast_t* super_params = ast_childidx(super, 3);
  ast_t* super_result = ast_sibling(super_params);
  ast_t* super_throws = ast_sibling(super_result);

  // TODO: reify with our own constraints?

  // contravariant parameters
  ast_t* sub_param = ast_child(sub_params);
  ast_t* super_param = ast_child(super_params);

  while((sub_param != NULL) && (super_param != NULL))
  {
    // extract the type if this is a parameter
    // otherwise, this is already a type
    ast_t* sub_type = (ast_id(sub_param) == TK_PARAM) ?
      ast_childidx(sub_param, 1) : sub_param;

    ast_t* super_type = (ast_id(super_param) == TK_PARAM) ?
      ast_childidx(super_param, 1) : super_param;

    assert(ast_id(sub_type) == TK_TYPEDEF);
    assert(ast_id(super_type) == TK_TYPEDEF);

    if(!is_subtype(super_type, sub_type))
      return false;

    sub_param = ast_sibling(sub_param);
    super_param = ast_sibling(super_param);
  }

  if((sub_param != NULL) || (super_param != NULL))
    return false;

  // covariant results
  if(!is_subtype(sub_result, super_result))
    return false;

  // covariant throws
  if(!is_throws_sub_throws(sub_throws, super_throws))
    return false;

  return true;
}

static bool is_structural_sub_fun(ast_t* sub, ast_t* fun)
{
  // must have some function that is a subtype of fun
  ast_t* sub_fun = ast_child(sub);

  while(sub_fun != NULL)
  {
    if(is_fun_sub_fun(sub_fun, fun))
      return true;

    sub_fun = ast_sibling(sub_fun);
  }

  return false;
}

static bool is_structural_sub_structural(ast_t* sub, ast_t* super)
{
  // must be a subtype of every function in super
  ast_t* fun = ast_child(super);

  while(fun != NULL)
  {
    if(!is_structural_sub_fun(sub, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_member_sub_fun(ast_t* member, ast_t* typeparams,
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
      bool is_sub = is_fun_sub_fun(r_fun, fun);

      if(r_fun != member)
        ast_free(r_fun);

      return is_sub;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_type_sub_fun(ast_t* def, ast_t* typeargs, ast_t* fun)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if(is_member_sub_fun(member, typeparams, typeargs, fun))
      return true;

    member = ast_sibling(member);
  }

  return false;
}

static bool is_nominal_sub_structural(ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_STRUCTURAL);

  ast_t* def = nominal_def(sub, sub);

  if(def == NULL)
    return false;

  // must be a subtype of every function in super
  ast_t* typeargs = ast_childidx(sub, 2);
  ast_t* fun = ast_child(super);

  while(fun != NULL)
  {
    if(!is_type_sub_fun(def, typeargs, fun))
      return false;

    fun = ast_sibling(fun);
  }

  return true;
}

static bool is_nominal_sub_nominal(ast_t* sub, ast_t* super)
{
  assert(ast_id(sub) == TK_NOMINAL);
  assert(ast_id(super) == TK_NOMINAL);

  ast_t* sub_def = nominal_def(sub, sub);
  ast_t* super_def = nominal_def(super, super);

  // check we found a definition for both
  if((sub_def == NULL) || (super_def == NULL))
    return false;

  // if we are the same nominal type, our typeargs must be the same
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  // our def might be a TK_TYPEPARAM
  if(ast_id(sub_def) == TK_TYPEPARAM)
  {
    // check if our constraint is a subtype of super
    ast_t* constraint = ast_childidx(sub_def, 1);

    if(ast_id(constraint) == TK_NONE)
      return false;

    return is_subtype(constraint, ast_parent(super));
  }

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // check traits, depth first
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    assert(ast_id(trait) == TK_TYPEDEF);
    ast_t* r_trait = reify(trait, typeparams, typeargs);
    bool is_sub = is_subtype(r_trait, ast_parent(super));

    if(r_trait != trait)
      ast_free(r_trait);

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  return false;
}

static bool is_sub_union(ast_t* sub, ast_t* super)
{
  ast_t* left = ast_child(super);
  ast_t* right = ast_sibling(left);
  return is_subtype(sub, left) || is_subtype(sub, right);
}

static bool is_sub_isect(ast_t* sub, ast_t* super)
{
  ast_t* left = ast_child(super);
  ast_t* right = ast_sibling(left);
  return is_subtype(sub, left) && is_subtype(sub, right);
}

static bool is_nominal_eq_nominal(ast_t* a, ast_t* b)
{
  ast_t* a_def = nominal_def(a, a);
  ast_t* b_def = nominal_def(b, b);

  // check we found a definition for both
  if((a_def == NULL) || (b_def == NULL))
    return false;

  // must be the same nominal type
  if(a_def != b_def)
    return false;

  // must have the same typeargs
  return is_eq_typeargs(a, b);
}

static bool is_typedef_sub(ast_t* type, ast_t* super)
{
  assert(ast_id(type) == TK_TYPEDEF);

  if(!is_subtype(ast_child(type), super))
    return false;

  // TODO: check cap and ephemeral
  return true;
}

static bool is_typedef_eq(ast_t* type, ast_t* b)
{
  assert(ast_id(type) == TK_TYPEDEF);

  if(!is_eqtype(ast_child(type), b))
    return false;

  // TODO: check cap and ephemeral
  return true;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  // if the supertype is a typeparam, check against the constraint
  if(ast_id(super) == TK_TYPEPARAM)
    return is_subtype(sub, ast_childidx(super, 1));

  // unwrap the subtype
  switch(ast_id(sub))
  {
    case TK_TYPEDEF:
      return is_typedef_sub(sub, super);

    case TK_TYPEPARAM:
      return is_subtype(ast_childidx(sub, 1), super);

    case TK_UNIONTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) && is_subtype(right, super);
    }

    case TK_ISECTTYPE:
    {
      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      return is_subtype(left, super) || is_subtype(right, super);
    }

    default: {}
  }

  // unwrap the supertype
  if(ast_id(super) == TK_TYPEDEF)
    super = ast_child(super);

  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
      return is_sub_union(sub, super);

    case TK_ISECTTYPE:
      return is_sub_isect(sub, super);

    default: {}
  }

  switch(ast_id(sub))
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return is_fun_sub_fun(sub, super);

    case TK_TUPLETYPE:
    {
      if(ast_id(super) != TK_TUPLETYPE)
        return false;

      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      ast_t* super_left = ast_child(super);
      ast_t* super_right = ast_sibling(super_left);
      return is_subtype(left, super_left) && is_subtype(right, super_right);
    }

    case TK_NOMINAL:
    {
      switch(ast_id(super))
      {
        case TK_NOMINAL:
          return is_nominal_sub_nominal(sub, super);

        case TK_STRUCTURAL:
          return is_nominal_sub_structural(sub, super);

        default: {}
      }

      return false;
    }

    case TK_STRUCTURAL:
    {
      if(ast_id(super) != TK_STRUCTURAL)
        return false;

      return is_structural_sub_structural(sub, super);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  if(ast_id(b) == TK_TYPEPARAM)
    return is_eqtype(a, ast_childidx(b, 1));

  switch(ast_id(a))
  {
    case TK_TYPEDEF:
      return is_typedef_eq(a, b);

    case TK_TYPEPARAM:
      return is_eqtype(ast_childidx(a, 1), b);

    default: {}
  }

  if(ast_id(b) == TK_TYPEDEF)
    b = ast_child(b);

  switch(ast_id(a))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_STRUCTURAL:
      return is_subtype(a, b) && is_subtype(b, a);

    case TK_TUPLETYPE:
    {
      if(ast_id(b) != TK_TUPLETYPE)
        return false;

      ast_t* a_left = ast_child(a);
      ast_t* a_right = ast_sibling(a_left);
      ast_t* b_left = ast_child(b);
      ast_t* b_right = ast_sibling(b_left);
      return is_eqtype(a_left, b_left) && is_eqtype(a_right, b_right);
    }

    case TK_NOMINAL:
    {
      if(ast_id(b) != TK_NOMINAL)
        return false;

      return is_nominal_eq_nominal(a, b);
    }

    default: {}
  }

  assert(0);
  return false;
}
