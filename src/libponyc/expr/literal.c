#include "literal.h"
#include "../ast/token.h"
#include "../ds/stringtab.h"
#include "../pass/names.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/cap.h"
#include <string.h>
#include <assert.h>

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
  // TODO: If in a recover expression, may not have access to "this".
  // Or we could lower it to tag, since it can't be assigned to. If in a
  // constructor, lower it to tag if not all fields are defined.
  ast_t* type = type_for_this(ast, cap_for_receiver(ast), false);
  ast_settype(ast, type);

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

  if(ast_id(ast) != TK_NOMINAL)
    return true;

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

  return check_constraints(typeparams, typeargs);
}

bool expr_fun(ast_t* ast)
{
  ast_t* type = ast_childidx(ast, 4);
  ast_t* can_error = ast_sibling(type);
  ast_t* body = ast_sibling(can_error);

  if(ast_id(body) == TK_NONE)
    return true;

  ast_t* def = ast_enclosing_type(ast);
  bool is_trait = ast_id(def) == TK_TRAIT;

  // if specified, body type must match return type
  ast_t* body_type = ast_type(body);

  if(body_type == NULL)
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body always results in an error");
    ast_error(last, "function body expression is here");
    return false;
  }

  if(ast_id(body_type) == TK_COMPILER_INTRINSIC)
    return true;

  // check partial functions
  if(ast_id(can_error) == TK_QUESTION)
  {
    // if a partial function, check that we might actually error
    if(!is_trait && !ast_canerror(body))
    {
      ast_error(can_error, "function body is not partial but the function is");
      return false;
    }
  } else {
    // if not a partial function, check that we can't error
    if(ast_canerror(body))
    {
      ast_error(can_error, "function body is partial but the function is not");
      return false;
    }
  }

  if((ast_id(ast) == TK_FUN) && !is_subtype(body_type, type))
  {
    ast_t* last = ast_childlast(body);
    ast_error(type, "function body isn't a subtype of the result type");
    ast_error(last, "function body expression is here");
    return false;
  }

  return true;
}


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


static bool is_type_arith_literal(ast_t* ast)
{
  if(ast == NULL)
    return false;

  token_id id = ast_id(ast);
  return (id == TK_INTLITERAL) || (id == TK_FLOATLITERAL);
}


static void propogate_coercion(ast_t* ast, ast_t* type)
{
  assert(ast != NULL);
  assert(type != NULL);

  if(!is_type_arith_literal(ast_type(ast)))
    return;

  ast_settype(ast, type);

  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
    propogate_coercion(p, type);
}


#define NUMBER_UIF_MASK  0xFFF
#define FLOAT_UIF_MASK   0x003
#define INTEGER_UIF_MASK 0xFFC

static const char* const _uif_types[] =
{
  "F64", "F32",
  "I128", "U128",
  "I64", "U64",
  "I32", "U32",
  "I16", "U16",
  "I8", "U8",
  NULL
//  "U8", "I8", "U16", "I16", "U32", "I32", "U64", "I64", "U128", "I128",
//  "F32", "F64", NULL
};


// Report the UIF mask to use for the specified literal type
static int uif_mask(token_id literal_id)
{
  switch(literal_id)
  {
    case TK_INTLITERAL: return NUMBER_UIF_MASK;
    case TK_FLOATLITERAL: return FLOAT_UIF_MASK;

    default:
      assert(0);
      return 0;
  }
}


// Generate the uif set that satisfies the given type
static int make_uif_set(ast_t* type)
{
  assert(type != NULL);

  int set = 0;

  for(int i = 0; _uif_types[i] != NULL; i++)
  {
    ast_t* uif = type_builtin(type, _uif_types[i]);

    if(is_subtype(uif, type))
      set |= (1 << i);

    ast_free(uif);
  }

  return set;
}


// Generate a literal set that satisfies the given type (which may be NULL)
static literal_set_t* make_literal_set(ast_t* type)
{
  literal_set_t* p = (literal_set_t*)malloc(sizeof(literal_set_t));
  p->uif_free_set = 0;
  p->constraints = NULL;

  if(type != NULL)
    p->uif_free_set = make_uif_set(type);

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

  if(!is_number)  // The type param may not have any UIF types
    return p;

  int uif_set = make_uif_set(constraint);

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
    c->uif_set = make_uif_set(constraint);
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
    case TK_STRUCTURAL:
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
      assert(ast_id(type) == TK_INTLITERAL || ast_id(type) == TK_FLOATLITERAL);
      assert(ast_childcount(type) == 0);
      ast_add(type, target_type);
      propogate_coercion(ast, type);
      return true;
    }
  }

  if(best_free < 0) // No legal types
    return false;

  // No constraints can be better than the free set, so ignore them
  ast_t* actual_type = type_builtin(ast, _uif_types[best_free]);
  AST_GET_CHILDREN(actual_type, ignore0, ignore1, ignore2, cap, ephemeral);
  assert(cap != NULL);
  ast_setid(cap, TK_ISO);
  assert(ephemeral != NULL);
  ast_setid(ephemeral, TK_HAT);
  propogate_coercion(ast, actual_type);
  return true;
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
  assert(target_type != NULL);

  if(out_type_changed != NULL)
    *out_type_changed = false;

  if(ast_id(ast) == TK_TUPLE)
    return coerce_tuple(ast, target_type, out_type_changed);

  ast_t* expr_type = ast_type(ast);
  assert(expr_type != NULL);

  if(!is_type_arith_literal(expr_type))
    return true;

  literal_set_t* set = determine_literal_set(ast_type(ast));
  if(set == NULL)
    return false;

  bool r = build_literal_type(set, uif_mask(ast_id(ast)), ast, target_type);

  if(!r)
    ast_error(ast, "cannot determine type of literal");

  if(out_type_changed != NULL)
    *out_type_changed = true;

  free_literal_set(set);
  return r;
}


ast_t* concrete_literal(ast_t* type)
{
  assert(type != NULL);
  
  int mask;

  switch(ast_id(type))
  {
    case TK_INTLITERAL: mask = NUMBER_UIF_MASK; break;
    case TK_FLOATLITERAL: mask = FLOAT_UIF_MASK; break;

    default: return type;
  }


  for(int i = 0; _uif_types[i] != NULL; i++)
  {
    if((i & mask) != 0)
    {
      ast_t* uif = type_builtin(type, _uif_types[i]);

      if(is_subtype(uif, type))
      {
        ast_t* cap = ast_childidx(uif, 3);
        assert(cap != NULL);
        ast_setid(cap, TK_TAG);

        ast_add(type, uif); // Ensuire uif is eventually freed
        return uif;
      }

      ast_free(uif);
    }
  }

  assert(0);
  return NULL;
}
