#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "alias.h"
#include "assemble.h"
#include "viewpoint.h"
#include "../ast/stringtab.h"
#include "../expr/literal.h"
#include <assert.h>

static bool is_isect_subtype(ast_t* sub, ast_t* super);

static bool is_literal(ast_t* type, const char* name)
{
  if(type == NULL)
    return false;

  if(ast_id(type) != TK_NOMINAL)
    return false;

  // Don't have to check the package, since literals are all builtins.
  return ast_name(ast_childidx(type, 1)) == stringtab(name);
}

static ast_t* fetch_cap(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return ast_childidx(type, 3);

    case TK_TYPEPARAMREF:
      return ast_childidx(type, 1);

    default: {}
  }

  assert(0);
  return NULL;
}

static bool check_cap_and_ephemeral(ast_t* sub, ast_t* super)
{
  ast_t* sub_cap = fetch_cap(sub);
  ast_t* sub_eph = ast_sibling(sub_cap);
  ast_t* super_cap = fetch_cap(super);
  ast_t* super_eph = ast_sibling(super_cap);

  token_id t_sub_cap = ast_id(sub_cap);
  token_id t_sub_eph = ast_id(sub_eph);
  token_id t_super_cap = ast_id(super_cap);
  token_id t_super_eph = ast_id(super_eph);
  token_id t_alt_cap = t_sub_cap;

  // Adjusted for borrowing.
  if(t_sub_eph == TK_BORROWED)
  {
    switch(t_alt_cap)
    {
      case TK_ISO: t_alt_cap = TK_TAG; break;
      case TK_TRN: t_alt_cap = TK_BOX; break;
      case TK_ANY_GENERIC: t_alt_cap = TK_TAG; break;
      default: {}
    }
  }

  switch(t_super_eph)
  {
    case TK_EPHEMERAL:
      // Sub must be ephemeral.
      if(t_sub_eph != TK_EPHEMERAL)
        return false;

      // Capability must be a sub-capability.
      return is_cap_sub_cap(t_sub_cap, t_super_cap);

    case TK_NONE:
      // Check the adjusted capability.
      return is_cap_sub_cap(t_alt_cap, t_super_cap);

    case TK_BORROWED:
      // Borrow a capability.
      if(t_sub_cap == t_super_cap)
        return true;

      // Or alias a capability.
      return is_cap_sub_cap(t_alt_cap, t_super_cap);

    default: {}
  }

  assert(0);
  return false;
}

static bool is_eq_typeargs(ast_t* a, ast_t* b)
{
  assert(ast_id(a) == TK_NOMINAL);
  assert(ast_id(b) == TK_NOMINAL);

  // Check typeargs are the same.
  ast_t* a_arg = ast_child(ast_childidx(a, 2));
  ast_t* b_arg = ast_child(ast_childidx(b, 2));

  while((a_arg != NULL) && (b_arg != NULL))
  {
    if(!is_eqtype(a_arg, b_arg))
      return false;

    a_arg = ast_sibling(a_arg);
    b_arg = ast_sibling(b_arg);
  }

  // Make sure we had the same number of typeargs.
  return (a_arg == NULL) && (b_arg == NULL);
}

static bool is_fun_sub_fun(ast_t* sub, ast_t* super)
{
  // Must be the same type of function.
  if(ast_id(sub) != ast_id(super))
    return false;

  AST_GET_CHILDREN(sub, sub_cap, sub_id, sub_typeparams, sub_params,
    sub_result, sub_throws);

  AST_GET_CHILDREN(super, super_cap, super_id, super_typeparams, super_params,
    super_result, super_throws);

  // Must have the same name.
  if(ast_name(sub_id) != ast_name(super_id))
    return false;

  switch(ast_id(sub))
  {
    case TK_NEW:
      // Covariant receiver.
      if(!is_cap_sub_cap(ast_id(sub_cap), ast_id(super_cap)))
        return false;

      // Covariant results.
      if(!is_subtype(sub_result, super_result))
        return false;
      break;

    case TK_FUN:
      // Contravariant receiver.
      if(!is_cap_sub_cap(ast_id(super_cap), ast_id(sub_cap)))
        return false;

      // Covariant results.
      if(!is_subtype(sub_result, super_result))
        return false;
      break;

    default: {}
  }

  // Contravariant type parameter constraints.
  ast_t* sub_typeparam = ast_child(sub_typeparams);
  ast_t* super_typeparam = ast_child(super_typeparams);

  while((sub_typeparam != NULL) && (super_typeparam != NULL))
  {
    ast_t* sub_constraint = ast_childidx(sub_typeparam, 1);
    ast_t* super_constraint = ast_childidx(super_typeparam, 1);

    if(!is_subtype(super_constraint, sub_constraint))
      return false;

    sub_typeparam = ast_sibling(sub_typeparam);
    super_typeparam = ast_sibling(super_typeparam);
  }

  if((sub_typeparam != NULL) || (super_typeparam != NULL))
    return false;

  // Contravariant parameters.
  ast_t* sub_param = ast_child(sub_params);
  ast_t* super_param = ast_child(super_params);

  while((sub_param != NULL) && (super_param != NULL))
  {
    ast_t* sub_type = ast_childidx(sub_param, 1);
    ast_t* super_type = ast_childidx(super_param, 1);

    // If either parameter type is a machine word, the other must be as well.
    if(is_machine_word(sub_type) && !is_machine_word(super_type))
      return false;

    if(is_machine_word(super_type) && !is_machine_word(sub_type))
      return false;

    // Contravariant: the super type must be a subtype of the sub type.
    if(!is_subtype(super_type, sub_type))
      return false;

    sub_param = ast_sibling(sub_param);
    super_param = ast_sibling(super_param);
  }

  if((sub_param != NULL) || (super_param != NULL))
    return false;

  // Covariant throws.
  if((ast_id(sub_throws) == TK_QUESTION) &&
    (ast_id(super_throws) != TK_QUESTION))
    return false;

  return true;
}

static bool is_nominal_sub_interface(ast_t* sub, ast_t* super)
{
  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  ast_t* sub_typeargs = ast_childidx(sub, 2);
  ast_t* super_typeargs = ast_childidx(super, 2);

  ast_t* sub_typeparams = ast_childidx(sub_def, 1);
  ast_t* super_typeparams = ast_childidx(super_def, 1);

  ast_t* super_members = ast_childidx(super_def, 4);
  ast_t* super_member = ast_child(super_members);

  while(super_member != NULL)
  {
    ast_t* super_member_id = ast_childidx(super_member, 1);

    sym_status_t s;
    ast_t* sub_member = ast_get(sub_def, ast_name(super_member_id), &s);

    if(sub_member == NULL)
      return false;

    ast_t* r_sub_member = reify(sub_member, sub_typeparams, sub_typeargs);
    ast_t* r_super_member = reify(super_member, super_typeparams,
      super_typeargs);

    bool ok = is_fun_sub_fun(r_sub_member, r_super_member);
    ast_free_unattached(r_sub_member);
    ast_free_unattached(r_super_member);

    if(!ok)
      return false;

    super_member = ast_sibling(super_member);
  }

  return true;
}

static bool is_nominal_sub_trait(ast_t* sub, ast_t* super)
{
  ast_t* sub_def = (ast_t*)ast_data(sub);

  // Get our typeparams and typeargs.
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // Check traits, depth first.
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // Reify the trait with our typeargs.
    ast_t* r_trait = reify(trait, typeparams, typeargs);

    // Use the cap and ephemerality of the subtype.
    reify_cap_and_ephemeral(sub, &r_trait);
    bool is_sub = is_subtype(r_trait, super);
    ast_free_unattached(r_trait);

    if(is_sub)
      return true;

    trait = ast_sibling(trait);
  }

  return false;
}

// Both sub and super are nominal types.
static bool is_nominal_sub_nominal(ast_t* sub, ast_t* super)
{
  if(!check_cap_and_ephemeral(sub, super))
    return false;

  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  // If we are the same nominal type, our typeargs must be the same.
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  switch(ast_id(super_def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      // If we aren't the same type, we can't be a subtype of a concrete type.
      return false;

    case TK_INTERFACE:
      // Check for a structural subtype.
      return is_nominal_sub_interface(sub, super);

    case TK_TRAIT:
      // Check for a nominal subtype.
      return is_nominal_sub_trait(sub, super);

    default: {}
  }

  return false;
}

// The subtype is a nominal, the super type is a typeparam.
static bool is_nominal_sub_typeparam(ast_t* sub, ast_t* super)
{
  // Must be a subtype of the lower bounds of the constraint.
  ast_t* def = (ast_t*)ast_data(super);
  ast_t* constraint = ast_childidx(def, 1);

  if(ast_id(constraint) == TK_NOMINAL)
  {
    ast_t* constraint_def = (ast_t*)ast_data(constraint);

    switch(ast_id(constraint_def))
    {
      case TK_PRIMITIVE:
      case TK_CLASS:
      case TK_ACTOR:
      {
        // Constraint must be modified with super ephemerality.
        AST_GET_CHILDREN(super, name, cap, eph);
        ast_t* r_constraint = set_cap_and_ephemeral(constraint, TK_NONE,
          ast_id(eph));

        // Must be a subtype of the constraint.
        bool ok = is_subtype(sub, constraint);
        ast_free_unattached(r_constraint);

        if(!ok)
          return false;

        // Capability must be a subtype of the lower bounds of the typeparam.
        ast_t* lower = viewpoint_lower(super);
        ok = check_cap_and_ephemeral(sub, lower);
        ast_free_unattached(lower);
        return ok;
      }

      default: {}
    }
  }

  return false;
}

// Both sub and super are tuples.
static bool is_tuple_sub_tuple(ast_t* sub, ast_t* super)
{
  // All elements must be pairwise subtypes.
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

// The subtype is either a nominal or a tuple, the super type is a union.
static bool is_subtype_union(ast_t* sub, ast_t* super)
{
  // Must be a subtype of one element of the union.
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    if(is_subtype(sub, child))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// The subtype is either a nominal or a tuple, the super type is an isect.
static bool is_subtype_isect(ast_t* sub, ast_t* super)
{
  // Must be a subtype of all elements of the intersection.
  ast_t* child = ast_child(super);

  while(child != NULL)
  {
    if(!is_subtype(sub, child))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

// The subtype is a nominal, typeparamref or tuple, the super type is an arrow.
static bool is_subtype_arrow(ast_t* sub, ast_t* super)
{
  // Must be a subtype of the lower bounds.
  ast_t* lower = viewpoint_lower(super);
  bool ok = is_subtype(sub, lower);
  ast_free_unattached(lower);

  return ok;
}

// The subtype is a union, the supertype could be anything.
static bool is_union_subtype(ast_t* sub, ast_t* super)
{
  // All elements of the union must be a subtype of super.
  ast_t* child = ast_child(sub);

  while(child != NULL)
  {
    if(!is_subtype(child, super))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

// The subtype is an isect, the supertype is an isect.
static bool is_isect_sub_isect(ast_t* sub, ast_t* super)
{
  // For every type in super, there must be some type in sub that is a subtype.
  ast_t* super_child = ast_child(super);

  while(super_child != NULL)
  {
    if(!is_isect_subtype(sub, super_child))
      return false;

    super_child = ast_sibling(super_child);
  }

  return true;
}

// The subtype is an isect, the supertype could be anything.
static bool is_isect_subtype(ast_t* sub, ast_t* super)
{
  if(ast_id(super) == TK_ISECTTYPE)
    return is_isect_sub_isect(sub, super);

  // One element of the intersection must be a subtype of super.
  ast_t* child = ast_child(sub);

  while(child != NULL)
  {
    if(is_subtype(child, super))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// The subtype is a tuple, the supertype could be anything.
static bool is_tuple_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      // A tuple can never be a strict subtype of a nominal type.
      return false;

    case TK_TYPEPARAMREF:
      // A tuple can never be a strict subtype of a type parameter.
      return false;

    case TK_UNIONTYPE:
      return is_subtype_union(sub, super);

    case TK_ISECTTYPE:
      return is_subtype_isect(sub, super);

    case TK_TUPLETYPE:
      return is_tuple_sub_tuple(sub, super);

    case TK_ARROW:
      return is_subtype_arrow(sub, super);

    default: {}
  }

  return false;
}

// The subtype is a pointer, the supertype could be anything.
static bool is_pointer_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
    {
      // Must be a Pointer, and the type argument must be the same.
      return is_pointer(super) &&
        is_eq_typeargs(sub, super) &&
        check_cap_and_ephemeral(sub, super);
    }

    case TK_TYPEPARAMREF:
    {
      // We must be a subtype of the constraint.
      ast_t* def = (ast_t*)ast_data(super);
      ast_t* constraint = ast_childidx(def, 1);

      return is_pointer_subtype(sub, constraint);
    }

    case TK_ARROW:
    {
      // We must be a subtype of the lower bounds.
      ast_t* lower = viewpoint_lower(super);
      bool ok = is_pointer_subtype(sub, lower);
      ast_free_unattached(lower);
      return ok;
    }

    default: {}
  }

  return false;
}

// The subtype is a nominal, the supertype could be anything.
static bool is_nominal_subtype(ast_t* sub, ast_t* super)
{
  // Special case Pointer[A]. It can only be a subtype of itself, because
  // in the code generator, a pointer has no vtable.
  if(is_pointer(sub))
    return is_pointer_subtype(sub, super);

  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return is_nominal_sub_nominal(sub, super);

    case TK_TYPEPARAMREF:
      return is_nominal_sub_typeparam(sub, super);

    case TK_UNIONTYPE:
      return is_subtype_union(sub, super);

    case TK_ISECTTYPE:
      return is_subtype_isect(sub, super);

    case TK_ARROW:
      return is_subtype_arrow(sub, super);

    default: {}
  }

  return false;
}

// The subtype is a typeparam, the supertype is a typeparam.
static bool is_typeparam_sub_typeparam(ast_t* sub, ast_t* super)
{
  // If the supertype is also a typeparam, we must be the same typeparam by
  // identity and be a subtype by cap and ephemeral.
  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  if(sub_def != super_def)
    return false;

  return check_cap_and_ephemeral(sub, super);
}

// The subtype is a typeparam, the supertype could be anything.
static bool is_typeparam_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_TYPEPARAMREF:
      if(is_typeparam_sub_typeparam(sub, super))
        return true;
      break;

    case TK_UNIONTYPE:
      if(is_subtype_union(sub, super))
        return true;
      break;

    case TK_ISECTTYPE:
      if(is_subtype_isect(sub, super))
        return true;
      break;

    case TK_ARROW:
      if(is_subtype_arrow(sub, super))
        return true;
      break;

    default: {}
  }

  // We can be a subtype if our upper bounds, ie our constraint, is a subtype.
  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* constraint = ast_childidx(sub_def, 1);

  if(ast_id(constraint) == TK_TYPEPARAMREF)
  {
    ast_t* constraint_def = (ast_t*)ast_data(constraint);

    if(constraint_def == sub_def)
      return false;
  }

  // Constraint must be modified with sub ephemerality.
  AST_GET_CHILDREN(sub, name, cap, eph);
  ast_t* r_constraint = set_cap_and_ephemeral(constraint, TK_NONE,
    ast_id(eph));

  bool ok = is_subtype(r_constraint, super);
  ast_free_unattached(r_constraint);

  return ok;
}

// The subtype is an arrow, the supertype is an arrow.
static bool is_arrow_sub_arrow(ast_t* sub, ast_t* super)
{
  // If the supertype is also a viewpoint and the viewpoint is the same, then
  // we are a subtype if our adapted type is a subtype of the supertype's
  // adapted type.
  AST_GET_CHILDREN(sub, sub_left, sub_right);
  AST_GET_CHILDREN(super, super_left, super_right);

  return is_eqtype(sub_left, super_left) && is_subtype(sub_right, super_right);
}

// The subtype is an arrow, the supertype could be anything.
static bool is_arrow_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_UNIONTYPE:
      // A->B <: (C | D)
      if(is_subtype_union(sub, super))
        return true;
      break;

    case TK_ISECTTYPE:
      // A->B <: (C & D)
      if(is_subtype_isect(sub, super))
        return true;
      break;

    case TK_TYPEPARAMREF:
    {
      // A->B <: C
      ast_t* right = ast_child(sub);

      if(is_eqtype(right, super))
      {
        // C->B <: C
        // If we are adapted by the supertype, we are a subtype if our lower
        // bounds is a subtype of super.
        ast_t* lower = viewpoint_lower(sub);
        bool ok = is_subtype(lower, super);
        ast_free_unattached(lower);
        return ok;
      }

      if(ast_id(right) != TK_THISTYPE)
        return false;
      break;
    }

    case TK_ARROW:
      // A->B <: C->D
      if(is_arrow_sub_arrow(sub, super))
        return true;
      break;

    default: {}
  }

  // Otherwise, we are a subtype if our upper bounds is a subtype of super.
  ast_t* upper = viewpoint_upper(sub);
  bool ok = is_subtype(upper, super);
  ast_free_unattached(upper);
  return ok;
}

bool is_subtype(ast_t* sub, ast_t* super)
{
  assert(sub != NULL);
  assert(super != NULL);

  if(ast_id(super) == TK_DONTCARE)
    return true;

  switch(ast_id(sub))
  {
    case TK_UNIONTYPE:
      return is_union_subtype(sub, super);

    case TK_ISECTTYPE:
      return is_isect_subtype(sub, super);

    case TK_TUPLETYPE:
      return is_tuple_subtype(sub, super);

    case TK_NOMINAL:
      return is_nominal_subtype(sub, super);

    case TK_TYPEPARAMREF:
      return is_typeparam_subtype(sub, super);

    case TK_ARROW:
      return is_arrow_subtype(sub, super);

    case TK_THISTYPE:
      return ast_id(super) == TK_THISTYPE;

    case TK_FUNTYPE:
      return false;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return is_fun_sub_fun(sub, super);

    default: {}
  }

  assert(0);
  return false;
}

bool is_eqtype(ast_t* a, ast_t* b)
{
  return is_subtype(a, b) && is_subtype(b, a);
}

bool is_pointer(ast_t* type)
{
  return is_literal(type, "Pointer");
}

bool is_none(ast_t* type)
{
  return is_literal(type, "None");
}

bool is_env(ast_t* type)
{
  return is_literal(type, "Env");
}

bool is_bool(ast_t* type)
{
  return is_literal(type, "Bool");
}

bool is_float(ast_t* type)
{
  return is_literal(type, "F32") ||
    is_literal(type, "F64");
}

bool is_integer(ast_t* type)
{
  return is_literal(type, "I8") ||
    is_literal(type, "I16") ||
    is_literal(type, "I32") ||
    is_literal(type, "I64") ||
    is_literal(type, "I128") ||
    is_literal(type, "U8") ||
    is_literal(type, "U16") ||
    is_literal(type, "U32") ||
    is_literal(type, "U64") ||
    is_literal(type, "U128");
}

bool is_machine_word(ast_t* type)
{
  return is_bool(type) ||
    is_literal(type, "I8") ||
    is_literal(type, "I16") ||
    is_literal(type, "I32") ||
    is_literal(type, "I64") ||
    is_literal(type, "I128") ||
    is_literal(type, "U8") ||
    is_literal(type, "U16") ||
    is_literal(type, "U32") ||
    is_literal(type, "U64") ||
    is_literal(type, "U128") ||
    is_literal(type, "F32") ||
    is_literal(type, "F64");
}

bool is_signed(pass_opt_t* opt, ast_t* type)
{
  if(type == NULL)
    return false;

  ast_t* builtin = type_builtin(opt, type, "Signed");

  if(builtin == NULL)
    return false;

  bool ok = is_subtype(type, builtin);
  ast_free_unattached(builtin);
  return ok;
}

bool is_composite(ast_t* type)
{
  return !is_machine_word(type) && !is_pointer(type);
}

bool is_constructable(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_TUPLETYPE:
      return false;

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(is_constructable(child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);

      switch(ast_id(def))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
        {
          ast_t* members = ast_childidx(def, 4);
          ast_t* member = ast_child(members);

          while(member != NULL)
          {
            if(ast_id(member) == TK_NEW)
              return true;

            member = ast_sibling(member);
          }

          return false;
        }

        case TK_PRIMITIVE:
        case TK_CLASS:
        case TK_ACTOR:
          return true;

        default: {}
      }
      break;
    }

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      ast_t* constraint = ast_childidx(def, 1);
      ast_t* constraint_def = (ast_t*)ast_data(constraint);

      if(def == constraint_def)
        return false;

      return is_constructable(constraint);
    }

    case TK_ARROW:
      return is_constructable(ast_childidx(type, 1));

    default: {}
  }

  assert(0);
  return false;
}

bool is_concrete(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_TUPLETYPE:
      return false;

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

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);

      switch(ast_id(def))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          return false;

        case TK_PRIMITIVE:
        case TK_CLASS:
        case TK_ACTOR:
          return true;

        default: {}
      }
      break;
    }

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      ast_t* constraint = ast_childidx(def, 1);

      return is_constructable(constraint);
    }

    case TK_ARROW:
      return is_concrete(ast_childidx(type, 1));

    default: {}
  }

  assert(0);
  return false;
}
