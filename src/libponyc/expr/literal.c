#include "literal.h"
#include "reference.h"
#include "../ast/astbuild.h"
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
#define UIF_INT_MASK    0x03FFF
#define UIF_ALL_TYPES   0x0FFFF
#define UIF_CONSTRAINED 0x10000
#define UIF_COUNT       16

static struct
{
  const char* name;
  lexint_t limit;
  bool neg_plus_one;    // Is a negated value allowed to be 1 bigger
} _str_uif_types[UIF_COUNT] =
{
  { "U8", {0x100, 0}, false },
  { "U16", {0x10000, 0}, false },
  { "U32", {0x100000000LL, 0}, false },
  { "U64", {0, 1}, false },
  { "U128", {0, 0}, false },  // Limit checked by lexer
  { "ULong", {0, 1}, false }, // Limited to 64 bits
  { "USize", {0, 1}, false }, // Limited to 64 bits
  { "I8", {0x80, 0}, true },
  { "I16", {0x8000, 0}, true },
  { "I32", {0x80000000, 0}, true },
  { "I64", {0x8000000000000000ULL, 0}, true },
  { "I128", {0, 0x8000000000000000ULL}, true },
  { "ILong", {0x8000000000000000ULL, 0}, true }, // Limited to 64 bits
  { "ISize", {0x8000000000000000ULL, 0}, true }, // Limited to 64 bits
  { "F32", {0, 0}, false },
  { "F64", {0, 0}, false }
};


typedef struct lit_op_info_t
{
  const char* name;
  size_t arg_count;
  bool can_propogate_literal;
  bool neg_plus_one;
} lit_op_info_t;

static lit_op_info_t _operator_fns[] =
{
  { "add", 1, true, false },
  { "sub", 1, true, false },
  { "mul", 1, true, false },
  { "div", 1, true, false },
  { "mod", 1, true, false },
  { "neg", 0, true, true },
  { "shl", 1, true, false },
  { "shr", 1, true, false },
  { "op_and", 1, true, false },
  { "op_or", 1, true, false },
  { "op_xor", 1, true, false },
  { "op_not", 0, true, false },
  { "eq", 1, false, false },
  { "ne", 1, false, false },
  { "lt", 1, false, false },
  { "le", 1, false, false },
  { "gt", 1, false, false },
  { "ge", 1, false, false },
  { NULL, 0, false, false }  // Terminator
};


static lit_op_info_t* lookup_literal_op(const char* name)
{
  for(int i = 0; _operator_fns[i].name != NULL; i++)
    if(strcmp(name, _operator_fns[i].name) == 0)
      return &_operator_fns[i];

  return NULL;
}


bool expr_literal(pass_opt_t* opt, ast_t* ast, const char* name)
{
  ast_t* type = type_builtin(opt, ast, name);

  if(is_typecheck_error(type))
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
  int cached_uif_index;
  bool valid_for_float;
  struct lit_chain_t* next; // Next node
} lit_chain_t;


static int uifset(pass_opt_t* opt, ast_t* type, lit_chain_t* chain);

static bool coerce_literal_to_type(ast_t** literal_expr, ast_t* target_type,
  lit_chain_t* chain, pass_opt_t* options, bool report_errors);

static bool unify(ast_t* ast, pass_opt_t* options, bool report_errors);


static void chain_init_head(lit_chain_t* head)
{
  assert(head != NULL);

  head->cardinality = CHAIN_CARD_BASE;
  head->formal = NULL;
  head->cached_type = NULL;
  head->name = NULL;
  head->cached_uif_index = -1;
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
  chain->cached_uif_index = -1;
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
static int uifset_simple_type(pass_opt_t* opt, ast_t* type)
{
  assert(type != NULL);

  int set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    ast_t* uif = type_builtin(opt, type, _str_uif_types[i].name);
    ast_setid(ast_childidx(uif, 3), TK_VAL);
    ast_setid(ast_childidx(uif, 4), TK_EPHEMERAL);

    if(is_subtype(uif, type, NULL, opt))
      set |= (1 << i);

    ast_free(uif);
  }

  return set;
}


// Determine the UIF types that the given formal parameter may be
static int uifset_formal_param(pass_opt_t* opt, ast_t* type_param_ref,
  lit_chain_t* chain)
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
  BUILD(typeargs, type_param,
    NODE(TK_TYPEARGS,
      NODE(TK_TYPEPARAMREF, DATA(type_param)
        ID(ast_name(ast_child(type_param))) NODE(TK_VAL) NONE)));

  ast_t* number = type_builtin(opt, type_param, "Number");
  ast_t* real = type_builtin_args(opt, type_param, "Real", typeargs);
  ast_setid(ast_childidx(real, 3), TK_BOX);

  bool is_real = is_subtype(constraint, real, NULL, opt);
  bool is_number = is_subtype(constraint, number, NULL, opt);
  ast_free(number);
  ast_free(real);

  if(!is_real || !is_number)
    // The formal param is not a subset of (Real[A] & Number)
    return UIF_NO_TYPES;

  int uif_set = 0;

  for(int i = 0; i < UIF_COUNT; i++)
  {
    ast_t* uif = type_builtin(opt, type_param, _str_uif_types[i].name);

    BUILD(params, type_param, NODE(TK_TYPEPARAMS, TREE(ast_dup(type_param))));
    BUILD(args, type_param, NODE(TK_TYPEARGS, TREE(uif)));

    if(check_constraints(NULL, params, args, false, opt))
      uif_set |= (1 << i);

    ast_free(args);
    ast_free(params);
  }

  if(uif_set == 0)  // No legal types
    return UIF_NO_TYPES;

  // Given formal parameter is legal to coerce to
  if(chain->formal != NULL && chain->formal != type_param)
  {
    ast_error(opt->check.errors, type_param_ref,
      "Cannot infer a literal type with multiple formal parameters");
    return UIF_ERROR;
  }

  chain->formal = type_param;
  chain->name = ast_name(ast_child(type_param));
  return uif_set | UIF_CONSTRAINED;
}


// Determine the UIF types that the given non-tuple union type may be
static int uifset_union(pass_opt_t* opt, ast_t* type, lit_chain_t* chain)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_UNIONTYPE);

  int uif_set = 0;

  // Process all elements of the union
  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(opt, p, chain);

    if(r == UIF_ERROR)  // Propogate errors
      return UIF_ERROR;

    bool child_valid = (r != UIF_NO_TYPES);
    bool child_formal = ((r & UIF_CONSTRAINED) != 0);
    bool others_valid = ((uif_set & UIF_ALL_TYPES) != 0);
    bool others_formal = ((uif_set & UIF_CONSTRAINED) != 0);

    if(child_valid && others_valid && (child_formal != others_formal))
    {
      // We're unioning a formal parameter and a UIF type, not allowed
      ast_error(opt->check.errors, type, "could not infer literal type, ambiguous union");
      return UIF_ERROR;
    }

    uif_set |= r;
  }

  return uif_set;
}


// Determine the UIF types that the given non-tuple intersection type may be
static int uifset_intersect(pass_opt_t* opt, ast_t* type, lit_chain_t* chain)
{
  assert(type != NULL);
  assert(ast_id(type) == TK_ISECTTYPE);

  int uif_set = UIF_ALL_TYPES;
  int constraint = 0;

  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
  {
    int r = uifset(opt, p, chain);

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
static int uifset(pass_opt_t* opt, ast_t* type, lit_chain_t* chain)
{
  assert(chain != NULL);

  if(is_typecheck_error(type))
    return UIF_NO_TYPES;

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return uifset_union(opt, type, chain);

    case TK_ISECTTYPE:
      return uifset_intersect(opt, type, chain);

    case TK_ARROW:
      // Since we don't care about capabilities we can just use the rhs
      assert(ast_id(ast_childidx(type, 1)) == TK_NOMINAL);
      return uifset(opt, ast_childidx(type, 1), chain);

    case TK_TYPEPARAMREF:
      if(chain->cardinality != CHAIN_CARD_BASE) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset_formal_param(opt, type, chain);

    case TK_TUPLETYPE:
      if(chain->cardinality != ast_childcount(type)) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset(opt, ast_childidx(type, chain->index), chain->next);

    case TK_NOMINAL:
      if(strcmp(ast_name(ast_childidx(type, 1)), "Array") == 0)
      {
        if(chain->cardinality != CHAIN_CARD_ARRAY) // Incorrect cardinality
          return UIF_NO_TYPES;

        ast_t* type_args = ast_childidx(type, 2);
        assert(ast_childcount(type_args) == 1);
        return uifset(opt, ast_child(type_args), chain->next);
      }

      if(chain->cardinality != CHAIN_CARD_BASE) // Incorrect cardinality
        return UIF_NO_TYPES;

      return uifset_simple_type(opt, type);

    case TK_DONTCARE:
    case TK_FUNTYPE:
      return UIF_NO_TYPES;

    default:
      ast_error(opt->check.errors, type, "Internal error: uif type, node %d", ast_id(type));
      assert(0);
      return UIF_ERROR;
  }
}


// Fill the given UIF type cache
static bool uif_type(pass_opt_t* opt, ast_t* literal, ast_t* type,
  lit_chain_t* chain_head, bool report_errors)
{
  assert(chain_head != NULL);
  assert(chain_head->cardinality == CHAIN_CARD_BASE);

  chain_head->formal = NULL;
  int r = uifset(opt, type, chain_head->next);

  if(r == UIF_ERROR)
    return false;

  if(r == UIF_NO_TYPES)
  {
    if(report_errors)
      ast_error(opt->check.errors, literal,
        "could not infer literal type, no valid types found");

    return false;
  }

  assert(type != NULL);

  if((r & UIF_CONSTRAINED) != 0)
  {
    // Type is a formal parameter
    assert(chain_head->formal != NULL);
    assert(chain_head->name != NULL);
    assert(chain_head->cached_uif_index < 0);

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
      chain_head->cached_type =
        type_builtin(opt, type, _str_uif_types[i].name);
      //ast_setid(ast_childidx(chain_head->cached_type, 4), TK_EPHEMERAL);
      chain_head->name = _str_uif_types[i].name;
      chain_head->cached_uif_index = i;
      return true;
    }
  }

  ast_error(opt->check.errors, literal, "Multiple possible types for literal");
  return false;
}


// Assign a UIF type from the given target type to the given AST
static bool uif_type_from_chain(pass_opt_t* opt, ast_t* literal,
  ast_t* target_type, lit_chain_t* chain, bool require_float,
  bool report_errors)
{
  assert(literal != NULL);
  assert(chain != NULL);

  lit_chain_t* chain_head = chain;
  while(chain_head->cardinality != CHAIN_CARD_BASE)
    chain_head = chain_head->next;

  if(chain_head->cached_type == NULL)
  {
    // This is the first time we've needed this type, find it
    if(!uif_type(opt, literal, target_type, chain_head, report_errors))
      return false;
  }

  if(require_float && !chain_head->valid_for_float)
  {
    if(report_errors)
      ast_error(opt->check.errors, literal, "Inferred possibly integer type %s for float literal",
        chain_head->name);

    return false;
  }

  if(ast_id(literal) == TK_INT && chain_head->cached_uif_index >= 0)
  {
    // Check for literals that are outside the range of their type.
    // Note we don't check for types bound to type parameters.
    int i = chain_head->cached_uif_index;

    if(_str_uif_types[i].limit.low != 0 || _str_uif_types[i].limit.high != 0)
    {
      // There is a limit specified for this type, the literal must be smaller
      // than that.
      bool neg_plus_one = false;

      if(_str_uif_types[i].neg_plus_one)
      {
        // If the literal is immediately negated it can be equal to the given
        // limit. This is because of how the world chooses to encode negative
        // integers.
        // For example, the maximum value in an I8 is 127. But the minimum
        // value is -128.
        // We don't actually calculate the negative value here, but we have a
        // looser test if the literal is immediately negated.
        // We do not do this if the negation is not immediate, eg "-(128)".
        ast_t* parent = ast_parent(literal);
        assert(parent != NULL);
        ast_t* parent_type = ast_type(parent);

        if(parent_type != NULL && ast_id(parent_type) == TK_OPERATORLITERAL &&
          ast_child(parent) == literal &&
          ((lit_op_info_t*)ast_data(parent_type))->neg_plus_one)
          neg_plus_one = true;
      }

      lexint_t* actual = ast_int(literal);
      int test = lexint_cmp(actual, &_str_uif_types[i].limit);

      if((test > 0) || (!neg_plus_one && (test == 0)))
      {
        // Illegal value.
        ast_error(opt->check.errors, literal, "Literal value is out of range for type (%s)",
          chain_head->name);
        return false;
      }
    }
  }

  ast_settype(literal, chain_head->cached_type);
  return true;
}


// Coerce a literal group (tuple or array) to be the specified target type
static bool coerce_group(ast_t** astp, ast_t* target_type, lit_chain_t* chain,
  size_t cardinality, pass_opt_t* options, bool report_errors)
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
  ast_t* p = ast_child(literal_expr);

  if(ast_id(literal_expr) == TK_ARRAY)
  {
    // The first child of an array AST is the forced type, skip it
    p = ast_sibling(p);
  }

  for(; p != NULL; p = ast_sibling(p))
  {
    ast_t* p_type = ast_type(p);

    if(is_typecheck_error(p_type))
      return false;

    if(is_type_literal(p_type))
    {
      // This element is a literal
      if(cardinality != CHAIN_CARD_ARRAY)
      {
        chain_clear_cache(&link);
        link.index = i;
      }

      if(!coerce_literal_to_type(&p, target_type, &link, options,
        report_errors))
        return false;
    }

    i++;
  }

  chain_remove(chain);
  return true;
}


// Coerce a literal control block to be the specified target type
static bool coerce_control_block(ast_t** astp, ast_t* target_type,
  lit_chain_t* chain, pass_opt_t* opt, bool report_errors)
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

    if(!coerce_literal_to_type(&branch, target_type, chain, opt,
      report_errors))
    {
      ast_free_unattached(block_type);
      return false;
    }

    block_type = type_union(opt, block_type, ast_type(branch));
  }

  if(is_typecheck_error(block_type))
    return false;

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
  lit_chain_t* chain, pass_opt_t* opt, bool report_errors)
{
  assert(astp != NULL);
  ast_t* literal_expr = *astp;
  assert(literal_expr != NULL);

  ast_t* lit_type = ast_type(literal_expr);

  if(lit_type == NULL ||
    (ast_id(lit_type) != TK_LITERAL && ast_id(lit_type) != TK_OPERATORLITERAL))
  {
    // Not a literal
    return true;
  }

  if(ast_child(lit_type) != NULL)
  {
    // Control block literal
    return coerce_control_block(astp, target_type, chain, opt,
      report_errors);
  }

  switch(ast_id(literal_expr))
  {
    case TK_TUPLE:  // Tuple literal
    {
      size_t cardinality = ast_childcount(literal_expr);
      if(!coerce_group(astp, target_type, chain, cardinality, opt,
        report_errors))
        return false;

      break;
    }

    case TK_INT:
      return uif_type_from_chain(opt, literal_expr, target_type, chain,
        false, report_errors);

    case TK_FLOAT:
      return uif_type_from_chain(opt, literal_expr, target_type, chain,
        true, report_errors);

    case TK_ARRAY:
      if(!coerce_group(astp, target_type, chain, CHAIN_CARD_ARRAY, opt,
        report_errors))
        return false;

      break;

    case TK_SEQ:
    {
      // Only coerce the last expression in the sequence
      ast_t* last = ast_childlast(literal_expr);

      if(!coerce_literal_to_type(&last, target_type, chain, opt,
        report_errors))
        return false;

      ast_settype(literal_expr, ast_type(last));
      return true;
    }

    case TK_CALL:
    {
      AST_GET_CHILDREN(literal_expr, positional, named, receiver);
      ast_t* arg = ast_child(positional);

      if(!coerce_literal_to_type(&receiver, target_type, chain, opt,
        report_errors))
        return false;

      if(arg != NULL &&
        !coerce_literal_to_type(&arg, target_type, chain, opt,
        report_errors))
        return false;

      ast_settype(literal_expr, ast_type(ast_child(receiver)));
      return true;
    }

    case TK_DOT:
    {
      ast_t* receiver = ast_child(literal_expr);
      if(!coerce_literal_to_type(&receiver, target_type, chain, opt,
        report_errors))
        return false;

      break;
    }

    case TK_RECOVER:
    {
      ast_t* expr = ast_childidx(literal_expr, 1);
      if(!coerce_literal_to_type(&expr, target_type, chain, opt,
        report_errors))
        return false;

      break;
    }

    default:
      ast_error(opt->check.errors, literal_expr, "Internal error, coerce_literal_to_type node %s",
        ast_get_print(literal_expr));
      assert(0);
      return false;
  }


  // Need to reprocess node now all the literals have types
  ast_settype(literal_expr, NULL);
  return (pass_expr(astp, opt) == AST_OK);
}


bool coerce_literals(ast_t** astp, ast_t* target_type, pass_opt_t* opt)
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

  if(target_type == NULL && !unify(literal_expr, opt, true))
    return false;

  lit_chain_t chain;
  chain_init_head(&chain);
  return coerce_literal_to_type(astp, target_type, &chain, opt, true);
}


// Unify all the branches of the given AST to the same type
static bool unify(ast_t* ast, pass_opt_t* opt, bool report_errors)
{
  assert(ast != NULL);
  ast_t* type = ast_type(ast);

  if(is_typecheck_error(type))
    return false;

  if(!is_type_literal(type)) // Not literal, nothing to unify
    return true;

  assert(type != NULL);
  ast_t* non_literal = ast_type(type);

  if(non_literal != NULL)
  {
    // Type has a non-literal element, coerce literals to that
    lit_chain_t chain;
    chain_init_head(&chain);
    return coerce_literal_to_type(&ast, non_literal, &chain, opt,
      report_errors);
    //return coerce_literals(&ast, non_literal, opt);
  }

  // Still a pure literal
  return true;
}


bool literal_member_access(ast_t* ast, pass_opt_t* opt)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_DOT || ast_id(ast) == TK_TILDE);

  AST_GET_CHILDREN(ast, receiver, name_node);

  if(!unify(receiver, opt, true))
    return false;

  ast_t* recv_type = ast_type(receiver);

  if(is_typecheck_error(recv_type))
    return false;

  if(ast_id(recv_type) != TK_LITERAL) // Literals resolved
    return true;

  // Receiver is a pure literal expression
  // Look up member name
  assert(ast_id(name_node) == TK_ID);
  const char* name = ast_name(name_node);
  lit_op_info_t* op = lookup_literal_op(name);

  if(op == NULL || ast_id(ast_parent(ast)) != TK_CALL)
  {
    ast_error(opt->check.errors, ast, "Cannot look up member %s on a literal", name);
    return false;
  }

  // This is a literal operator
  ast_t* op_type = ast_from(ast, TK_OPERATORLITERAL);
  ast_setdata(op_type, (void*)op);
  ast_settype(ast, op_type);
  return true;
}


bool literal_call(ast_t* ast, pass_opt_t* opt)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_CALL);

  AST_GET_CHILDREN(ast, positional_args, named_args, receiver);

  ast_t* recv_type = ast_type(receiver);

  if(is_typecheck_error(recv_type))
    return false;

  if(ast_id(recv_type) == TK_LITERAL)
  {
    ast_error(opt->check.errors, ast, "Cannot call a literal");
    return false;
  }

  if(ast_id(recv_type) != TK_OPERATORLITERAL) // Nothing to do
    return true;

  lit_op_info_t* op = (lit_op_info_t*)ast_data(recv_type);
  assert(op != NULL);

  if(ast_childcount(named_args) != 0)
  {
    ast_error(opt->check.errors, named_args, "Cannot use named arguments with literal operator");
    return false;
  }

  ast_t* arg = ast_child(positional_args);

  if(arg != NULL)
  {
    if(!unify(arg, opt, true))
      return false;

    ast_t* arg_type = ast_type(arg);

    if(is_typecheck_error(arg_type))
      return false;

    if(ast_id(arg_type) != TK_LITERAL)  // Apply argument type to receiver
      return coerce_literals(&receiver, arg_type, opt);

    if(!op->can_propogate_literal)
    {
      ast_error(opt->check.errors, ast, "Cannot infer operand type");
      return false;
    }
  }

  size_t arg_count = ast_childcount(positional_args);

  if(op->arg_count != arg_count)
  {
    ast_error(opt->check.errors, ast, "Invalid number of arguments to literal operator");
    return false;
  }

  make_literal_type(ast);
  return true;
}


bool literal_is(ast_t* ast, pass_opt_t* opt)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_IS || ast_id(ast) == TK_ISNT);

  AST_GET_CHILDREN(ast, left, right);

  ast_t* l_type = ast_type(left);
  ast_t* r_type = ast_type(right);

  if(is_typecheck_error(l_type) || is_typecheck_error(r_type))
    return false;

  if(!is_type_literal(l_type) && !is_type_literal(r_type))
    // No literals here.
    return true;

  if(is_type_literal(l_type) && !is_type_literal(r_type))
  {
    // Coerce left to type of right.
    return coerce_literals(&left, r_type, opt);
  }

  if(!is_type_literal(l_type) && is_type_literal(r_type))
  {
    // Coerce right to type of left.
    return coerce_literals(&right, l_type, opt);
  }

  // Both sides are literals, that's a problem.
  assert(is_type_literal(l_type));
  assert(is_type_literal(r_type));
  ast_error(opt->check.errors, ast, "Cannot infer type of operands");
  return false;
}


void literal_unify_control(ast_t* ast, pass_opt_t* opt)
{
  unify(ast, opt, false);
}
