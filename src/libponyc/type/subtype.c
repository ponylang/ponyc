#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "assemble.h"
#include "viewpoint.h"
#include "../ast/token.h"
#include "../pass/names.h"
#include "../ds/stringtab.h"
#include <assert.h>

static bool is_throws_sub_throws(ast_t* sub, ast_t* super)
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

static bool check_cap_and_ephemeral(ast_t* sub, ast_t* super)
{
  int sub_index = ast_id(sub) == TK_NOMINAL ? 4 : 2;
  int super_index = ast_id(super) == TK_NOMINAL ? 4 : 2;

  ast_t* sub_ephemeral = ast_childidx(sub, sub_index);
  ast_t* super_ephemeral = ast_childidx(super, super_index);

  token_id sub_tcap = cap_for_type(sub);
  token_id super_tcap = cap_for_type(super);

  // ignore ephemerality if it isn't iso/trn and can't be recovered to iso/trn
  if((ast_id(super_ephemeral) == TK_HAT) &&
    (super_tcap < TK_VAL) &&
    (ast_id(sub_ephemeral) != TK_HAT))
    return false;

  return is_cap_sub_cap(sub_tcap, super_tcap);
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
    // extract the type if this is a parameter
    // otherwise, this is already a type
    ast_t* sub_type = (ast_id(sub_param) == TK_PARAM) ?
      ast_childidx(sub_param, 1) : sub_param;

    ast_t* super_type = (ast_id(super_param) == TK_PARAM) ?
      ast_childidx(super_param, 1) : super_param;

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
  ast_t* members = ast_child(sub);
  ast_t* sub_fun = ast_child(members);

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
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

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
      ast_free_unattached(r_fun);
      return is_sub;
    }

    default: {}
  }

  assert(0);
  return false;
}

static bool is_type_sub_fun(ast_t* def, ast_t* typeargs,
  ast_t* fun)
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

  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* def = (ast_t*)ast_data(sub);
  assert(def != NULL);

  // must be a subtype of every function in super
  ast_t* typeargs = ast_childidx(sub, 2);
  ast_t* members = ast_child(super);
  ast_t* fun = ast_child(members);

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

  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);
  assert(sub_def != NULL);
  assert(super_def != NULL);

  // if we are the same nominal type, our typeargs must be the same
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  // get our typeparams and typeargs
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // check traits, depth first
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // reify the trait
    ast_t* r_trait = reify(trait, typeparams, typeargs);

    // use the cap and ephemerality of the subtype
    reify_cap_and_ephemeral(sub, &r_trait);
    bool is_sub = is_subtype(r_trait, super);
    ast_free_unattached(r_trait);

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  return false;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  // check if the subtype is a union, isect, type param, type alias or viewpoint
  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
    {
      // all elements of the union must be subtypes
      ast_t* child = ast_child(sub);

      while(child != NULL)
      {
        if(!is_subtype(child, super))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ISECTTYPE:
    {
      // one element of the intersection must be a subtype
      ast_t* child = ast_child(sub);

      while(child != NULL)
      {
        if(is_subtype(child, super))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_TYPEPARAMREF:
    {
      // check if our constraint is a subtype of super
      ast_t* def = (ast_t*)ast_data(sub);
      ast_t* constraint = ast_childidx(def, 1);

      // if it isn't, keep trying
      if(ast_id(constraint) != TK_NONE)
      {
        // use the cap and ephemerality of the typeparam
        reify_cap_and_ephemeral(sub, &constraint);
        bool ok = is_subtype(constraint, super);
        ast_free_unattached(constraint);

        if(ok)
          return true;
      }
      break;
    }

    case TK_ARROW:
    {
      // an arrow type can be a subtype if its upper bounds is a subtype
      ast_t* upper = viewpoint_upper(sub);
      bool ok = is_subtype(upper, super);
      ast_free_unattached(upper);

      if(ok)
        return true;
      break;
    }

    default: {}
  }

  // check if the supertype is a union, isect, type alias or viewpoint
  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
    {
      // must be a subtype of one element of the union
      ast_t* child = ast_child(super);

      while(child != NULL)
      {
        if(is_subtype(sub, child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      // must be a subtype of all elements of the intersection
      ast_t* child = ast_child(super);

      while(child != NULL)
      {
        if(!is_subtype(sub, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      // an arrow type can be a supertype if its lower bounds is a supertype
      ast_t* lower = viewpoint_lower(super);
      bool ok = is_subtype(sub, lower);
      ast_free_unattached(lower);

      if(ok)
        return true;
      break;
    }

    case TK_TYPEPARAMREF:
    {
      // we also can be a subtype of a typeparam if its constraint is concrete
      ast_t* def = (ast_t*)ast_data(super);
      ast_t* constraint = ast_childidx(def, 1);

      if(ast_id(constraint) == TK_NOMINAL)
      {
        ast_t* constraint_def = (ast_t*)ast_data(constraint);

        switch(ast_id(constraint_def))
        {
          case TK_DATA:
          case TK_CLASS:
          case TK_ACTOR:
            if(is_eqtype(sub, constraint))
              return true;
            break;

          default: {}
        }
      }
      break;
    }

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
      switch(ast_id(super))
      {
        case TK_STRUCTURAL:
        {
          // a tuple is a subtype of an empty structural type
          ast_t* members = ast_child(super);
          ast_t* member = ast_child(members);
          return member == NULL;
        }

        case TK_TUPLETYPE:
        {
          // elements must be pairwise subtypes
          ast_t* sub_child = ast_child(sub);
          ast_t* super_child = ast_child(super);

          while((sub_child != NULL) && (super_child != NULL))
          {
            if(!is_subtype(sub_child, super_child))
              return false;

            sub_child = ast_sibling(sub_child);
            super_child = ast_sibling(super_child);
          }

          return (sub_child == NULL) && (super_child == NULL);
        }

        default: {}
      }

      return false;
    }

    case TK_NOMINAL:
    {
      // check for numeric literals and special case them
      if(is_uintliteral(sub))
      {
        // an unsigned integer literal is a subtype of any arithmetic type
        if(is_intliteral(super) ||
          is_floatliteral(super) ||
          is_arithmetic(super)
          )
          return true;
      } else if(is_sintliteral(sub)) {
        // a signed integer literal is a subtype of any signed type
        if(is_sintliteral(super) ||
          is_floatliteral(super) ||
          is_signed(super)
          )
          return true;
      } else if(is_floatliteral(sub)) {
        // a float literal is a subtype of any float type
        if(is_floatliteral(super) ||
          (!is_intliteral(super) && is_float(super))
          )
          return true;
      }

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

    case TK_TYPEPARAMREF:
    {
      if(ast_id(super) != TK_TYPEPARAMREF)
        return false;

      if(!check_cap_and_ephemeral(sub, super))
        return false;

      return ast_data(sub) == ast_data(super);
    }

    case TK_ARROW:
    {
      if(ast_id(super) != TK_ARROW)
        return false;

      ast_t* left = ast_child(sub);
      ast_t* right = ast_sibling(left);
      ast_t* super_left = ast_child(super);
      ast_t* super_right = ast_sibling(super_left);

      return is_eqtype(left, super_left) && is_subtype(right, super_right);
    }

    case TK_THISTYPE:
      return ast_id(super) == TK_THISTYPE;

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  return is_subtype(a, b) && is_subtype(b, a);
}

bool is_literal(ast_t* type, const char* name)
{
  if(type == NULL)
    return false;

  if(ast_id(type) != TK_NOMINAL)
    return false;

  // don't have to check the package, since literals are all builtins
  return ast_name(ast_childidx(type, 1)) == stringtab(name);
}

bool is_builtin(ast_t* type, const char* name)
{
  if(type == NULL)
    return false;

  ast_t* builtin = type_builtin(type, name);
  bool ok = is_subtype(type, builtin);
  ast_free_unattached(builtin);

  return ok;
}

bool is_bool(ast_t* type)
{
  return is_builtin(type, "Bool");
}

bool is_sintliteral(ast_t* type)
{
  return is_literal(type, "SIntLiteral");
}

bool is_uintliteral(ast_t* type)
{
  return is_literal(type, "UIntLiteral");
}

bool is_intliteral(ast_t* type)
{
  return is_sintliteral(type) || is_uintliteral(type);
}

bool is_floatliteral(ast_t* type)
{
  return is_literal(type, "FloatLiteral");
}

bool is_arithmetic(ast_t* type)
{
  return is_singletype(type) && is_builtin(type, "Arithmetic");
}

bool is_integer(ast_t* type)
{
  return is_singletype(type) && is_builtin(type, "Integer");
}

bool is_float(ast_t* type)
{
  return is_singletype(type) && is_builtin(type, "Float");
}

bool is_signed(ast_t* type)
{
  return is_singletype(type) &&
    (is_builtin(type, "SInt") || is_builtin(type, "Float"));
}

bool is_singletype(ast_t* type)
{
  if(type == NULL)
    return false;

  switch(ast_id(type))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return true;

    case TK_ARROW:
      return is_singletype(ast_childidx(type, 1));

    default: {}
  }

  return false;
}

bool is_math_compatible(ast_t* a, ast_t* b)
{
  if(!is_singletype(a) || !is_singletype(b) || !is_arithmetic(a))
    return false;

  if(is_intliteral(a))
    return is_arithmetic(b);

  if(is_floatliteral(a))
    return is_float(b);

  if(is_intliteral(b))
    return is_arithmetic(a);

  if(is_floatliteral(b))
    return is_float(a);

  return is_eqtype(a, b);
}

bool is_concrete(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!is_concrete(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(is_concrete(child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_TUPLETYPE:
      // TODO: Not correct. (T1, T2) is ID compatible with (T3, T4). Need to
      // check tuples pair-wise.
      return true;

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);
      return ast_id(def) != TK_TRAIT;
    }

    case TK_STRUCTURAL:
      return false;

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      ast_t* constraint = ast_childidx(def, 1);
      return is_concrete(constraint);
    }

    case TK_ARROW:
      return is_concrete(ast_childidx(type, 1));

    default: {}
  }

  assert(0);
  return false;
}

bool is_id_compatible(ast_t* a, ast_t* b)
{
  // subtype without capability or ephemeral mattering
  a = viewpoint_tag(a);
  b = viewpoint_tag(b);
  bool ok;

  if(is_concrete(a) || is_concrete(b))
  {
    // if either side is concrete, then one side must be a subtype of the other
    ok = is_subtype(a, b) || is_subtype(b, a);
  } else {
    // if neither side is concrete, there could be a concrete type that was both
    ok = true;
  }

  ast_free_unattached(a);
  ast_free_unattached(b);
  return ok;
}

bool is_match_compatible(ast_t* expr_type, ast_t* match_type)
{
  // if it's a subtype, this will always match
  if(is_subtype(expr_type, match_type))
  {
    ast_error(match_type, "expression is always the match type");
    return false;
  }

  // if they aren't id compatible, this can never match
  if(!is_id_compatible(expr_type, match_type))
  {
    ast_error(match_type, "expression can never be the match type");
    return false;
  }

  // TODO: match type cannot contain any non-empty structural types
  // could allow them, but for all structural types in pattern matches
  // we would have to determine if every concrete type is a subtype at compile
  // time and store that
  return true;
}
