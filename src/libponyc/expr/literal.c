#include "literal.h"
#include "reference.h"
#include "../pass/expr.h"
#include "../type/subtype.h"
#include "../type/assemble.h"
#include "../type/alias.h"
#include "../type/reify.h"
#include "../ast/token.h"
#include "../ast/stringtab.h"
#include <string.h>
#include <assert.h>


#define UIF_ERROR       -1
#define UIF_NO_TYPES    0
#define UIF_INT_MASK    0x03FF
#define UIF_ALL_TYPES   0x0FFF
#define UIF_CONSTRAINED 0x1000
#define UIF_COUNT       12

static const char* _str_uif_types[UIF_COUNT] =
{
  "U8", "U16", "U32", "U64", "U128",
  "I8", "I16", "I32", "I64", "I128",
  "F32", "F64"
};

typedef struct uif_type_info_t
{
  ast_t* type;
  const char* name;
  bool valid_for_float;
} uif_type_info_t;


// Forward declarations
static int uifset(ast_t* type, ast_t** formal);

static bool coerce_literal_to_type(ast_t* literal_expr, ast_t* target_type,
  uif_type_info_t* cached_type);


bool expr_literal(ast_t* ast, const char* name)
{
  ast_t* type = type_builtin(ast, name);

  if(type == NULL)
    return false;

  ast_settype(ast, type);
  return true;
}


void make_literal_type(ast_t* ast)
{
  ast_t* type = ast_from(ast, TK_LITERAL);
  ast_settype(ast, type);
}


bool is_type_literal(ast_t* type)
{
  if(type == NULL)
    return false;

  return ast_id(type) == TK_LITERAL;
}


// Determine the UIF types that satisfy the given "simple" type.
// Here a simple type is defined as a non-tuple type that does not depend on
// any formal parameters.
static int uifset_simple_type(ast_t* type)
{
  assert(type != NULL);

  int set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    ast_t* uif = type_builtin(type, _str_uif_types[i]);
    ast_setid(ast_childidx(uif, 3), TK_VAL);

    if(is_subtype(uif, type))
      set |= (1 << i);

    ast_free(uif);
  }

  return set;
}


// Determine the UIF types that the given formal parameter may be
static int uifset_formal_param(ast_t* type_param_ref, ast_t** formal)
{
  assert(type_param_ref != NULL);
  assert(ast_id(type_param_ref) == TK_TYPEPARAMREF);

  ast_t* type_param = (ast_t*)ast_data(type_param_ref);

  assert(type_param != NULL);
  assert(ast_id(type_param) == TK_TYPEPARAM);
  assert(formal != NULL);

  ast_t* constraint = ast_childidx(type_param, 1);
  assert(constraint != NULL);

  // If the constraint is not a subtype of (Real[A] & Number) then there are no
  // legal types in the set
  ast_t* number = type_builtin(type_param, "Number");
  ast_t* real = type_builtin(type_param, "Real");
  ast_setid(ast_childidx(real, 3), TK_BOX);

  ast_t* p_ref = ast_childidx(real, 2);
  REPLACE(&p_ref,
    NODE(TK_TYPEARGS,
      NODE(TK_TYPEPARAMREF, DATA(type_param)
        ID(ast_name(ast_child(type_param))) NODE(TK_VAL) NONE)));

  bool is_real = is_subtype(constraint, real);
  bool is_number = is_subtype(constraint, number);
  ast_free(number);
  ast_free(real);

  if(!is_real || !is_number)
    // The formal param is not a subset of (Real[A] & Number)
    return UIF_NO_TYPES;

  int uif_set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    ast_t* uif = type_builtin(type_param, _str_uif_types[i]);

    BUILD(params, type_param, NODE(TK_TYPEPARAMS, TREE(ast_dup(type_param))));
    BUILD(args, type_param, NODE(TK_TYPEARGS, TREE(uif)));

    if(check_constraints(params, args, false))
      uif_set |= (1 << i);

    ast_free(args);
    ast_free(params);
  }

  if(uif_set == 0)  // No legal types
    return UIF_NO_TYPES;

  // Given formal parameter is legal to coerce to
  if(*formal != NULL && *formal != type_param)
  {
    ast_error(type_param_ref,
      "Cannot infer a literal type with multiple formal parameters");
    return UIF_ERROR;
  }

  *formal = type_param;
  return uif_set | UIF_CONSTRAINED;
}


// Determine the UIF types that the given non-tuple union type may be
static int uifset_union(ast_t* type, ast_t** formal)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_UNIONTYPE);

  int uif_set = 0;

  // Process all elements of the union
  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(p, formal);

    if(r == UIF_ERROR)  // Propogate errors
      return UIF_ERROR;

    bool child_valid = (r != UIF_NO_TYPES);
    bool child_formal = ((r & UIF_CONSTRAINED) != 0);
    bool others_valid = ((uif_set & UIF_ALL_TYPES) != 0);
    bool others_formal = ((uif_set & UIF_CONSTRAINED) != 0);

    if(child_valid && others_valid && (child_formal != others_formal))
    {
      // We're unioning a formal parameter and a UIF type, not allowed
      return UIF_ERROR;
    }
    
    uif_set |= r;
  }

  return uif_set;
}


// Determine the UIF types that the given non-tuple intersection type may be
static int uifset_intersect(ast_t* type, ast_t** formal)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_ISECTTYPE);

  int uif_set = UIF_ALL_TYPES;
  int constraint = 0;

  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(p, formal);

    if(r == UIF_ERROR)  // Propogate errors
      return UIF_ERROR;

    if((r & UIF_CONSTRAINED) != 0)
    {
      // We have a formal parameter
      constraint = r;
    }
    else
    {
      uif_set |= r;
    }
  }

  if(constraint != 0)
  {
    // We had a formal parameter
    int constraint_set = constraint & UIF_ALL_TYPES;

    if((constraint_set & uif_set) != constraint_set)
      // UIF type limits formal parameter types, no UIF guaranteed
      return UIF_NO_TYPES;

    return constraint;
  }

  return uif_set;
}


// Determine the UIF types that the given non-tuple type may be
static int uifset(ast_t* type, ast_t** formal)
{
  assert(type != NULL);
  assert(formal != NULL);

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return uifset_union(type, formal);

    case TK_ISECTTYPE:
      return uifset_intersect(type, formal);

    case TK_ARROW:
      // Since we don't care about capabilities we can just use the rhs
      assert(ast_id(ast_childidx(type, 1)) == TK_NOMINAL);
      return uifset(ast_childidx(type, 1), formal);

    case TK_TYPEPARAMREF:
      return uifset_formal_param(type, formal);

    case TK_TUPLETYPE:
      // Incorrect cardinality
      return UIF_NO_TYPES;

    case TK_NOMINAL:
      return uifset_simple_type(type);

    default:
      ast_error(type, "Internal error: uif type, node %d", ast_id(type));
      assert(0);
      return UIF_ERROR;
  }
}


// Fill the given UIF type cache
static bool uif_type_fill_cache(ast_t* type, uif_type_info_t* cached_type)
{
  assert(type != NULL);
  assert(cached_type != NULL);
  assert(cached_type->type == NULL);

  ast_t* formal = NULL;
  int r = uifset(type, &formal);

  if(r == UIF_ERROR || r == UIF_NO_TYPES)
  {
    ast_error(type, "Could not infer literal type");
    return false;
  }

  if((r & UIF_CONSTRAINED) != 0)
  {
    // Type is a formal parameter
    assert(formal != NULL);
    const char* name = ast_name(ast_child(formal));

    BUILD(uif_type, type,
      NODE(TK_TYPEPARAMREF, DATA(formal)
        ID(name) NODE(TK_VAL) NONE));

    cached_type->type = uif_type;
    cached_type->valid_for_float = ((r & UIF_INT_MASK) == 0);
    cached_type->name = name;
    return true;
  }

  // Type is one or more UIFs
  for(int i = 0; i < UIF_COUNT; i++)
  {
    if(r == (1 << i))
    {
      cached_type->valid_for_float = (((1 << i) & UIF_INT_MASK) == 0);
      cached_type->type = type_builtin(type, _str_uif_types[i]);
      cached_type->name = _str_uif_types[i];
      return true;
    }
  }

  ast_error(type, "Multiple possible types for literal");
  return false;
}


// Assign a UIF type from the given target type to the given AST
static bool get_uif_type(ast_t* literal, ast_t* target_type,
  uif_type_info_t* cached_type, bool require_float)
{
  assert(literal != NULL);
  assert(cached_type != NULL);

  if(cached_type->type == NULL)
  {
    // This is the first time we've needed this type, find it
    if(!uif_type_fill_cache(target_type, cached_type))
      return false;
  }

  assert(cached_type->type != NULL);

  if(require_float && !cached_type->valid_for_float)
  {
    ast_error(literal, "Inferred possibly integer type %s for float literal",
      cached_type->name);
    return false;
  }

  ast_settype(literal, cached_type->type);
  return true;
}


// Extract a copy of the specified element of tuple elements of the correct
// cardinality of the given type. NULL for none
static ast_t* extract_tuple_element(ast_t* type, size_t cardinality,
  size_t index)
{
  if(type == NULL)
    return NULL;

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* compound_type = NULL;

      // Process each element, building up a union or intersection
      for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
      {
        ast_t* t = extract_tuple_element(p, cardinality, index);

        if(t != NULL)
        {
          if(ast_id(type) == TK_UNIONTYPE)
            compound_type = type_union(compound_type, t);
          else
            compound_type = type_isect(compound_type, t);
        }
      }

      return compound_type;
    }

    case TK_TUPLETYPE:
      if(ast_childcount(type) != cardinality)
        return NULL;

      return ast_dup(ast_childidx(type, index));

    case TK_TYPEPARAMREF:
      // Although a type parameter can have a tuple type, the tuple elements
      // have no useful constraint, so they are not relevant to us
      return NULL;

    case TK_ARROW:
    case TK_NOMINAL:
      return NULL;

    case TK_ARRAY:
      // TODO
      return NULL;

    default:
      ast_error(type, "Internal error: tuple type extraction, node %d",
        ast_id(type));
      assert(0);
      return NULL;
  }
}


// Coerce a literal tuple to be the specified target type
static bool coerce_tuple(ast_t* literal_expr, ast_t* target_type)
{
  assert(literal_expr != NULL);
  assert(ast_id(literal_expr) == TK_TUPLE);

  size_t cardinality = ast_childcount(literal_expr);
  size_t i = 0;
  ast_t* tuple_type = ast_from(literal_expr, TK_TUPLETYPE);
  ast_settype(literal_expr, tuple_type);

  // Process each tuple element separately
  for(ast_t* p = ast_child(literal_expr); p != NULL; p = ast_sibling(p))
  {
    ast_t* child_lit_type = ast_type(p);
    assert(child_lit_type != NULL);

    if(ast_id(child_lit_type) == TK_LITERAL)
    {
      // This element is a literal, get the type element and coerce to it
      ast_t* child_tuple_type = extract_tuple_element(target_type,
        cardinality, i);

      bool r = coerce_literals(p, child_tuple_type);
      ast_free_unattached(child_tuple_type);

      if(!r)
        return false;
    }

    ast_append(tuple_type, ast_type(p));

    i++;
  }

  return true;
}


// Coerce a literal control block to be the specified target type
static bool coerce_control_block(ast_t* literal_expr, ast_t* target_type,
  uif_type_info_t* cached_type)
{
  assert(literal_expr != NULL);

  ast_t* lit_type = ast_type(literal_expr);
  assert(lit_type != NULL);
  assert(ast_id(lit_type) == TK_LITERAL);

  ast_t* block_type = ast_type(lit_type);

  for(ast_t* p = ast_child(lit_type); p != NULL; p = ast_sibling(p))
  {
    assert(ast_id(p) == TK_LITERALBRANCH);
    ast_t* branch = (ast_t*)ast_data(p);
    assert(branch != NULL);

    if(!coerce_literal_to_type(branch, target_type, cached_type))
    {
      ast_free_unattached(block_type);
      return false;
    }

    block_type = type_union(block_type, ast_type(branch));
  }

  ast_settype(literal_expr, block_type);
  return true;
}


// Coerce a literal expression to given tuple or non-tuple types
static bool coerce_literal_to_type(ast_t* literal_expr, ast_t* target_type,
  uif_type_info_t* cached_type)
{
  assert(literal_expr != NULL);

  ast_t* lit_type = ast_type(literal_expr);

  if(lit_type == NULL ||
    (ast_id(lit_type) != TK_LITERAL && ast_id(lit_type) != TK_OPERATORLITERAL))
    // Not a literal
    return true;

  if(ast_child(lit_type) != NULL) // Control block literal
    return coerce_control_block(literal_expr, target_type, cached_type);

  switch(ast_id(literal_expr))
  {
    case TK_TUPLE:  // Tuple literal
      return coerce_tuple(literal_expr, target_type);

    case TK_INT:
      return get_uif_type(literal_expr, target_type, cached_type, false);

    case TK_FLOAT:
      return get_uif_type(literal_expr, target_type, cached_type, true);

    case TK_SEQ:
    {
      // Only coerce the last expression in the sequence
      ast_t* last = ast_childlast(literal_expr);
      if(!coerce_literal_to_type(last, target_type, cached_type))
        return false;

      ast_settype(literal_expr, ast_type(last));
      return true;
    }

    case TK_CALL:
    {
      AST_GET_CHILDREN(literal_expr, positional, named, receiver);
      ast_t* arg = ast_child(positional);

      if(!coerce_literal_to_type(receiver, target_type, cached_type))
        return false;

      if(arg != NULL && !coerce_literal_to_type(arg, target_type, cached_type))
        return false;

      ast_settype(literal_expr, ast_type(ast_child(receiver)));
      return true;
    }

    case TK_DOT:
      if(!coerce_literal_to_type(ast_child(literal_expr), target_type,
        cached_type))
        return false;
      
      // We've determined the type of an operator, need to look up the function
      ast_settype(literal_expr, NULL);
      return (pass_expr(&literal_expr, NULL) == AST_OK);

    default:
      ast_error(literal_expr, "Internal error, coerce_literal_to_type node %s",
        ast_get_print(literal_expr));
      assert(0);
      return false;
  }
}


bool coerce_literals(ast_t* literal_expr, ast_t* target_type)
{
  assert(literal_expr != NULL);

  if(ast_id(literal_expr) == TK_NONE)
    return true;

  ast_t* lit_type = ast_type(literal_expr);

  if(lit_type != NULL && ast_id(lit_type) != TK_LITERAL &&
    ast_id(lit_type) != TK_OPERATORLITERAL)
    return true;

  uif_type_info_t cached_type = { NULL, NULL, false };
  return coerce_literal_to_type(literal_expr, target_type, &cached_type);
}


typedef struct lit_op_info_t
{
  const char* name;
  size_t arg_count;
  bool can_propogate_literal;
} lit_op_info_t;

static lit_op_info_t _operator_fns[] =
{
  { "add", 1, true },
  { "sub", 1, true },
  { "mul", 1, true },
  { "div", 1, true },
  { "mod", 1, true },
  { "neg", 0, true },
  { "shl", 1, true },
  { "shr", 1, true },
  { "op_and", 1, true },
  { "op_or", 1, true },
  { "op_xor", 1, true },
  { "op_not", 0, true },
  { "eq", 1, false },
  { "ne", 1, false },
  { "lt", 1, false },
  { "le", 1, false },
  { "gt", 1, false },
  { "ge", 1, false },
  { NULL, 0, false }  // Terminator
};


static lit_op_info_t* lookup_literal_op(const char* name)
{
  for(int i = 0; _operator_fns[i].name != NULL; i++)
    if(strcmp(name, _operator_fns[i].name) == 0)
      return &_operator_fns[i];

  return NULL;
}


// Unify all the branches of the given AST to the same type
static bool unify(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* type = ast_type(ast);
  assert(type != NULL);

  if(ast_id(type) != TK_LITERAL) // Not literal, nothing to unify
    return true;

  ast_t* non_literal = (ast_t*)ast_data(type);

  if(non_literal != NULL)
  {
    // Type has a non-literal element, coerce literals to that
    return coerce_literals(ast, non_literal);
  }

  // Still a pure literal
  return true;
}


bool literal_member_access(ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_DOT || ast_id(ast) == TK_TILDE);

  AST_GET_CHILDREN(ast, receiver, name_node);

  if(!unify(receiver))
    return false;

  ast_t* recv_type = ast_type(receiver);
  assert(recv_type != NULL);

  if(ast_id(recv_type) != TK_LITERAL) // Literals resolved
    return true;

  // Receiver is a pure literal expression
  // Look up member name
  assert(ast_id(name_node) == TK_ID);
  const char* name = ast_name(name_node);
  lit_op_info_t* op = lookup_literal_op(name);

  if(op == NULL || ast_id(ast_parent(ast)) != TK_CALL)
  {
    ast_error(ast, "Cannot look up member %s on a literal", name);
    return false;
  }

  // This is a literal operator
  ast_t* op_type = ast_from(ast, TK_OPERATORLITERAL);
  ast_setdata(op_type, (void*)op);
  ast_settype(ast, op_type);
  return true;
}


bool literal_call(ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_CALL);

  AST_GET_CHILDREN(ast, positional_args, named_args, receiver);

  ast_t* recv_type = ast_type(receiver);
  assert(recv_type != NULL);

  if(ast_id(recv_type) == TK_LITERAL)
  {
    ast_error(ast, "Cannot call a literal");
    return false;
  }

  if(ast_id(recv_type) != TK_OPERATORLITERAL) // Nothing to do
    return true;

  lit_op_info_t* op = (lit_op_info_t*)ast_data(recv_type);
  assert(op != NULL);

  // TODO: Should we allow named arguments?
  ast_t* arg = ast_child(positional_args);

  if(arg != NULL)
  {
    if(!unify(arg))
      return false;

    ast_t* arg_type = ast_type(arg);
    assert(arg_type != NULL);

    if(ast_id(arg_type) != TK_LITERAL)  // Apply argument type to receiver
      return coerce_literals(receiver, arg_type);

    if(!op->can_propogate_literal)
    {
      ast_error(ast, "Cannot infer operand type");
      return false;
    }
  }

  size_t arg_count = ast_childcount(positional_args);

  if(op->arg_count != arg_count)
  {
    ast_error(ast, "Invalid number of arguments to literal operator");
    return false;
  }

  make_literal_type(ast);
  return true;
}
