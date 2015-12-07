#include "subtype.h"
#include "reify.h"
#include "cap.h"
#include "alias.h"
#include "assemble.h"
#include "viewpoint.h"
#include "../ast/astbuild.h"
#include "../ast/stringtab.h"
#include "../expr/literal.h"
#include <assert.h>

static bool is_isect_subtype(ast_t* sub, ast_t* super);

bool is_literal(ast_t* type, const char* name)
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

bool is_sub_cap_and_ephemeral(ast_t* sub, ast_t* super)
{
  ast_t* sub_cap = fetch_cap(sub);
  ast_t* sub_eph = ast_sibling(sub_cap);
  ast_t* super_cap = fetch_cap(super);
  ast_t* super_eph = ast_sibling(super_cap);

  return is_cap_sub_cap(ast_id(sub_cap), ast_id(sub_eph), ast_id(super_cap),
    ast_id(super_eph));
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

static bool is_nominal_eq_nominal(ast_t* sub, ast_t* super)
{
  ast_t* sub_cap = fetch_cap(sub);
  ast_t* sub_eph = ast_sibling(sub_cap);
  ast_t* super_cap = fetch_cap(super);
  ast_t* super_eph = ast_sibling(super_cap);

  token_id t_sub_cap = ast_id(sub_cap);
  token_id t_sub_eph = ast_id(sub_eph);
  token_id t_super_cap = ast_id(super_cap);
  token_id t_super_eph = ast_id(super_eph);

  if((t_sub_cap != t_super_cap) || (t_sub_eph != t_super_eph))
    return false;

  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  // If we are the same nominal type, our typeargs must be the same.
  if(sub_def == super_def)
    return is_eq_typeargs(sub, super);

  return false;
}

static bool is_recursive_interface(ast_t* sub, ast_t* super, ast_t* isub,
  ast_t* isuper)
{
  if(isuper == NULL)
  {
    assert(isub == NULL);
    return false;
  }

  assert(ast_id(isub) == TK_NOMINAL);
  assert(ast_id(isuper) == TK_NOMINAL);

  return (ast_id(sub) == TK_NOMINAL) && (ast_id(super) == TK_NOMINAL) &&
    is_nominal_eq_nominal(sub, isub) && is_nominal_eq_nominal(super, isuper);
}

static bool is_reified_fun_sub_fun(ast_t* sub, ast_t* super,
  ast_t* isub, ast_t* isuper)
{
  AST_GET_CHILDREN(sub, sub_cap, sub_id, sub_typeparams, sub_params,
    sub_result, sub_throws);

  AST_GET_CHILDREN(super, super_cap, super_id, super_typeparams, super_params,
    super_result, super_throws);

  switch(ast_id(sub))
  {
    case TK_NEW:
    {
      // Covariant receiver.
      if(!is_cap_sub_cap(ast_id(sub_cap), TK_NONE, ast_id(super_cap), TK_NONE))
        return false;

      // Covariant result. Don't check this for interfaces, as it produces
      // an infinite loop. It will be true if the whole interface is provided.
      if(isuper == NULL)
      {
        if(!is_subtype(sub_result, super_result))
          return false;

        // If either result type is a machine word, the other must be as well.
        if(is_machine_word(sub_result) && !is_machine_word(super_result))
          return false;

        if(is_machine_word(super_result) && !is_machine_word(sub_result))
          return false;
      }

      break;
    }

    case TK_FUN:
    case TK_BE:
    {
      // Contravariant receiver.
      if(!is_cap_sub_cap(ast_id(super_cap), TK_NONE, ast_id(sub_cap), TK_NONE))
        return false;

      // Covariant result.
      if(!is_recursive_interface(sub_result, super_result, isub, isuper))
      {
        if(!is_subtype(sub_result, super_result))
          return false;

        // If either result type is a machine word, the other must be as well.
        if(is_machine_word(sub_result) && !is_machine_word(super_result))
          return false;

        if(is_machine_word(super_result) && !is_machine_word(sub_result))
          return false;
      }

      break;
    }

    default: {}
  }

  // Contravariant type parameter constraints.
  ast_t* sub_typeparam = ast_child(sub_typeparams);
  ast_t* super_typeparam = ast_child(super_typeparams);

  while((sub_typeparam != NULL) && (super_typeparam != NULL))
  {
    ast_t* sub_constraint = ast_childidx(sub_typeparam, 1);
    ast_t* super_constraint = ast_childidx(super_typeparam, 1);

    if(!is_recursive_interface(super_constraint, sub_constraint, isub, isuper)
      && !is_subtype(super_constraint, sub_constraint))
      return false;

    sub_typeparam = ast_sibling(sub_typeparam);
    super_typeparam = ast_sibling(super_typeparam);
  }

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
    if(!is_recursive_interface(super_type, sub_type, isub, isuper) &&
      !is_subtype(super_type, sub_type))
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

static bool is_fun_sub_fun(ast_t* sub, ast_t* super,
  ast_t* isub, ast_t* isuper)
{
  token_id tsub = ast_id(sub);
  token_id tsuper = ast_id(super);

  switch(tsub)
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      break;

    default:
      return false;
  }

  switch(tsuper)
  {
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      break;

    default:
      return false;
  }

  // A constructor can only be a subtype of a constructor.
  if(((tsub == TK_NEW) || (tsuper == TK_NEW)) && (tsub != tsuper))
    return false;

  AST_GET_CHILDREN(sub, sub_cap, sub_id, sub_typeparams, sub_params);
  AST_GET_CHILDREN(super, super_cap, super_id, super_typeparams, super_params);

  // Must have the same name.
  if(ast_name(sub_id) != ast_name(super_id))
    return false;

  // Must have the same number of type parameters and parameters.
  if((ast_childcount(sub_typeparams) != ast_childcount(super_typeparams)) ||
    (ast_childcount(sub_params) != ast_childcount(super_params)))
    return false;

  ast_t* r_sub = sub;

  if(ast_id(super_typeparams) != TK_NONE)
  {
    // Reify sub with the type parameters of super.
    BUILD(typeargs, super_typeparams, NODE(TK_TYPEARGS));
    ast_t* super_typeparam = ast_child(super_typeparams);

    while(super_typeparam != NULL)
    {
      AST_GET_CHILDREN(super_typeparam, super_id, super_constraint);
      token_id cap = cap_from_constraint(super_constraint);

      BUILD(typearg, super_typeparam,
        NODE(TK_TYPEPARAMREF, TREE(super_id) NODE(cap) NONE));

      ast_t* def = ast_get(super_typeparam, ast_name(super_id), NULL);
      ast_setdata(typearg, def);
      ast_append(typeargs, typearg);

      super_typeparam = ast_sibling(super_typeparam);
    }

    r_sub = reify(sub, sub, sub_typeparams, typeargs);
    ast_free_unattached(typeargs);
  }

  bool ok = is_reified_fun_sub_fun(r_sub, super, isub, isuper);

  if(r_sub != sub)
    ast_free_unattached(r_sub);

  return ok;
}

static bool is_nominal_sub_interface(ast_t* sub, ast_t* super)
{
  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  // A struct has no descriptor, so can't be a subtype of an interface.
  if(ast_id(sub_def) == TK_STRUCT)
    return false;

  ast_t* sub_typeargs = ast_childidx(sub, 2);
  ast_t* super_typeargs = ast_childidx(super, 2);

  ast_t* sub_typeparams = ast_childidx(sub_def, 1);
  ast_t* super_typeparams = ast_childidx(super_def, 1);

  ast_t* super_members = ast_childidx(super_def, 4);
  ast_t* super_member = ast_child(super_members);

  while(super_member != NULL)
  {
    ast_t* super_member_id = ast_childidx(super_member, 1);
    ast_t* sub_member = ast_get(sub_def, ast_name(super_member_id), NULL);

    if(sub_member == NULL)
      return false;

    ast_t* r_sub_member = reify(sub_typeargs, sub_member, sub_typeparams,
      sub_typeargs);

    if(r_sub_member == NULL)
      return false;

    ast_t* r_super_member = reify(super_typeargs, super_member,
      super_typeparams, super_typeargs);

    if(r_super_member == NULL)
    {
      ast_free_unattached(r_sub_member);
      return false;
    }

    bool ok = is_fun_sub_fun(r_sub_member, r_super_member, sub, super);
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

  // A struct has no descriptor, so can't be a subtype of a trait.
  if(ast_id(sub_def) == TK_STRUCT)
    return false;

  // Get our typeparams and typeargs.
  ast_t* typeparams = ast_childidx(sub_def, 1);
  ast_t* typeargs = ast_childidx(sub, 2);

  // Check traits, depth first.
  ast_t* traits = ast_childidx(sub_def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    // Reify the trait with our typeargs.
    ast_t* r_trait = reify(typeargs, trait, typeparams, typeargs);

    if(r_trait == NULL)
      return false;

    // Use the cap and ephemerality of the subtype.
    AST_GET_CHILDREN(sub, pkg, name, typeparams, cap, eph);
    ast_t* rr_trait = set_cap_and_ephemeral(r_trait, ast_id(cap), ast_id(eph));

    if(rr_trait != r_trait)
    {
      ast_free_unattached(r_trait);
      r_trait = rr_trait;
    }

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
  if(!is_sub_cap_and_ephemeral(sub, super))
    return false;

  ast_t* sub_def = (ast_t*)ast_data(sub);
  ast_t* super_def = (ast_t*)ast_data(super);

  switch(ast_id(super_def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      // If we are the same nominal type, our typeargs must be the same.
      if(sub_def == super_def)
        return is_eq_typeargs(sub, super);

      // If we aren't the same type, we can't be a subtype of a concrete type.
      return false;

    case TK_INTERFACE:
      // Check for an explicit provide or a structural subtype.
      return is_nominal_sub_trait(sub, super) ||
        is_nominal_sub_interface(sub, super);

    case TK_TRAIT:
      // If we are the same nominal type, our typeargs must be the same.
      if(sub_def == super_def)
        return is_eq_typeargs(sub, super);

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
      case TK_STRUCT:
      case TK_CLASS:
      case TK_ACTOR:
      {
        // Use the cap and ephemerality of the subtype.
        AST_GET_CHILDREN(super, name, cap, eph);
        ast_t* r_constraint = set_cap_and_ephemeral(constraint, ast_id(cap),
          ast_id(eph));

        // Must be a subtype of the constraint.
        bool ok = is_subtype(sub, constraint);
        ast_free_unattached(r_constraint);

        if(!ok)
          return false;

        // Capability must be a subtype of the lower bounds of the typeparam.
        ast_t* lower = viewpoint_lower(super);
        ok = is_sub_cap_and_ephemeral(sub, lower);
        ast_free_unattached(lower);
        return ok;
      }

      default: {}
    }
  }

  return false;
}

// The subtype is a nominal, the super type is a union.
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

// The subtype is a nominal, the super type is an isect.
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

// The subtype is a nominal or typeparamref, the super type is an arrow.
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

// Build the type of the specified element within tuples of the specified
// cardinality within the given type.
// The returned type (if any) must be freed with ast_free_unattached().
// Returns: tuple element type, NULL for no such type.
static ast_t* extract_tuple_elem_type(ast_t* type, size_t elem_index,
  size_t cardinality)
{
  assert(type != NULL);
  assert(elem_index < cardinality);

  switch(ast_id(type))
  {
    case TK_DONTCARE:
      return type;

    case TK_UNIONTYPE:
    {
      ast_t* r = NULL;

      // Union child types.
      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* child_t = extract_tuple_elem_type(p, elem_index, cardinality);

        // Ignore NULL child types.
        if(child_t != NULL)
        {
          if(r == NULL)
          {
            r = child_t;
          }
          else
          {
            BUILD(union_t, r, NODE(TK_UNIONTYPE, TREE(r) TREE(child_t)));
            r = union_t;
          }
        }
      }

      return r;
    }

    case TK_ISECTTYPE:
    {
      ast_t* r = NULL;

      // Intersect child types.
      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* child_t = extract_tuple_elem_type(p, elem_index, cardinality);

        if(child_t == NULL)
        {
          // Any impossible child means our whole type is impossible.
          ast_free_unattached(r);
          return NULL;
        }

        if(r == NULL)
        {
          r = child_t;
        }
        else
        {
          BUILD(isec_t, r, NODE(TK_ISECTTYPE, TREE(r) TREE(child_t)));
          r = isec_t;
        }
      }

      return r;
    }

    case TK_TUPLETYPE:
      if(ast_childcount(type) != cardinality) // Type is wrong size.
        return NULL;

      // This tuple is the right size, return type of relevant element.
      return ast_childidx(type, elem_index);

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);

      ast_t* right_t = extract_tuple_elem_type(right, elem_index, cardinality);

      if(right_t == NULL) // No valid type.
        return NULL;

      BUILD(t, left, NODE(TK_ARROW, TREE(left) TREE(right_t)));
      return t;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      // Cannot be a tuple.
      return NULL;

    default:
      ast_print(type);
      assert(0);
      return NULL;
  }
}


// The subtype is a tuple, the supertype could be anything.
static bool is_tuple_subtype(ast_t* sub, ast_t* super)
{
  assert(sub != NULL);
  assert(super != NULL);

  // Deconstruct both sub and super together, even if super isn't directly a
  // tuple.
  size_t cardinality = ast_childcount(sub);
  size_t i = 0;

  // Check each element in turn.
  for(ast_t* p = ast_child(sub); p != NULL; p = ast_sibling(p))
  {
    ast_t* elem_t = extract_tuple_elem_type(super, i++, cardinality);

    if(elem_t == NULL)  // No valid super type.
      return false;

    bool r = is_subtype(p, elem_t);
    ast_free_unattached(elem_t);

    if(!r)  // Elements not subtypes.
      return false;
  }

  // All elements are subtypes.
  return true;
}

// The subtype is a nominal, the supertype could be anything.
static bool is_nominal_subtype(ast_t* sub, ast_t* super)
{
  switch(ast_id(super))
  {
    case TK_NOMINAL:
      return is_nominal_sub_nominal(sub, super);

    case TK_TYPEPARAMREF:
      return is_nominal_sub_typeparam(sub, super);

    case TK_UNIONTYPE:
    {
      ast_t* def = (ast_t*)ast_data(sub);

      // A struct has no descriptor, so can't be a subtype of a union.
      if(ast_id(def) == TK_STRUCT)
        return false;

      return is_subtype_union(sub, super);
    }

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

  return is_sub_cap_and_ephemeral(sub, super);
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

  // Use the cap and ephemerality of the subtype.
  AST_GET_CHILDREN(sub, name, cap, eph);
  ast_t* r_constraint = set_cap_and_ephemeral(constraint, ast_id(cap),
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

  switch(ast_id(sub_left))
  {
    case TK_THISTYPE:
      switch(ast_id(super_left))
      {
        case TK_THISTYPE:
        case TK_BOXTYPE:
          break;

        default:
          return false;
      }
      break;

    case TK_BOXTYPE:
      if(ast_id(super_left) != TK_BOXTYPE)
        return false;
      break;

    default:
      if(!is_eqtype(sub_left, super_left))
        return false;
      break;
  }

  return is_subtype(sub_right, super_right);
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

      switch(ast_id(right))
      {
        case TK_THISTYPE:
        case TK_BOXTYPE:
          break;

        default:
          if(is_eqtype(right, super))
          {
            // C->B <: C
            // If we are adapted by the supertype, we are a subtype if our
            // lower bounds is a subtype of super.
            ast_t* lower = viewpoint_lower(sub);
            bool ok = is_subtype(lower, super);
            ast_free_unattached(lower);
            return ok;
          }
          break;
      }
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

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return false;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      return is_fun_sub_fun(sub, super, NULL, NULL);

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

bool is_maybe(ast_t* type)
{
  return is_literal(type, "Maybe");
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
  return is_literal(type, "F32") || is_literal(type, "F64");
}

bool is_integer(ast_t* type)
{
  return
    is_literal(type, "I8") ||
    is_literal(type, "I16") ||
    is_literal(type, "I32") ||
    is_literal(type, "I64") ||
    is_literal(type, "I128") ||
    is_literal(type, "ILong") ||
    is_literal(type, "ISize") ||
    is_literal(type, "U8") ||
    is_literal(type, "U16") ||
    is_literal(type, "U32") ||
    is_literal(type, "U64") ||
    is_literal(type, "U128") ||
    is_literal(type, "ULong") ||
    is_literal(type, "USize");
}

bool is_machine_word(ast_t* type)
{
  return is_bool(type) || is_integer(type) || is_float(type);
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
        case TK_STRUCT:
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
        case TK_STRUCT:
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

bool is_known(ast_t* type)
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
        if(is_known(child))
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
        case TK_STRUCT:
        case TK_CLASS:
        case TK_ACTOR:
          return true;

        default: {}
      }
      break;
    }

    case TK_ARROW:
      return is_known(ast_childidx(type, 1));

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      ast_t* constraint = ast_childidx(def, 1);

      return is_known(constraint);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_entity(ast_t* type, token_id entity)
{
  switch(ast_id(type))
  {
    case TK_TUPLETYPE:
      return false;

    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!is_entity(child, entity))
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
        if(is_entity(child, entity))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(type);
      return ast_id(def) == entity;
    }

    case TK_ARROW:
      return is_entity(ast_childidx(type, 1), entity);

    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      ast_t* constraint = ast_childidx(def, 1);

      return is_entity(constraint, entity);
    }

    default: {}
  }

  assert(0);
  return false;
}
