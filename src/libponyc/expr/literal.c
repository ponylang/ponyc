#include "literal.h"
#include "reference.h"
#include "../pass/expr.h"
#include "../pass/names.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/alias.h"
#include "../type/reify.h"
#include "../type/cap.h"
#include "../ast/token.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>


bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type = type_builtin(ast, name);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}

bool expr_this(ast_t* ast)
{
  // TODO: If in a constructor, lower it to tag if not all fields are defined.

  // If this is the return value of a function, it is ephemeral.
  ast_t* parent = ast_parent(ast);
  bool ephemeral = false;

  switch(ast_id(parent))
  {
    case TK_RETURN:
      ephemeral = true;
      break;

    case TK_SEQ:
      ephemeral = ast_id(ast_parent(parent)) == TK_FUN;
      break;

    default: {}
  }

  ast_t* type = type_for_this(ast, cap_for_receiver(ast), ephemeral);
  ast_settype(ast, type);

  if(ast_enclosing_default_arg(ast) != NULL)
  {
    ast_error(ast, "can't reference 'this' in a default argument");
    return false;
  }

  if(!sendable(type) && (ast_nearest(ast, TK_RECOVER) != NULL))
  {
    ast_error(ast,
      "can't access a non-sendable 'this' from inside a recover expression");
    return false;
  }

  ast_t* nominal;

  if(ast_id(type) == TK_NOMINAL)
    nominal = type;
  else
    nominal = ast_childidx(type, 1);

  ast_t* typeargs = ast_childidx(nominal, 2);
  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    if(!expr_nominal(&typearg))
    {
      ast_error(ast, "couldn't create a type for 'this'");
      ast_free(type);
      return false;
    }

    typearg = ast_sibling(typearg);
  }

  if(!expr_nominal(&nominal))
  {
    ast_error(ast, "couldn't create a type for 'this'");
    ast_free(type);
    return false;
  }

  return true;
}

bool expr_tuple(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  ast_t* type;

  if(ast_sibling(child) == NULL)
  {
    type = ast_type(child);
  } else {
    type = ast_from(ast, TK_TUPLETYPE);

    while(child != NULL)
    {
      ast_t* c_type = ast_type(child);

      if(c_type == NULL)
      {
        ast_error(child,
          "a tuple can't contain a control flow statement that never results "
          "in a value");
        return false;
      }

      if(is_type_literal(c_type))
      {
        // If any tuple members are literal, the whole tuple is
        ast_free(type);
        type = ast_from(ast, TK_TUPLELITERAL);
        break;
      }

      ast_append(type, c_type);
      child = ast_sibling(child);
    }
  }

  ast_settype(ast, type);
  ast_inheriterror(ast);
  return true;
}

bool expr_error(ast_t* ast)
{
  if(ast_sibling(ast) != NULL)
  {
    ast_error(ast, "error must be the last expression in a sequence");
    ast_error(ast_sibling(ast), "error is followed with this expression");
    return false;
  }

  ast_seterror(ast);
  return true;
}

bool expr_compiler_intrinsic(ast_t* ast)
{
  ast_t* fun = ast_enclosing_method_body(ast);
  ast_t* body = ast_childidx(fun, 6);
  ast_t* child = ast_child(body);

  // Allow a docstring before the compiler_instrinsic.
  if(ast_id(child) == TK_STRING)
    child = ast_sibling(child);

  if((child != ast) || (ast_sibling(child) != NULL))
  {
    ast_error(ast, "a compiler intrinsic must be the entire body");
    return false;
  }

  ast_settype(ast, ast_from(ast, TK_COMPILER_INTRINSIC));
  return true;
}

bool expr_nominal(ast_t** astp)
{
  // Resolve typealiases and typeparam references.
  if(!names_nominal(*astp, astp))
    return false;

  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      return flatten_typeparamref(ast) == AST_OK;

    case TK_NOMINAL:
      break;

    default:
      return true;
  }

  // If still nominal, check constraints.
  ast_t* def = (ast_t*)ast_data(ast);

  // Special case: don't check the constraint of a Pointer. This allows a
  // Pointer[Pointer[A]], which is normally not allowed, as a Pointer[A] is
  // not a subtype of Any.
  ast_t* id = ast_child(def);
  const char* name = ast_name(id);

  if(!strcmp(name, "Pointer"))
    return true;

  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* typeargs = ast_childidx(ast, 2);

  return check_constraints(typeparams, typeargs, true);
}

static bool show_partiality(ast_t* ast)
{
  ast_t* child = ast_child(ast);
  bool found = false;

  while(child != NULL)
  {
    if(ast_canerror(child))
      found |= show_partiality(child);

    child = ast_sibling(child);
  }

  if(found)
    return true;

  if(ast_canerror(ast))
  {
    ast_error(ast, "an error can be raised here");
    return true;
  }

  return false;
}

static bool check_fields_defined(ast_t* ast)
{
  assert(ast_id(ast) == TK_NEW);

  ast_t* members = ast_parent(ast);
  ast_t* member = ast_child(members);
  bool result = true;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_t* def = ast_get(ast, ast_name(id), &status);

        if((def != member) || (status != SYM_DEFINED))
        {
          ast_error(def, "field left undefined in constructor");
          result = false;
        }

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(!result)
    ast_error(ast, "constructor with undefined fields is here");

  return result;
}

static bool check_return_type(ast_t* ast)
{
  assert(ast_id(ast) == TK_FUN);

  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  // If the return type is None, add a None at the end of the body.
  if(is_none(type))
  {
    ast_t* last = ast_childlast(body);
    BUILD(ref, last, NODE(TK_REFERENCE, ID("None")));
    ast_append(body, ref);
    expr_reference(ref);
    return true;
  }

  ast_t* body_type = ast_type(body);

  // The last statement is an error, and we've already checked any return
  // expressions in the method.
  if(body_type == NULL)
    return true;

  // If it's a compiler intrinsic, ignore it.
  if(ast_id(body_type) == TK_COMPILER_INTRINSIC)
    return true;

  // The body type must match the return type, without subsumption, or an alias
  // of the body type must be a subtype of the return type.
  ast_t* a_type = alias(body_type);

  if(!is_eqtype(body_type, type) && !is_subtype(a_type, type))
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body isn't the result type");
    ast_error(last, "function body expression is here");
    ast_error(type, "function return type: %s", ast_print_type(type));
    ast_error(body_type, "function body type: %s", ast_print_type(body_type));
    ast_free_unattached(a_type);
    return false;
  }

  ast_free_unattached(a_type);
  return true;
}

bool expr_fun(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  if(!coerce_literals(body, type))
    return false;

  if(ast_id(body) == TK_NONE)
    return true;

  ast_t* def = ast_enclosing_type(ast);
  bool is_trait = (ast_id(def) == TK_TRAIT) || (ast_id(def) == TK_INTERFACE);

  // Check partial functions.
  if(ast_id(can_error) == TK_QUESTION)
  {
    // If a partial function, check that we might actually error.
    if(!is_trait && !ast_canerror(body))
    {
      ast_error(can_error, "function body is not partial but the function is");
      return false;
    }
  } else {
    // If not a partial function, check that we can't error.
    if(ast_canerror(body))
    {
      ast_error(can_error, "function body is partial but the function is not");
      show_partiality(body);
      return false;
    }
  }

  switch(ast_id(ast))
  {
    case TK_NEW:
      return check_fields_defined(ast);

    case TK_FUN:
      return check_return_type(ast);

    default: {}
  }

  return true;
}


// New literal inference

typedef enum
{
	LIT_OK,
	LIT_CONSTRAINED,
	LIT_NO_TYPES
} lit_result_t;


static lit_result_t uif_expr(ast_t* literals, ast_t** type_ptr);


#define UIF_COUNT   12
#define UIF_TYPES   0xFFF

static const char* _str_dollar_1;

static const char* _str_uif_types[UIF_COUNT] =
{
  "F64", "F32",
  "I128", "U128",
  "I64", "U64",
  "I32", "U32",
  "I16", "U16",
  "I8", "U8"
};


/// Setup interned strings to make type comparisons quicker
void literals_init()
{
  _str_dollar_1 = stringtab("$1");

  for(int i = 0; i < UIF_COUNT; i++)
    _str_uif_types[i] = stringtab(_str_uif_types[i]);
}
  

// Generate the correct literal mask for the given ID
static int uif_mask(token_id id)
{
  switch(id)
  {
    case TK_NUMBERLITERAL: return 0xFFF;
    case TK_INTLITERAL: return 0xFFC;
    case TK_FLOATLITERAL: return 0x003;
    default:
      assert(0);
      return 0;
  }
}


// Return the index of the best UIF type in the given non-empty set
static ast_t* best_uif_in_set(int set, ast_t* from_type)
{
  assert((set != 0) && ((set & ~UIF_TYPES) == 0));

  for(int64_t i = 0; i < UIF_COUNT; i++)
  {
    if((set & 1) != 0)
    {
      ast_t* type = type_builtin(from_type, _str_uif_types[i]);
      ast_setid(ast_childidx(type, 3), TK_ISO);
      ast_setid(ast_childidx(type, 4), TK_HAT);
      return type;
    }

    set >>= 1;
  }

  assert(0);
  return NULL;
}


// Compare now good the types a and b are for the given literal
// Both a and b must be valid types for the literal
// @return
//   <0 if b is better than a
//   >0 if a is better than b
//   0 if a and b are equal
static int compare_uif_types(ast_t* a, ast_t* b, ast_t* literals)
{
  assert(a != NULL);
  assert(b != NULL);
  assert(literals != NULL);
  assert(ast_type(literals) != NULL);

  switch(ast_id(ast_type(literals)))
  {
    case TK_TUPLELITERAL:
    {
      // Check tuple elements in order until a winner is found
      ast_t* child_a = ast_child(a);
      ast_t* child_b = ast_child(b);

      for(ast_t* p = ast_child(literals); p != NULL; p = ast_sibling(p))
      {
        assert(child_a != NULL);
        assert(child_b != NULL);

        int r = compare_uif_types(child_a, child_b, p);

        if(r != 0)
          return r;

        child_a = ast_sibling(child_a);
        child_b = ast_sibling(child_b);
      }

      // All elements match, tuples are equal
      assert(child_a == NULL);
      assert(child_b == NULL);
      return 0;
    }

    case TK_NUMBERLITERAL:
    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
    {
      assert(ast_id(a) == TK_NOMINAL);
      AST_GET_CHILDREN(a, package_a, id_a);
      assert(ast_name(package_a) == _str_dollar_1);
      const char* name_a = ast_name(id_a);

      assert(ast_id(b) == TK_NOMINAL);
      AST_GET_CHILDREN(b, package_b, id_b);
      assert(ast_name(package_b) == _str_dollar_1);
      const char* name_b = ast_name(id_b);

      if(name_a == name_b)  // Types are equal
        return 0;

      // Check through the UIF types in order, the first match wins
      for(int i = 0; i < UIF_TYPES; i++)
      {
        if(name_a == _str_uif_types[i]) // A is better
          return 1;

        if(name_b == _str_uif_types[i]) // B is better
          return -1;
      }

      assert(0);
      return 0;
    }

    default:
      // Not a literal
      return 0;
  }
}


// Pick the best of 2 types for the given literal and discard the other
// Either type may be NULL
static ast_t* best_uif_type(ast_t* a, ast_t* b, ast_t* literals)
{
  assert(literals != NULL);

  if(a == NULL)
    return b;

  if(b == NULL)
    return a;

  if(compare_uif_types(a, b, literals) < 0)
  {
    // B is better
    ast_free(a);
    return b;
  }

  // A is better (or the same)
  ast_free(b);
  return a;
}


// Determine the UIF types that satisfy the given "simple" type.
// Here a simple type is defined as a non-tuple type that does not depend on
// any formal parameters.
// The UIF types to check for is specified by the mask parameter, which must be
// a subset of UIF_TYPES.
static int uifset_simple_type(ast_t* type, int mask)
{
  assert(type != NULL);
  assert((mask != 0) && ((mask & ~UIF_TYPES) == 0));

  int set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    if((mask & (1 << i)) != 0)
    {
      ast_t* uif = type_builtin(type, _str_uif_types[i]);
      ast_setid(ast_childidx(uif, 3), TK_ISO);

      if(is_subtype(uif, type))
        set |= (1 << i);

      ast_free(uif);
    }
  }

  return set;
}


// Determine the UIF types that the given formal parameter may be
// The legal UIF types are specified by the mask parameter
static lit_result_t uif_formal_param(ast_t* type_param, int mask,
	int* out_uif_set)
{
  assert(type_param != NULL);
  assert(ast_id(type_param) == TK_TYPEPARAM);
  assert((mask != 0) && ((mask & ~UIF_TYPES) == 0));
  assert(out_uif_set != NULL);

  ast_t* constraint = ast_childidx(type_param, 1);
  assert(constraint != NULL);

  // If the constraint is not a subtype of Number then there are no legal types
  // in the set
  ast_t* number = type_builtin(type_param, "Number");
  bool is_number = is_subtype(constraint, number);
  ast_free(number);

  if(!is_number)  // The formal param may have non UIF types
    return LIT_NO_TYPES;

  int uif_set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    ast_t* uif = type_builtin(type_param, _str_uif_types[i]);

    BUILD(params, type_param, NODE(TK_TYPEPARAMS, TREE(ast_dup(type_param))));
    BUILD(args, type_param, NODE(TK_TYPEARGS, TREE(uif)));

    if(check_constraints(params, args, true))
      uif_set |= (1 << i);

    ast_free(args);
    ast_free(params);
  }

  if((uif_set & ~mask) != 0)
  {
    // We are not guaranteed constraint is a legal type for us
    return LIT_NO_TYPES;
  }

  *out_uif_set = uif_set;
  return LIT_OK;
}

/*
// Determine the UIF types that the given intersection type may be
static lit_result_t uif_intersect(ast_t* ast, int mask, int* out_uif_set)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_ISECTTYPE);
  assert(ast_type(ast) == NULL);
  assert(ast_child(ast) != NULL);;
  assert((mask != 0) && ((mask & ~UIF_TYPES) == 0));
  assert(out_uif_set != NULL);

  ast_t* formal_param = NULL;
  int constraint_uif_set = 0;
  int possible_uif_set = mask;

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) == TK_TYPEPARAMREF)
    {
      // We have a formal parameter
      ast_t* fp_def = (ast_t*)ast_data(p);

      // TODO(andy): handle single type constraints

      if(formal_param != NULL && fp_def != formal_param)
      {
        // We are intersecting (at least) 2 formal parameters, we are
        // guaranteed that we can't always pick a suitable type
        return LIT_NO_TYPES;
      }

      formal_param = fp_def;
      lit_result_t r = uif_formal_param(fp_def, mask, &constraint_uif_set);

      if(r == LIT_NO_TYPES)  // Constraint allows types we don't
        return LIT_NO_TYPES;
    }
    else
    {
      // We have a non-generic type
      possible_uif_set &= uifset_simple_type(p, mask);
    }
  }

  if(formal_param != NULL)
  {
    // All types the constraint might be must be allowed by the other types
    if((constraint_uif_set & possible_uif_set) != constraint_uif_set)
      return LIT_NO_TYPES;

    return LIT_CONSTRAINED;
  }

  *out_uif_set = possible_uif_set;
  return LIT_OK;
}
*/


// Determine the UIF types that the given non-tuple type may be
static lit_result_t uif_mono(ast_t* ast, int mask, int* out_uif_set)
{
	assert(ast != NULL);
	assert((mask != 0) && ((mask & ~UIF_TYPES) == 0));
	assert(out_uif_set != NULL);

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    {
      int union_set = 0;

      for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
      {
        int child_set;
        lit_result_t r = uif_mono(p, mask, &child_set);

        if(r == LIT_CONSTRAINED)
          return LIT_CONSTRAINED;

        union_set |= child_set;
      }

      *out_uif_set = union_set;
      return LIT_OK;
    }

    case TK_ISECTTYPE:
      //return uif_intersect(ast, mask, out_uif_set);
      ast_error(ast,
        "Literal inference not yet supported for intersect types");
      return LIT_NO_TYPES;

    case TK_ARROW:
      // Since we don't care about capabilities we can just use the rhs
      assert(ast_id(ast_childidx(ast, 1)) == TK_NOMINAL);
      *out_uif_set = uifset_simple_type(ast_childidx(ast, 1), mask);
      return LIT_OK;

    case TK_TYPEPARAMREF:
    {
      // We don't actually care what UIF types this type parameter can be, just
      // whether we're guaranteed it is one
      int x = 0;
      if(uif_formal_param((ast_t*)ast_data(ast), mask, &x) == LIT_NO_TYPES)
        return LIT_NO_TYPES;

      return LIT_CONSTRAINED;
    }

    case TK_TUPLETYPE:
      // Incorrect cardinality
      return LIT_NO_TYPES;

    case TK_NOMINAL:
      *out_uif_set = uifset_simple_type(ast, mask);
      return LIT_OK;

    default:
      assert(0);
      return LIT_NO_TYPES;
  }
}


// Determine the best set of UIF types for the given tuple union
static lit_result_t uif_tuple_union(ast_t* literals, ast_t** type_ptr)
{
  assert(literals != NULL);
  assert(ast_id(literals) == TK_TUPLE);
  assert(type_ptr != NULL);

  ast_t* type = *type_ptr;
  assert(type != NULL);
  assert(ast_id(type) == TK_UNIONTYPE);

  ast_t* best = NULL;  // Best unconstrained child type
  ast_t* constraints = ast_from(type, TK_UNIONTYPE); // Constrained child types
  ast_t* child;

  while((child = ast_pop(type)) != NULL)
  {
    switch(uif_expr(literals, &child))
    {
      case LIT_CONSTRAINED:
        // Child is constrained, add it to our list
        ast_add(constraints, child);
        break;

      case LIT_NO_TYPES:
        // Child has no legal types, discard it
        ast_free(child);
        break;

      case LIT_OK:
        // Child is a valid type, keep the best one
        best = best_uif_type(best, child, literals);
        break;
    }
  }

  if(best == NULL && ast_child(constraints) == NULL)
  {
    // No legal types in union
    ast_free(constraints);
    return LIT_NO_TYPES;
  }

  // Determine if we are constrained
  lit_result_t r = (ast_child(constraints) == NULL) ? LIT_OK : LIT_CONSTRAINED;

  // Add our best unconstrained type (if any) to our constrained list
  if(best != NULL)
    ast_add(constraints, best);

  if(ast_childcount(constraints) == 1)
  {
    // We only have a single child, replace union with it
    ast_t* t = ast_pop(constraints);
    ast_free(constraints);
    constraints = t;
  }

  // Replace union with our trimmed union (or single child)
  ast_replace(type_ptr, constraints);
  return r;
}


// Determine the best set of UIF types for the given tuple
static lit_result_t uif_tuple_value(ast_t* literals, ast_t* type)
{
  assert(literals != NULL);
  assert(ast_id(literals) == TK_TUPLE);
  assert(type != NULL);
  assert(ast_id(type) == TK_TUPLETYPE);

  lit_result_t result = LIT_OK;

  ast_t* lit_child = ast_child(literals);
  ast_t* type_child = ast_child(type);

  while(lit_child != NULL && type_child != NULL)
  {
    lit_result_t r = uif_expr(lit_child, &type_child);

    if(r == LIT_NO_TYPES) // Element is bad, discard whole tuple
      return LIT_NO_TYPES;

    if(r == LIT_CONSTRAINED)  // Element is constrained, so whole tuple is
      result = LIT_CONSTRAINED;

    lit_child = ast_sibling(lit_child);
    type_child = ast_sibling(type_child);
  }

  if(lit_child != NULL || type_child != NULL) // Cardinalities don't match
    return LIT_NO_TYPES;

  return result;
}


static lit_result_t uif_tuple(ast_t* literals, ast_t** type_ptr)
{
  assert(literals != NULL);
  assert(ast_type(literals) != NULL);
  assert(ast_id(ast_type(literals)) == TK_TUPLELITERAL);
  assert(type_ptr != NULL);

  ast_t* type = *type_ptr;
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return uif_tuple_union(literals, type_ptr);

    case TK_TUPLETYPE:
      return uif_tuple_value(literals, type);

    case TK_ISECTTYPE:
      ast_error(literals,
        "Literal inference not yet supported for intersect types");
      return LIT_NO_TYPES;

    default:
      ast_error(type, "Internal error: invalid literal tuple type node %s",
        ast_get_print(type));
      assert(0);
      return LIT_NO_TYPES;
  }
}


static lit_result_t uif_expr(ast_t* literals, ast_t** type_ptr)
{
  assert(literals != NULL);
  assert(ast_type(literals) != NULL);
  assert(type_ptr != NULL);

  ast_t* type = *type_ptr;
  assert(type != NULL);

  token_id literal_id = ast_id(ast_type(literals));

  switch(ast_id(ast_type(literals)))
  {
    case TK_TUPLELITERAL:
      // Tuple literal
      return uif_tuple(literals, type_ptr);

    case TK_NUMBERLITERAL:
    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
    {
      // Non-tuple literal
      int uif_set;
      lit_result_t r = uif_mono(type, uif_mask(literal_id), &uif_set);

      if(r == LIT_OK)  // Replace specified type with best legal type
        ast_replace(type_ptr, best_uif_in_set(uif_set, type));

      return r;
    }

    default:
      // Not a new literal, type must be compatible
      if(is_subtype(ast_type(literals), type))
        return LIT_OK;

      return LIT_NO_TYPES;
  }
}


// Propogate a non-tuple type coercion down the tree
static void propogate_coercion(ast_t* ast, ast_t* type)
{
  assert(ast != NULL);
  assert(type != NULL);

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    propogate_coercion(p, type);

  // Need to reprocess TK_DOTs to lookup functions
  // Have to set type before this
  if(ast_id(ast) == TK_DOT)
  {
    ast_result_t r = pass_expr(&ast, NULL);
    (void)r;
    assert(r == AST_OK);
  }

  if(is_type_literal(ast_type(ast)))
    ast_settype(ast, ast_dup(type));
}


// Apply the given type to the given literals
static void apply_type(ast_t* literals, ast_t* type)
{
  assert(literals != NULL);
  assert(type != NULL);

  switch(ast_id(ast_type(literals)))
  {
    case TK_NUMBERLITERAL:
    case TK_INTLITERAL:
    case TK_FLOATLITERAL:
      propogate_coercion(literals, type);
      break;

    case TK_TUPLELITERAL:
    {
      assert(ast_id(type) == TK_TUPLETYPE);

      ast_t* tuple_type = ast_from(literals, TK_TUPLETYPE);
      ast_t* prev_child = NULL;

      ast_t* lit_child = ast_child(literals);
      ast_t* type_child = ast_child(type);

      while(lit_child != NULL)
      {
        assert(type_child != NULL);

        apply_type(lit_child, type_child);
        ast_t* child_type = ast_type(lit_child);
        assert(child_type != NULL);
        child_type = ast_dup(child_type);

        if(prev_child == NULL)
          prev_child = ast_add(tuple_type, child_type);
        else
          prev_child = ast_add_sibling(prev_child, child_type);

        lit_child = ast_sibling(lit_child);
        type_child = ast_sibling(type_child);
      }

      assert(type_child == NULL);
      ast_settype(literals, tuple_type);
      break;
    }

    default:
      // Nothing to do for non-literals
      break;
  }
}


// Public methods

bool is_type_literal(ast_t* type)
{
  if(type == NULL)
    return false;

  token_id id = ast_id(type);
  return (id == TK_NUMBERLITERAL) || (id == TK_INTLITERAL) ||
    (id == TK_FLOATLITERAL) || (id == TK_OPERATORLITERAL) ||
    (id == TK_TUPLELITERAL);
}


bool coerce_literals(ast_t* literals, ast_t* target_type)
{
  assert(literals != NULL);

  ast_t* literals_type = ast_type(literals);

  if(!is_type_literal(literals_type)) // Not a literal, nothing to do
    return true;

  if(target_type == NULL)
  {
    // We're doing local and literal type inference together
    ast_error(literals, "cannot infer expression and variable types");
    return false;
  }

  // TODO(andy): canonicalise type
  ast_t* canonical_type = ast_dup(target_type);

  lit_result_t expr_result = uif_expr(literals, &canonical_type);

  if(expr_result == LIT_NO_TYPES)
  {
    ast_error(literals, "cannot coerce %s literal to suitable type",
      ast_get_print(literals_type));
    ast_free(canonical_type);
    return false;
  }

  if(expr_result == LIT_CONSTRAINED)
  {
    // We know there is a legal type, but we don't know exactly what until
    // reification. Store semi-processed type for later
    ast_add(literals_type, canonical_type);
    return true;
  }
  
  // We know the exact type, apply it to the literal tree
  apply_type(literals, canonical_type);
  ast_free(canonical_type);
  return true;
}


void reify_literals(ast_t* ast)
{
  assert(ast != NULL);

  // We only care about nodes whose type is a literal with a child
  ast_t* type = ast_type(ast);

  if(type == NULL || ast_child(type) == NULL || !is_type_literal(type))
    return;

  ast_t* canonical_type = ast_pop(type);

  lit_result_t expr_result = uif_expr(ast, &canonical_type);
  (void)expr_result;  // So release build doesn't complain
  assert(expr_result == LIT_OK);

  apply_type(ast, canonical_type);
  ast_free(canonical_type);
}



#if 0

/** When a literal is coerced to the type of an expression, we need to work out
what legal types that expression can be. This may be arbitarilly complex due to
unions, intersections and formal parameter constraints.

We keep a set of the possible UIF types, using simple bit flags. This is called
the free set. The bit flags we use are 1 shifted by the index of the UIF type
into _uif_types. We also have to keep a list of the possible formal parameter
constraints, which will be reified later.

The final type we choose can be any one from our constraints or free set. If we
have a choice we pick the "widest" type available, as defined in the
"_uif_types" list below. If that choice depends on a formal parameter
constraint then we keep a union of possiblities and only make the final
decision during codegen (which is after reification).

For constraints our deductions must work for every possible type that the
parameter is eventually bound to, including unions types. Therefore we say that
any constraint which is not a subtype of Number produces no legal values. For
constraints that satisfy this we determine which UIFs types a constraint may be
bound to by testing each individually.

Since literals can be assigned to variables with any capability we can safely
ignore capabilities throughout this process.
*/

typedef struct constraint_t
{
  int uif_set;
  ast_t* formal_param;
  struct constraint_t* next;
} constraint_t;


typedef struct literal_set_t
{
  int uif_free_set;
  constraint_t* constraints;
} literal_set_t;


static void free_literal_set(literal_set_t* set)
{
  if(set == NULL)
    return;

  constraint_t* p = set->constraints;
  while(p != NULL)
  {
    constraint_t* next = p->next;
    free(p);
    p = next;
  }

  free(set);
}

/*
static void literal_set_print(literal_set_t* set)
{
  if(set == NULL)
    return;

  printf("Set free: %x {\n", set->uif_free_set);

  for(constraint_t* p = set->constraints; p != NULL; p = p->next)
    printf("  Constraint %p %x\n", p->formal_param, p->uif_set);

  printf("}\n");
}
*/

bool is_type_literal(ast_t* type)
{
  if(type == NULL)
    return false;

  token_id id = ast_id(type);
  return (id == TK_NUMBERLITERAL) || (id == TK_INTLITERAL) ||
    (id == TK_FLOATLITERAL) || (id == TK_OPERATORLITERAL);
}


static bool propogate_coercion(ast_t* ast, ast_t* type)
{
  assert(ast != NULL);
  assert(type != NULL);

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    if(!propogate_coercion(p, type))
      return false;

  // Need to reprocess TK_DOTs to lookup functions
  // Have to set type before this since if
  if(ast_id(ast) == TK_DOT)
    return pass_expr(&ast, NULL) == AST_OK;

  if(is_type_literal(ast_type(ast)))
    ast_settype(ast, type);

  return true;
}


static const char* const _uif_types[] =
{
  "F64", "F32",
  "I128", "U128",
  "I64", "U64",
  "I32", "U32",
  "I16", "U16",
  "I8", "U8",
  NULL
};


// Generate the correct literal mask for the given ID
// Return 0 is given ID is not a literal
static int get_uif_mask(token_id id)
{
  switch(id)
  {
    case TK_NUMBERLITERAL: return 0xFFF;
    case TK_INTLITERAL: return 0xFFC;
    case TK_FLOATLITERAL: return 0x003;
    default: return 0;
  }
}


// Generate a literal set that satisfies the given type (which may be NULL)
static literal_set_t* make_literal_set(ast_t* type)
{
  literal_set_t* p = (literal_set_t*)malloc(sizeof(literal_set_t));
  p->uif_free_set = 0;
  p->constraints = NULL;

  if(type != NULL)
  {
    int set = 0;

    for(int i = 0; _uif_types[i] != NULL; i++)
    {
      ast_t* uif = type_builtin(type, _uif_types[i]);
      ast_setid(ast_childidx(uif, 3), TK_ISO);

      if(is_subtype(uif, type))
        set |= (1 << i);

      ast_free(uif);
    }

    p->uif_free_set = set;
  }

  return p;
}


// Generate a literal set for the given type parameter
static literal_set_t* make_constraint_set(ast_t* type_param)
{
  assert(type_param != NULL);
  assert(ast_id(type_param) == TK_TYPEPARAM);

  ast_t* constraint = ast_childidx(type_param, 1);
  assert(constraint != NULL);

  literal_set_t* p = (literal_set_t*)malloc(sizeof(literal_set_t));
  p->uif_free_set = 0;
  p->constraints = NULL;

  // If the constraint is not a subtype of Number then there are no legal types
  // in the set
  ast_t* number = type_builtin(type_param, "Number");
  bool is_number = is_subtype(constraint, number);
  ast_free(number);

  if(!is_number)  // The type param may have non UIF types
    return p;

  int uif_set = 0;

  for(int i = 0; _uif_types[i] != NULL; i++)
  {
    ast_t* uif = type_builtin(type_param, _uif_types[i]);
    ast_setid(ast_childidx(uif, 3), TK_VAL);

    BUILD(params, type_param, NODE(TK_TYPEPARAMS, TREE(ast_dup(type_param))));
    BUILD(args, type_param, NODE(TK_TYPEARGS, TREE(uif)));

    if(check_constraints(params, args, true))
      uif_set |= (1 << i);

    ast_free(args);
    ast_free(params);
  }

  if(((uif_set - 1) & uif_set) == 0)
  {
    // There is only 1 UIF type is the set (or possibly none), don't make a
    // constraint list
    p->uif_free_set = uif_set;
  }
  else
  {
    // UIF set contains more than one type, start a constraint list
    constraint_t* c = (constraint_t*)malloc(sizeof(constraint_t));
    c->formal_param = type_param;
    c->uif_set = uif_set;
    c->next = NULL;
    p->constraints = c;
  }

  return p;
}


// Add the given constraint to the given literal set, without duplicates
static void add_constraint(literal_set_t* set, constraint_t* constraint)
{
  assert(set != NULL);
  assert(constraint != NULL);

  for(constraint_t* p = set->constraints; p != NULL; p = p->next)
  {
    if(p->formal_param == constraint->formal_param)
    {
      // Constraint is already in this set
      free(constraint);
      return;
    }
  }

  // Constraint is not already in this set
  constraint->next = set->constraints;
  set->constraints = constraint;
}


/* Produce the union of 2 literal sets, both of which are destroyed.
 *
 * A literal set c, being union of a and b is determined as follows:
 *   The free set of c is the union of the free sets in a and b.
 *   The constraint set c is the union of the constraint sets of a and b.
 */
static literal_set_t* literal_set_union(literal_set_t* a, literal_set_t* b)
{
  assert(a != NULL);
  assert(b != NULL);

  // Merge constriants of b into a
  constraint_t* p = b->constraints;
  while(p != NULL)
  {
    constraint_t* next = p->next;
    add_constraint(a, p);
    p = next;
  }

  a->uif_free_set |= b->uif_free_set;
  free(b);
  return a;
}


/* Check if the given constraint is contained within the given literal set.
 *
 * Contained within is defined as being either in the literal set's constraints
 * list or having a uif set which matches the literal set's free types.
 */
static bool is_contained(literal_set_t* set, constraint_t* constraint)
{
  assert(set != NULL);
  assert(constraint != NULL);

  if(constraint->uif_set == set->uif_free_set)
    return true;

  for(constraint_t* p = set->constraints; p != NULL; p=p->next)
  {
    if(p->formal_param == constraint->formal_param)
      return true;
  }

  return false;
}


/* Produce the intersection of 2 literal sets, both of which are destroyed
 *
 * A literal set c, being intersection of a and b is determined as follows:
 *   The free set of c is the intersection of the free sets in a and b.
 *   The constraint set c is those constraints in either of a and b that are
 *   contained within the other. (See above for definitino of contained.)
 */
static literal_set_t* literal_set_intersect(literal_set_t* a, literal_set_t* b)
{
  assert(a != NULL);
  assert(b != NULL);

  literal_set_t* c = (literal_set_t*)malloc(sizeof(literal_set_t));
  c->uif_free_set = a->uif_free_set & b->uif_free_set;
  c->constraints = NULL;

  // Add constraints from set a
  constraint_t* p = a->constraints;
  while(p != NULL)
  {
    constraint_t* next = p->next;

    if(is_contained(b, p))
      add_constraint(c, p);
    else
      free(p);

    p = next;
  }

  a->constraints = NULL;

  // Add constraints from set b
  p = b->constraints;
  while(p != NULL)
  {
    constraint_t* next = p->next;

    if(is_contained(a, p))
      add_constraint(c, p);
    else
      free(p);

    p = next;
  }

  free(a);
  free(b);
  return c;
}


// Determine the literal set for the given type
static literal_set_t* determine_literal_set(ast_t* type)
{
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return make_literal_set(type);

    case TK_UNIONTYPE:
    {
      literal_set_t* r = determine_literal_set(ast_child(type));

      for(ast_t* p = ast_childidx(type, 1); p != NULL; p = ast_sibling(p))
        r = literal_set_union(r, determine_literal_set(p));

      return r;
    }

    case TK_ISECTTYPE:
    {
      literal_set_t* r = determine_literal_set(ast_child(type));

      for(ast_t* p = ast_childidx(type, 1); p != NULL; p = ast_sibling(p))
        r = literal_set_intersect(r, determine_literal_set(p));

      return r;
    }

    case TK_TUPLETYPE:  // A literal isn't a tuple
      return make_literal_set(NULL);

    case TK_ARROW:
      // Since we don't care about capabilities we just use the type to the
      // right of the arrow
      return make_constraint_set(ast_childidx(type, 1));

    case TK_TYPEPARAMREF:
      return make_constraint_set((ast_t*)ast_data(type));

    default:
      break;
  }

  printf("Oops\n");
  ast_print(type);
  assert(0);
  return NULL;
}


// Determine the index of the "best" type in the given set.
// Return <0 for no types in set.
static int best_type(int actual_set, int required_mask)
{
  int available = actual_set & required_mask;

  if(available == 0)
    return -1;

  for(int i = 0; true; i++)
  {
    if((available & 1) != 0)
      return i;

    available >>= 1;
  }
}


// Generate the type for the given literal set restricted to the given literal
// id. Return false on error.
static bool build_literal_type(literal_set_t* set, int uif_mask,
  ast_t* ast, ast_t* target_type)
{
  assert(set != NULL);
  assert(ast != NULL);
  assert(target_type != NULL);

  // Determine the type from the free set (if any) and a mask of the valid
  // types that are better than that
  int best_free = best_type(set->uif_free_set, uif_mask);
  int better_than_free = uif_mask;

  if(best_free >= 0)
    better_than_free = ((1 << best_free) - 1) & uif_mask;

  // Check if any constraints can have a better type than the free set
  for(constraint_t* p = set->constraints; p != NULL; p = p->next)
  {
    if((p->uif_set & better_than_free) != 0)
    {
      // Constraints can be better than the free set, so we need them
      ast_t* type = ast_type(ast);
      assert(type != NULL);
      assert(ast_id(type) == TK_NUMBERLITERAL ||
        ast_id(type) == TK_INTLITERAL || ast_id(type) == TK_FLOATLITERAL);
      assert(ast_childcount(type) == 0);
      ast_add(type, target_type);
      return propogate_coercion(ast, type);
    }
  }

  if(best_free < 0) // No legal types
    return false;

  // No constraints can be better than the free set, so ignore them
  ast_t* actual_type = type_builtin(ast, _uif_types[best_free]);
  AST_GET_CHILDREN(actual_type, ignore0, ignore1, ignore2, cap, ephemeral);
  assert(cap != NULL);
  ast_setid(cap, TK_VAL);
  return propogate_coercion(ast, actual_type);
}


static bool coerce_tuple(ast_t* ast, ast_t* target_type,
  bool* out_type_changed)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_TUPLE);

  if(out_type_changed != NULL)
    *out_type_changed = false;

  ast_t* tuple_type = ast_type(ast);
  assert(tuple_type != NULL);
  assert(ast_id(tuple_type) == TK_TUPLETYPE);

  ast_t* child = ast_child(ast);
  ast_t* child_expr_type = ast_child(tuple_type);
  ast_t* child_target_type;

  if(target_type == NULL)
    child_target_type = NULL;
  else
    child_target_type = ast_child(target_type);

  while(child != NULL)
  {
    assert(ast_id(child) == TK_SEQ);
    assert(child_expr_type != NULL);
    ast_t* inner_child = ast_child(child);

    bool type_changed = false;
    if(!coerce_literals(inner_child, child_target_type, &type_changed))
      return false;

    if(type_changed)
    {
      ast_replace(&child_expr_type, ast_type(inner_child));

      if(out_type_changed != NULL)
        *out_type_changed = true;
    }

    child = ast_sibling(child);
    child_expr_type = ast_sibling(child_expr_type);

    if(child_target_type != NULL)
      child_target_type = ast_sibling(child_target_type);
  }

  assert(child_expr_type == NULL);
  return true;
}


bool coerce_literals(ast_t* ast, ast_t* target_type, bool* out_type_changed)
{
  assert(ast != NULL);

  if(out_type_changed != NULL)
    *out_type_changed = false;

  if(ast_id(ast) == TK_TUPLE && ast_type(ast) != NULL &&
    ast_id(ast_type(ast)) == TK_TUPLETYPE)
    return coerce_tuple(ast, target_type, out_type_changed);

  ast_t* expr_type = ast_type(ast);

  if(!is_type_literal(expr_type))
    return true;

  if(target_type == NULL)
  {
    // We're doing local and literal type inference together
    // TODO(andy): Maybe a better error message?
    ast_error(ast, "cannot determine type of literal (7)");
    return false;
  }

  literal_set_t* set = determine_literal_set(target_type);
  if(set == NULL)
    return false;

  int mask = get_uif_mask(ast_id(ast_type(ast)));
  bool r = build_literal_type(set, mask, ast, target_type);

  if(!r)
    ast_error(ast, "cannot coerce %s literal to suitable type",
      ast_get_print(ast_type(ast)));

  if(out_type_changed != NULL)
    *out_type_changed = true;

  free_literal_set(set);
  return r;
}

#endif


bool coerce_literal_member(ast_t* ast)
{
  assert(ast != NULL);

  if(ast_id(ast_parent(ast)) != TK_CALL)
  {
    ast_error(ast_child(ast), "cannot determine type of literal (5)");
    return false;
  }

  ast_settype(ast, ast_from(ast, TK_OPERATORLITERAL));
  return true;
}


static struct
{
  const char* name;
  int arg_count;
  bool is_int_only;
  bool can_propogate_literal;
} _operator_fns[] =
{
  { "add", 1, false, true },
  { "sub", 1, false, true },
  { "mul", 1, false, true },
  { "div", 1, false, true },
  { "mod", 1, false, true },
  { "neg", 0, false, true },
  { "shl", 1, true, true },
  { "shr", 1, true, true },
  { "and_", 1, true, true },
  { "or_", 1, true, true },
  { "xor_", 1, true, true },
  { "not_", 0, true, true },
  { "eq", 1, false, false },
  { "ne", 1, false, false },
  { "lt", 1, false, false },
  { "le", 1, false, false },
  { "gt", 1, false, false },
  { "ge", 1, false, false },
  { NULL, 0, false, false }  // Terminator
};


// Combine the types of the specified arguments
static bool combine_arg_types(int fn_index, ast_t* ast, ast_t* left,
  ast_t* right)
{
  assert(ast != NULL);
  assert(left != NULL);

  ast_t* left_type = ast_type(left);

  if(_operator_fns[fn_index].arg_count == 0)
  {
    // Unary operator, result is type of argument
    ast_settype(ast, left_type);
    return true;
  }

  assert(right != NULL);
  ast_t* right_type = ast_type(right);

  if(!is_type_literal(right_type))
  {
    // Rhs is not literal, coerce lhs to type of rhs
    if(!coerce_literals(left, right_type))
      return false;

    ast_settype(ast, ast_type(left));
    return true;
  }

  // Lhs and rhs are both literals
  if(!_operator_fns[fn_index].can_propogate_literal)
  {
    ast_error(right, "cannot determine type of literal (4)");
    return false;
  }

  token_id left_id = ast_id(left_type);
  token_id right_id = ast_id(right_type);

  if((left_id == TK_INTLITERAL && right_id == TK_FLOATLITERAL) ||
    (left_id == TK_FLOATLITERAL && right_id == TK_INTLITERAL))
  {
    ast_error(right, "cannot determine type of literal (3)");
    return false;
  }

  if(left_id == TK_NUMBERLITERAL)
    ast_settype(ast, right_type);
  else
    ast_settype(ast, left_type);

  return true;
}


bool coerce_literal_operator(ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_CALL);

  AST_GET_CHILDREN(ast, positional, named, lhs);
  assert(lhs != NULL);
  assert(positional != NULL);
  assert(named != NULL);

  ast_t* left_type = ast_type(lhs);
  if(left_type == NULL || ast_id(left_type) != TK_OPERATORLITERAL)
    return true; // Not a literal operator

  assert(ast_id(lhs) == TK_DOT);
  AST_GET_CHILDREN(lhs, left, call_id);
  assert(call_id != NULL);
  assert(ast_id(call_id) == TK_ID);

  // Determine which operator this call is (if any)
  const char* call_name = ast_name(call_id);
  int fn;

  for(fn = 0; _operator_fns[fn].name != NULL; fn++)
  {
    if(strcmp(call_name, _operator_fns[fn].name) == 0)
      break;
  }

  if(_operator_fns[fn].name == NULL || // Not an operator function
    ast_id(named) != TK_NONE || // Named arguments present
    ast_childcount(positional) != _operator_fns[fn].arg_count) // Bad arg count
  {
    ast_error(ast, "cannot determine type of literal (1)");
    return false;
  }

  if(!combine_arg_types(fn, ast, left, ast_child(positional)))
    return false;

  if(_operator_fns[fn].is_int_only)
  {
    if(ast_id(ast_type(ast)) == TK_NUMBERLITERAL)
      ast_settype(ast, ast_from(ast, TK_INTLITERAL));

    if(ast_id(ast_type(ast)) != TK_INTLITERAL)
    {
      ast_error(ast, "cannot apply %s to floats", call_name);
      return false;
    }
  }

  // Need to reprocess TK_DOT to lookup functions
  return pass_expr(&lhs, NULL) == AST_OK;
}
