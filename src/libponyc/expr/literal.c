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


// Type chains

// TODO: explain what the hell is going on

#define CHAIN_CARD_BASE   0 // Basic type
#define CHAIN_CARD_ARRAY  1 // Array member

typedef struct lit_chain_t
{
  size_t cardinality; // Tuple cardinality / CHAIN_CARD_* value
  size_t index;
  ast_t* formal;
  ast_t* cached_type;
  const char* name;
  bool valid_for_float;
  struct lit_chain_t* next; // Next node
} lit_chain_t;


static int uifset(ast_t* type, lit_chain_t* chain);

static bool coerce_literal_to_type(ast_t** literal_expr, ast_t* target_type,
  lit_chain_t* chain, pass_opt_t* options);

static bool unify(ast_t* ast, pass_opt_t* options);


static void chain_init_head(lit_chain_t* head)
{
  assert(head != NULL);

  head->cardinality = CHAIN_CARD_BASE;
  head->formal = NULL;
  head->cached_type = NULL;
  head->name = NULL;
  head->valid_for_float = false;
  head->next = head;
}


static void chain_clear_cache(lit_chain_t* chain)
{
  assert(chain != NULL);

  while(chain->cardinality != CHAIN_CARD_BASE)
    chain = chain->next;

  chain->formal = NULL;
  chain->cached_type = NULL;
  chain->name = NULL;
  chain->valid_for_float = false;
}


static void chain_add(lit_chain_t* chain, lit_chain_t* new_link,
  size_t cardinality)
{
  assert(new_link != NULL);
  assert(cardinality != CHAIN_CARD_BASE);

  new_link->cardinality = cardinality;
  new_link->index = 0;

  assert(chain != NULL);
  assert(chain->next != NULL);
  new_link->next = chain->next;
  chain->next = new_link;

  chain_clear_cache(new_link);
}


static void chain_remove(lit_chain_t* old_tail)
{
  assert(old_tail != NULL);
  assert(old_tail->next != NULL);
  assert(old_tail->next->next != NULL);
  assert(old_tail->next->next->cardinality == CHAIN_CARD_BASE);

  old_tail->next = old_tail->next->next;
  chain_clear_cache(old_tail);
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
    ast_setid(ast_childidx(uif, 4), TK_EPHEMERAL);

    if(is_subtype(uif, type))
      set |= (1 << i);

    ast_free(uif);
  }

  return set;
}


// Determine the UIF types that the given formal parameter may be
static int uifset_formal_param(ast_t* type_param_ref, lit_chain_t* chain)
{
  assert(type_param_ref != NULL);
  assert(ast_id(type_param_ref) == TK_TYPEPARAMREF);

  ast_t* type_param = (ast_t*)ast_data(type_param_ref);

  assert(type_param != NULL);
  assert(ast_id(type_param) == TK_TYPEPARAM);
  assert(chain != NULL);

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
  if(chain->formal != NULL && chain->formal != type_param)
  {
    ast_error(type_param_ref,
      "Cannot infer a literal type with multiple formal parameters");
    return UIF_ERROR;
  }

  chain->formal = type_param;
  chain->name = ast_name(ast_child(type_param));
  return uif_set | UIF_CONSTRAINED;
}


// Determine the UIF types that the given non-tuple union type may be
static int uifset_union(ast_t* type, lit_chain_t* chain)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_UNIONTYPE);

  int uif_set = 0;

  // Process all elements of the union
  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(p, chain);

    if(r == UIF_ERROR)  // Propogate errors
      return UIF_ERROR;

    bool child_valid = (r != UIF_NO_TYPES);
    bool child_formal = ((r & UIF_CONSTRAINED) != 0);
    bool others_valid = ((uif_set & UIF_ALL_TYPES) != 0);
    bool others_formal = ((uif_set & UIF_CONSTRAINED) != 0);

    if(child_valid && others_valid && (child_formal != others_formal))
    {
      // We're unioning a formal parameter and a UIF type, not allowed
      ast_error(type, "Could not infer literal type, ambiguous union");
      return UIF_ERROR;
    }
    
    uif_set |= r;
  }

  return uif_set;
}


// Determine the UIF types that the given non-tuple intersection type may be
static int uifset_intersect(ast_t* type, lit_chain_t* chain)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_ISECTTYPE);

  int uif_set = UIF_ALL_TYPES;
  int constraint = 0;

  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(p, chain);

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


// Determine the UIF types that the given type may be
static int uifset(ast_t* type, lit_chain_t* chain)
{
  assert(chain != NULL);

  if(type == NULL)
    return UIF_NO_TYPES;

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return uifset_union(type, chain);

    case TK_ISECTTYPE:
      return uifset_intersect(type, chain);

    case TK_ARROW:
      // Since we don't care about capabilities we can just use the rhs
      assert(ast_id(ast_childidx(type, 1)) == TK_NOMINAL);
      return uifset(ast_childidx(type, 1), chain);

    case TK_TYPEPARAMREF:
      if(chain->cardinality != CHAIN_CARD_BASE) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset_formal_param(type, chain);

    case TK_TUPLETYPE:
      if(chain->cardinality != ast_childcount(type)) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset(ast_childidx(type, chain->index), chain->next);

    case TK_NOMINAL:
      if(strcmp(ast_name(ast_childidx(type, 1)), "Array") == 0)
      {
        if(chain->cardinality != CHAIN_CARD_ARRAY) // Incorrect cardinality
          return UIF_NO_TYPES;

        ast_t* type_args = ast_childidx(type, 2);
        assert(ast_childcount(type_args) == 1);
        return uifset(ast_child(type_args), chain->next);
      }

      if(chain->cardinality != CHAIN_CARD_BASE) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset_simple_type(type);

    default:
      ast_error(type, "Internal error: uif type, node %d", ast_id(type));
      assert(0);
      return UIF_ERROR;
  }
}


// Fill the given UIF type cache
static bool uif_type(ast_t* literal, ast_t* type, lit_chain_t* chain_head)
{
  assert(chain_head != NULL);
  assert(chain_head->cardinality == CHAIN_CARD_BASE);

  chain_head->formal = NULL;
  int r = uifset(type, chain_head->next);

  if(r == UIF_ERROR)
    return false;
  
  if(r == UIF_NO_TYPES)
  {
    ast_error(literal, "Could not infer literal type, no valid types found");
    return false;
  }

  assert(type != NULL);

  if((r & UIF_CONSTRAINED) != 0)
  {
    // Type is a formal parameter
    assert(chain_head->formal != NULL);
    assert(chain_head->name != NULL);

    BUILD(uif_type, type,
      NODE(TK_TYPEPARAMREF, DATA(chain_head->formal)
      ID(chain_head->name) NODE(TK_VAL) NONE));

    chain_head->cached_type = uif_type;
    chain_head->valid_for_float = ((r & UIF_INT_MASK) == 0);
    return true;
  }

  // Type is one or more UIFs
  for(int i = 0; i < UIF_COUNT; i++)
  {
    if(r == (1 << i))
    {
      chain_head->valid_for_float = (((1 << i) & UIF_INT_MASK) == 0);
      chain_head->cached_type = type_builtin(type, _str_uif_types[i]);
      //ast_setid(ast_childidx(chain_head->cached_type, 4), TK_EPHEMERAL);
      chain_head->name = _str_uif_types[i];
      return true;
    }
  }

  ast_error(type, "Multiple possible types for literal");
  return false;
}


// Assign a UIF type from the given target type to the given AST
static bool uif_type_from_chain(ast_t* literal, ast_t* target_type,
  lit_chain_t* chain, bool require_float)
{
  assert(literal != NULL);
  assert(chain != NULL);

  lit_chain_t* chain_head = chain;
  while(chain_head->cardinality != CHAIN_CARD_BASE)
    chain_head = chain_head->next;

  if(chain_head->cached_type == NULL)
  {
    // This is the first time we've needed this type, find it
    if(!uif_type(literal, target_type, chain_head))
      return false;
  }

  if(require_float && !chain_head->valid_for_float)
  {
    ast_error(literal, "Inferred possibly integer type %s for float literal",
      chain_head->name);
    return false;
  }

  ast_settype(literal, chain_head->cached_type);
  return true;
}


// Coerce a literal group (tuple or array) to be the specified target type
static bool coerce_group(ast_t** astp, ast_t* target_type, lit_chain_t* chain,
  size_t cardinality, pass_opt_t* options)
{
  assert(astp != NULL);
  ast_t* literal_expr = *astp;
  assert(literal_expr != NULL);
  assert(ast_id(literal_expr) == TK_TUPLE || ast_id(literal_expr) == TK_ARRAY);
  assert(chain != NULL);
  assert(cardinality != CHAIN_CARD_BASE);

  size_t i = 0;
  lit_chain_t link;

  chain_add(chain, &link, cardinality);

  // Process each group element separately
  for(ast_t* p = ast_child(literal_expr); p != NULL; p = ast_sibling(p))
  {
    if(is_type_literal(ast_type(p)))
    {
      // This element is a literal
      if(cardinality != CHAIN_CARD_ARRAY)
      {
        chain_clear_cache(&link);
        link.index = i;
      }

      if(!coerce_literal_to_type(&p, target_type, &link, options))
        return false;
    }

    i++;
  }

  chain_remove(chain);
  return true;
}


// Coerce a literal control block to be the specified target type
static bool coerce_control_block(ast_t** astp, ast_t* target_type,
  lit_chain_t* chain, pass_opt_t* options)
{
  assert(astp != NULL);
  ast_t* literal_expr = *astp;
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

    if(!coerce_literal_to_type(&branch, target_type, chain, options))
    {
      ast_free_unattached(block_type);
      return false;
    }

    block_type = type_union(block_type, ast_type(branch));
  }

  // block_type may be a sub-tree of the current type of literal_expr.
  // This means we must copy it before setting it as the type since ast_settype
  // will first free the existing type of literal_expr, which may include
  // block_type.
  if(ast_parent(block_type) != NULL)
    block_type = ast_dup(block_type);

  ast_settype(literal_expr, block_type);
  return true;
}


// Coerce a literal expression to given tuple or non-tuple types
static bool coerce_literal_to_type(ast_t** astp, ast_t* target_type,
  lit_chain_t* chain, pass_opt_t* options)
{
  assert(astp != NULL);
  ast_t* literal_expr = *astp;
  assert(literal_expr != NULL);

  ast_t* lit_type = ast_type(literal_expr);

  if(lit_type == NULL ||
    (ast_id(lit_type) != TK_LITERAL && ast_id(lit_type) != TK_OPERATORLITERAL))
    // Not a literal
    return true;

  if(ast_child(lit_type) != NULL) // Control block literal
    return coerce_control_block(astp, target_type, chain, options);

  switch(ast_id(literal_expr))
  {
    case TK_TUPLE:  // Tuple literal
    {
      size_t cardinality = ast_childcount(literal_expr);
      if(!coerce_group(astp, target_type, chain, cardinality, options))
        return false;

      break;
    }

    case TK_INT:
      return uif_type_from_chain(literal_expr, target_type, chain, false);

    case TK_FLOAT:
      return uif_type_from_chain(literal_expr, target_type, chain, true);

    case TK_ARRAY:
      if(!coerce_group(astp, target_type, chain, CHAIN_CARD_ARRAY, options))
        return false;

      break;

    case TK_SEQ:
    {
      // Only coerce the last expression in the sequence
      ast_t* last = ast_childlast(literal_expr);
      if(!coerce_literal_to_type(&last, target_type, chain, options))
        return false;

      ast_settype(literal_expr, ast_type(last));
      return true;
    }

    case TK_CALL:
    {
      AST_GET_CHILDREN(literal_expr, positional, named, receiver);
      ast_t* arg = ast_child(positional);

      if(!coerce_literal_to_type(&receiver, target_type, chain, options))
        return false;

      if(arg != NULL &&
        !coerce_literal_to_type(&arg, target_type, chain, options))
        return false;

      ast_settype(literal_expr, ast_type(ast_child(receiver)));
      return true;
    }

    case TK_DOT:
    {
      ast_t* receiver = ast_child(literal_expr);
      if(!coerce_literal_to_type(&receiver, target_type, chain, options))
        return false;

      break;
    }

    default:
      ast_error(literal_expr, "Internal error, coerce_literal_to_type node %s",
        ast_get_print(literal_expr));
      assert(0);
      return false;
  }


  // Need to reprocess node now all the literals have types
  ast_settype(literal_expr, NULL);
  return (pass_expr(astp, options) == AST_OK);
}


bool coerce_literals(ast_t** astp, ast_t* target_type, pass_opt_t* options)
{
  assert(astp != NULL);
  ast_t* literal_expr = *astp;
  assert(literal_expr != NULL);

  if(ast_id(literal_expr) == TK_NONE)
    return true;

  ast_t* lit_type = ast_type(literal_expr);

  if(lit_type != NULL && ast_id(lit_type) != TK_LITERAL &&
    ast_id(lit_type) != TK_OPERATORLITERAL)
    return true;

  if(target_type == NULL && !unify(literal_expr, options))
    return false;

  lit_chain_t chain;
  chain_init_head(&chain);
  return coerce_literal_to_type(astp, target_type, &chain, options);
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
static bool unify(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);

  ast_t* type = ast_type(ast);

  if(!is_type_literal(type)) // Not literal, nothing to unify
    return true;

  assert(type != NULL);
  ast_t* non_literal = ast_type(type);

  if(non_literal != NULL)
  {
    // Type has a non-literal element, coerce literals to that
    return coerce_literals(&ast, non_literal, options);
  }

  // Still a pure literal
  return true;
}


bool literal_member_access(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_DOT || ast_id(ast) == TK_TILDE);

  AST_GET_CHILDREN(ast, receiver, name_node);

  if(!unify(receiver, options))
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


bool literal_call(ast_t* ast, pass_opt_t* options)
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

  if(ast_childcount(named_args) != 0)
  {
    ast_error(named_args, "Cannot use named arguments with literal operator");
    return false;
  }

  ast_t* arg = ast_child(positional_args);

  if(arg != NULL)
  {
    if(!unify(arg, options))
      return false;

    ast_t* arg_type = ast_type(arg);
    assert(arg_type != NULL);

    if(ast_id(arg_type) != TK_LITERAL)  // Apply argument type to receiver
      return coerce_literals(&receiver, arg_type, options);

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
