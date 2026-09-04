#include "infer.h"
#include "call.h"
#include "../ast/astbuild.h"
#include "../pass/expr.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../type/typealias.h"
#include "../type/typeparam.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

typedef enum
{
  REASON_NO_POSITION,
  REASON_NO_ARGUMENT,
  REASON_LITERAL_ONLY,
  REASON_UNTYPED_ARG,
  REASON_DEPENDENT,
  REASON_DEFAULT_UNDECIDED
} unbound_reason_t;

typedef enum
{
  REC_UNBOUND,
  REC_BOUND,
  REC_PINNED,
  REC_CONFLICT
} record_state_t;

typedef struct
{
  record_state_t state;
  ast_t* typeparam;

  union
  {
    struct
    {
      unbound_reason_t reason;
      ast_t* anchor;
      const char* detail;
    } unbound;

    struct
    {
      ast_t* type;
      ast_t* arg;
    } bound;

    struct
    {
      ast_t* type;
      ast_t* arg;
    } pinned;

    struct
    {
      ast_t* arg_i;
      ast_t* arg_j;
      bool nested;
    } conflict;
  };
} infer_record_t;

// True when def is one of the type parameters in the given list, by root
// pointer (scope resolution stores the original definition in ast_data).
static bool is_typeparam_of(ast_t* def, ast_t* typeparams)
{
  ast_t* root = typeparam_root(def);

  ast_t* tp = ast_child(typeparams);
  while(tp != NULL)
  {
    if(typeparam_root(tp) == root)
      return true;
    tp = ast_sibling(tp);
  }

  return false;
}

static bool type_mentions_one_typeparam(ast_t* type, ast_t* tp_node)
{
  if(type == NULL)
    return false;

  if(ast_id(type) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(type);
    return typeparam_root(def) == typeparam_root(tp_node);
  }

  for(ast_t* child = ast_child(type); child != NULL;
    child = ast_sibling(child))
  {
    if(type_mentions_one_typeparam(child, tp_node))
      return true;
  }

  return false;
}

bool type_mentions_typeparams(ast_t* type, ast_t* typeparams)
{
  if(type == NULL)
    return false;

  if(ast_id(type) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(type);
    return is_typeparam_of(def, typeparams);
  }

  for(ast_t* child = ast_child(type); child != NULL;
    child = ast_sibling(child))
  {
    if(type_mentions_typeparams(child, typeparams))
      return true;
  }

  return false;
}

static bool bindable_position_one(ast_t* type, ast_t* tp_node)
{
  if(type == NULL)
    return false;

  switch(ast_id(type))
  {
    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      return typeparam_root(def) == typeparam_root(tp_node);
    }

    case TK_NOMINAL:
    {
      ast_t* typeargs = ast_childidx(type, 2);
      ast_t* ta = ast_child(typeargs);
      while(ta != NULL)
      {
        if(type_mentions_one_typeparam(ta, tp_node))
          return true;
        ta = ast_sibling(ta);
      }
      return false;
    }

    case TK_TUPLETYPE:
    {
      ast_t* elem = ast_child(type);
      while(elem != NULL)
      {
        if(bindable_position_one(elem, tp_node))
          return true;
        elem = ast_sibling(elem);
      }
      return false;
    }

    default:
      return false;
  }
}

bool infer_bindable_position(ast_t* type, ast_t* typeparams)
{
  if(type == NULL)
    return false;

  switch(ast_id(type))
  {
    case TK_TYPEPARAMREF:
    {
      ast_t* def = (ast_t*)ast_data(type);
      return is_typeparam_of(def, typeparams);
    }

    case TK_NOMINAL:
    {
      ast_t* typeargs = ast_childidx(type, 2);
      ast_t* ta = ast_child(typeargs);
      while(ta != NULL)
      {
        if(type_mentions_typeparams(ta, typeparams))
          return true;
        ta = ast_sibling(ta);
      }
      return false;
    }

    case TK_TUPLETYPE:
    {
      ast_t* elem = ast_child(type);
      while(elem != NULL)
      {
        if(infer_bindable_position(elem, typeparams))
          return true;
        elem = ast_sibling(elem);
      }
      return false;
    }

    default:
      return false;
  }
}

ast_t* antecedent_prune(ast_t* type, ast_t* typeparams)
{
  if(type == NULL || ast_id(typeparams) == TK_NONE)
    return type;

  switch(ast_id(type))
  {
    case TK_TYPEPARAMREF:
      return type;

    case TK_NOMINAL:
    {
      if(type_mentions_typeparams(type, typeparams))
        return NULL;
      return type;
    }

    case TK_TUPLETYPE:
    case TK_ARROW:
    case TK_FUNTYPE:
    {
      if(type_mentions_typeparams(type, typeparams))
        return NULL;
      return type;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* survivor = NULL;
      size_t survivors = 0;
      bool any_dropped = false;

      ast_t* member = ast_child(type);
      while(member != NULL)
      {
        if(ast_id(member) == TK_TYPEPARAMREF)
        {
          survivor = member;
          survivors++;
        }
        else if(!type_mentions_typeparams(member, typeparams))
        {
          survivor = member;
          survivors++;
        }
        else
        {
          any_dropped = true;
        }
        member = ast_sibling(member);
      }

      if(!any_dropped)
        return type;

      if(survivors == 1)
        return survivor;

      return NULL;
    }

    case TK_TYPEALIASREF:
    {
      if(type_mentions_typeparams(type, typeparams))
        return NULL;
      return type;
    }

    default:
      return type;
  }
}

// Walk up from a node through propagation contexts, returning the parent
// context or NULL when propagation stops. The method_body and loop_body
// parameters are the targets of TK_RETURN and TK_BREAK respectively.
static ast_t* antecedent_parent(ast_t* ast, ast_t* method_body,
  ast_t* loop_body, bool* is_recovered)
{
  ast_t* parent = ast_parent(ast);

  switch(ast_id(parent))
  {
    case TK_SEQ:
    {
      if(ast_childlast(parent) == ast)
        return parent;

      if(ast_id(ast_parent(parent)) == TK_ARRAY)
        return parent;

      return NULL;
    }

    case TK_TUPLE:
      return parent;

    case TK_RECOVER:
    {
      if(is_recovered != NULL)
        *is_recovered = true;
      return parent;
    }

    case TK_RETURN:
      return method_body;

    case TK_BREAK:
      return loop_body;

    case TK_IF:
    case TK_IFDEF:
    case TK_IFTYPE:
    case TK_IFTYPE_SET:
    case TK_THEN:
    case TK_ELSE:
    case TK_WHILE:
    case TK_REPEAT:
    case TK_MATCH:
    case TK_CASES:
    case TK_CASE:
    case TK_TRY:
    case TK_TRY_NO_CHECK:
    case TK_DISPOSING_BLOCK:
    case TK_CALL:
      return parent;

    default:
      return NULL;
  }
}

// Find the structural loop body (nearest TK_WHILE or TK_REPEAT's body)
// between a TK_BREAK node and a given ancestor. Returns NULL when no
// loop intervenes.
static ast_t* structural_break_target(ast_t* brk, ast_t* ancestor)
{
  ast_t* node = ast_parent(brk);

  while(node != NULL && node != ancestor)
  {
    if(ast_id(node) == TK_WHILE || ast_id(node) == TK_REPEAT)
      return ast_child(node);

    node = ast_parent(node);
  }

  return NULL;
}

// True when a node in the argument's subtree is antecedent-dependent:
// an array literal, lambda, object literal, or `as` cast reachable through
// propagation contexts from the argument root.
static bool subtree_has_dependent(ast_t* node, ast_t* argument)
{
  switch(ast_id(node))
  {
    case TK_ARRAY:
    case TK_LAMBDA:
    case TK_BARELAMBDA:
    case TK_OBJECT:
    case TK_AS:
    {
      ast_t* start = node;

      // For an array literal that is the receiver of a DOT (`.values()`),
      // start from the DOT.
      if(ast_id(node) == TK_ARRAY)
      {
        ast_t* p = ast_parent(node);
        if(p != NULL && ast_id(p) == TK_DOT && ast_child(p) == node)
          start = p;
      }

      ast_t* walk = start;
      while(walk != NULL && walk != argument)
      {
        ast_t* break_target = NULL;

        if(ast_id(walk) == TK_BREAK)
          break_target = structural_break_target(walk, argument);

        walk = antecedent_parent(walk, NULL, break_target, NULL);
      }

      return (walk == argument);
    }

    default:
      break;
  }

  // Don't enter lambda/object bodies.
  if(ast_id(node) == TK_FUN || ast_id(node) == TK_BE ||
    ast_id(node) == TK_NEW)
    return false;

  for(ast_t* child = ast_child(node); child != NULL;
    child = ast_sibling(child))
  {
    if(subtree_has_dependent(child, argument))
      return true;
  }

  return false;
}

static bool is_antecedent_dependent(ast_t* arg)
{
  return subtree_has_dependent(arg, arg);
}

// True when a lambda or bare lambda has any parameter without an explicit
// type annotation.
static bool lambda_has_untyped_params(ast_t* node)
{
  // Lambda: child 3 = params, bare lambda: child 4 = params.
  ast_t* params = (ast_id(node) == TK_LAMBDA)
    ? ast_childidx(node, 3)
    : ast_childidx(node, 4);

  if(ast_id(params) == TK_NONE)
    return false;

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(ast_childidx(p, 1)) == TK_NONE)
      return true;
  }

  return false;
}

// True when an array literal has no explicit `as` type annotation.
static bool array_has_no_explicit_type(ast_t* node)
{
  return ast_id(ast_child(node)) == TK_NONE;
}

// Like subtree_has_dependent, but only matches dependent expressions that
// lack explicit types — ones that will fail if visited before the parameter
// type is reified. An array with `as String val` or a lambda with all typed
// parameters can be visited early and provide evidence for inference.
static bool subtree_needs_antecedent(ast_t* node, ast_t* argument)
{
  switch(ast_id(node))
  {
    case TK_ARRAY:
    case TK_LAMBDA:
    case TK_BARELAMBDA:
    {
      bool needs_type = false;

      if(ast_id(node) == TK_ARRAY)
        needs_type = array_has_no_explicit_type(node);
      else
        needs_type = lambda_has_untyped_params(node);

      if(!needs_type)
        break;

      ast_t* start = node;

      if(ast_id(node) == TK_ARRAY)
      {
        ast_t* p = ast_parent(node);
        if(p != NULL && ast_id(p) == TK_DOT && ast_child(p) == node)
          start = p;
      }

      ast_t* walk = start;
      while(walk != NULL && walk != argument)
      {
        ast_t* break_target = NULL;

        if(ast_id(walk) == TK_BREAK)
          break_target = structural_break_target(walk, argument);

        walk = antecedent_parent(walk, NULL, break_target, NULL);
      }

      return (walk == argument);
    }

    // Object literals and `as` casts always have explicit types.
    case TK_OBJECT:
    case TK_AS:
      break;

    default:
      break;
  }

  if(ast_id(node) == TK_FUN || ast_id(node) == TK_BE ||
    ast_id(node) == TK_NEW)
    return false;

  for(ast_t* child = ast_child(node); child != NULL;
    child = ast_sibling(child))
  {
    if(subtree_needs_antecedent(child, argument))
      return true;
  }

  return false;
}

bool infer_needs_antecedent_type(ast_t* arg)
{
  return subtree_needs_antecedent(arg, arg);
}

// Find the index of a type parameter in the list by root pointer.
static size_t typeparam_index(ast_t* def, ast_t* typeparams)
{
  ast_t* root = typeparam_root(def);
  size_t i = 0;

  ast_t* tp = ast_child(typeparams);
  while(tp != NULL)
  {
    if(typeparam_root(tp) == root)
      return i;
    tp = ast_sibling(tp);
    i++;
  }

  return (size_t)-1;
}

// Record evidence at a direct position: the argument's aliased type is a
// candidate for the type parameter. The record takes ownership of a dup.
static void record_direct(infer_record_t* records, ast_t* typeparams,
  ast_t* def, ast_t* candidate, ast_t* arg, pass_opt_t* opt)
{
  size_t idx = typeparam_index(def, typeparams);
  if(idx == (size_t)-1)
    return;

  infer_record_t* rec = &records[idx];

  switch(rec->state)
  {
    case REC_UNBOUND:
      rec->state = REC_BOUND;
      rec->bound.type = ast_dup(candidate);
      rec->bound.arg = arg;
      break;

    case REC_BOUND:
    {
      // Merge: find the supertype.
      if(is_subtype(rec->bound.type, candidate, NULL, opt))
      {
        ast_free_unattached(rec->bound.type);
        rec->bound.type = ast_dup(candidate);
        rec->bound.arg = arg;
      }
      else if(!is_subtype(candidate, rec->bound.type, NULL, opt))
      {
        ast_t* first_arg = rec->bound.arg;
        ast_free_unattached(rec->bound.type);
        rec->state = REC_CONFLICT;
        rec->conflict.arg_i = first_arg;
        rec->conflict.arg_j = arg;
        rec->conflict.nested = false;
      }
      break;
    }

    case REC_PINNED:
      // Pinned wins; direct left to check_arg_types.
      break;

    case REC_CONFLICT:
      break;
  }
}

// Record evidence at a nested position: exact type from structural matching.
// The record takes ownership of a dup.
static void record_nested(infer_record_t* records, ast_t* typeparams,
  ast_t* def, ast_t* exact_type, ast_t* arg, pass_opt_t* opt)
{
  size_t idx = typeparam_index(def, typeparams);
  if(idx == (size_t)-1)
    return;

  infer_record_t* rec = &records[idx];

  switch(rec->state)
  {
    case REC_UNBOUND:
      rec->state = REC_PINNED;
      rec->pinned.type = ast_dup(exact_type);
      rec->pinned.arg = arg;
      break;

    case REC_BOUND:
      // Pinned wins over direct.
      ast_free_unattached(rec->bound.type);
      rec->state = REC_PINNED;
      rec->pinned.type = ast_dup(exact_type);
      rec->pinned.arg = arg;
      break;

    case REC_PINNED:
    {
      if(!is_eqtype(rec->pinned.type, exact_type, NULL, opt))
      {
        ast_free_unattached(rec->pinned.type);
        rec->state = REC_CONFLICT;
        rec->conflict.arg_i = rec->pinned.arg;
        rec->conflict.arg_j = arg;
        rec->conflict.nested = true;
      }
      break;
    }

    case REC_CONFLICT:
      break;
  }
}

static void extract_nested(infer_record_t* records, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* param_type, ast_t* arg_type, ast_t* arg,
  pass_opt_t* opt);

// Walk the provides list of arg_type's def to find param_type's def.
static void provides_walk(infer_record_t* records, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* param_type, ast_t* arg_type, ast_t* arg,
  pass_opt_t* opt)
{
  ast_t* param_def = (ast_t*)ast_data(param_type);
  ast_t* arg_def = (ast_t*)ast_data(arg_type);

  if(param_def == NULL || arg_def == NULL)
    return;

  ast_t* provides = ast_childidx(arg_def, 3);
  if(provides == NULL || ast_id(provides) == TK_NONE)
    return;

  ast_t* arg_tp = ast_childidx(arg_def, 1);
  ast_t* arg_ta = ast_childidx(arg_type, 2);

  bool multi = (ast_id(provides) == TK_PROVIDES ||
    ast_id(provides) == TK_UNIONTYPE ||
    ast_id(provides) == TK_ISECTTYPE);

  ast_t* entry = multi ? ast_child(provides) : provides;

  while(entry != NULL)
  {
    ast_t* reified_entry = reify(entry, arg_tp, arg_ta, opt, true);

    if(ast_id(reified_entry) == TK_NOMINAL)
    {
      ast_t* entry_def = (ast_t*)ast_data(reified_entry);
      if(entry_def == param_def)
      {
        // Match: recurse pairwise.
        ast_t* p_ta = ast_childidx(param_type, 2);
        ast_t* e_ta = ast_childidx(reified_entry, 2);

        ast_t* p_arg = ast_child(p_ta);
        ast_t* e_arg = ast_child(e_ta);

        while(p_arg != NULL && e_arg != NULL)
        {
          extract_nested(records, typeparams, other_typeparams,
            p_arg, e_arg, arg, opt);
          p_arg = ast_sibling(p_arg);
          e_arg = ast_sibling(e_arg);
        }

        ast_free_unattached(reified_entry);
        return;
      }
    }
    else if(ast_id(reified_entry) != TK_ARROW)
    {
      // Non-arrow, non-nominal: might have nested matches.
      // (Arrow types from reification treated as opaque.)
    }

    ast_free_unattached(reified_entry);
    entry = multi ? ast_sibling(entry) : NULL;
  }
}

static void extract_nested(infer_record_t* records, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* param_type, ast_t* arg_type, ast_t* arg,
  pass_opt_t* opt)
{
  if(param_type == NULL || arg_type == NULL)
    return;

  ast_t* p_unfolded = NULL;
  ast_t* a_unfolded = NULL;

  if(ast_id(param_type) == TK_TYPEALIASREF)
  {
    p_unfolded = typealias_unfold(param_type);
    if(p_unfolded != NULL)
      param_type = p_unfolded;
  }

  if(ast_id(arg_type) == TK_TYPEALIASREF)
  {
    a_unfolded = typealias_unfold(arg_type);
    if(a_unfolded != NULL)
      arg_type = a_unfolded;
  }

  // Direct position: bare TYPEPARAMREF of the inferred list.
  if(ast_id(param_type) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(param_type);

    if(is_typeparam_of(def, typeparams))
    {
      record_nested(records, typeparams, def, arg_type, arg, opt);
      goto cleanup;
    }

    goto cleanup;
  }

  // Nominal: same def -> pairwise; different def -> provides walk.
  if(ast_id(param_type) == TK_NOMINAL && ast_id(arg_type) == TK_NOMINAL)
  {
    ast_t* param_def = (ast_t*)ast_data(param_type);
    ast_t* arg_def = (ast_t*)ast_data(arg_type);

    if(param_def == arg_def)
    {
      ast_t* p_ta = ast_childidx(param_type, 2);
      ast_t* a_ta = ast_childidx(arg_type, 2);

      ast_t* p_arg = ast_child(p_ta);
      ast_t* a_arg = ast_child(a_ta);

      while(p_arg != NULL && a_arg != NULL)
      {
        extract_nested(records, typeparams, other_typeparams,
          p_arg, a_arg, arg, opt);
        p_arg = ast_sibling(p_arg);
        a_arg = ast_sibling(a_arg);
      }
    }
    else
    {
      provides_walk(records, typeparams, other_typeparams,
        param_type, arg_type, arg, opt);
    }

    goto cleanup;
  }

  // Intersection argument: try each member.
  if(ast_id(arg_type) == TK_ISECTTYPE)
  {
    ast_t* member = ast_child(arg_type);
    while(member != NULL)
    {
      extract_nested(records, typeparams, other_typeparams,
        param_type, member, arg, opt);
      member = ast_sibling(member);
    }
    goto cleanup;
  }

  // TYPEPARAMREF argument at a nested position: retry against constraint.
  if(ast_id(arg_type) == TK_TYPEPARAMREF)
  {
    ast_t* arg_def = (ast_t*)ast_data(arg_type);
    if(arg_def == NULL)
      goto cleanup;

    ast_t* constraint = typeparam_constraint(arg_type);
    if(constraint != NULL && ast_id(constraint) != TK_NONE)
    {
      extract_nested(records, typeparams, other_typeparams,
        param_type, constraint, arg, opt);
    }
    goto cleanup;
  }

  // Tuples: pairwise.
  if(ast_id(param_type) == TK_TUPLETYPE && ast_id(arg_type) == TK_TUPLETYPE)
  {
    ast_t* p_elem = ast_child(param_type);
    ast_t* a_elem = ast_child(arg_type);

    while(p_elem != NULL && a_elem != NULL)
    {
      extract_nested(records, typeparams, other_typeparams,
        p_elem, a_elem, arg, opt);
      p_elem = ast_sibling(p_elem);
      a_elem = ast_sibling(a_elem);
    }
    goto cleanup;
  }

cleanup:
  if(p_unfolded != NULL)
    ast_free_unattached(p_unfolded);
  if(a_unfolded != NULL)
    ast_free_unattached(a_unfolded);
}

static void collect_evidence(infer_record_t* records, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* param_type, ast_t* arg,
  pass_opt_t* opt)
{
  if(ast_id(param_type) == TK_TYPEPARAMREF)
  {
    // Direct position.
    ast_t* def = (ast_t*)ast_data(param_type);
    if(!is_typeparam_of(def, typeparams))
      return;

    ast_t* arg_type = ast_type(arg);
    ast_t* candidate = alias(arg_type, opt);

    // If alias introduces TK_ALIASED, use the argument's own type
    // so the missing consume is reported by check_arg_types.
    if(candidate != NULL && ast_id(candidate) == TK_ALIASED)
    {
      ast_free_unattached(candidate);
      candidate = arg_type;
    }
    else if(candidate == NULL)
    {
      return;
    }

    record_direct(records, typeparams, def, candidate, arg, opt);

    if(candidate != arg_type)
      ast_free_unattached(candidate);
  }
  else
  {
    // Nested position.
    ast_t* arg_type = ast_type(arg);
    ast_t* aliased = alias(arg_type, opt);

    if(aliased == NULL)
      return;

    if(ast_id(aliased) == TK_ALIASED)
    {
      ast_free_unattached(aliased);
      aliased = arg_type;
    }

    extract_nested(records, typeparams, other_typeparams,
      param_type, aliased, arg, opt);

    if(aliased != arg_type)
      ast_free_unattached(aliased);
  }
}

// Report a continuation message for one unbound type parameter.
static void report_unbound(pass_opt_t* opt, ast_t* error_at,
  const char* callee_name, infer_record_t* rec)
{
  ast_t* tp = rec->typeparam;
  const char* tp_name = ast_name(ast_child(tp));

  ast_error_continue(opt->check.errors, tp,
    "type parameter '%s' could not be determined", tp_name);

  switch(rec->unbound.reason)
  {
    case REASON_NO_POSITION:
      ast_error_continue(opt->check.errors, tp,
        "'%s' is not mentioned by the type of any parameter of '%s'",
        tp_name, callee_name);
      ast_error_continue(opt->check.errors, tp,
        "no argument can determine '%s'", tp_name);
      break;

    case REASON_NO_ARGUMENT:
      ast_error_continue(opt->check.errors, error_at,
        "no argument was given for any parameter whose type mentions '%s'",
        tp_name);
      break;

    case REASON_LITERAL_ONLY:
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "'%s' could only be determined by this argument", tp_name);
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "a literal has no type of its own until the parameter type is known");
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "give the literal a type, as in 'U8(1)'");
      break;

    case REASON_DEPENDENT:
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "this argument cannot determine '%s'", tp_name);
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "its type is inferred from the parameter");
      ast_error_continue(opt->check.errors, rec->unbound.anchor,
        "with the type argument written, this argument's type comes "
        "from the parameter");
      break;

    case REASON_UNTYPED_ARG:
      break;

    case REASON_DEFAULT_UNDECIDED:
      ast_error_continue(opt->check.errors, tp,
        "the default for '%s' refers to '%s', which could not be determined",
        tp_name, rec->unbound.detail);
      break;
  }
}

// Report the error for conflicting type evidence.
static void report_conflict(pass_opt_t* opt, ast_t* error_at,
  const char* callee_name, infer_record_t* rec)
{
  const char* tp_name = ast_name(ast_child(rec->typeparam));

  ast_error(opt->check.errors, error_at,
    "the arguments give conflicting types for type parameter '%s'", tp_name);

  if(rec->conflict.nested)
  {
    ast_error_continue(opt->check.errors, rec->conflict.arg_i,
      "this argument gives %s for '%s', from %s",
      ast_print_type(ast_type(rec->conflict.arg_i), opt->strtab),
      tp_name,
      ast_print_type(ast_type(rec->conflict.arg_i), opt->strtab));

    ast_error_continue(opt->check.errors, rec->conflict.arg_j,
      "this argument gives %s for '%s', from %s",
      ast_print_type(ast_type(rec->conflict.arg_j), opt->strtab),
      tp_name,
      ast_print_type(ast_type(rec->conflict.arg_j), opt->strtab));

    ast_error_continue(opt->check.errors, error_at,
      "the types must be exactly the same, because type arguments "
      "are invariant");
  }
  else
  {
    ast_error_continue(opt->check.errors, rec->conflict.arg_i,
      "this argument gives %s for '%s'",
      ast_print_type(ast_type(rec->conflict.arg_i), opt->strtab),
      tp_name);

    ast_error_continue(opt->check.errors, rec->conflict.arg_j,
      "this argument gives %s for '%s'",
      ast_print_type(ast_type(rec->conflict.arg_j), opt->strtab),
      tp_name);

    ast_error_continue(opt->check.errors, error_at,
      "the types must be the same, or one a subtype of the other");
    ast_error_continue(opt->check.errors, error_at,
      "when one is a subtype of the other, the supertype is used");
  }

  ast_error_continue(opt->check.errors, error_at,
    "write the type argument to choose between them, as in '%s[T](...)'",
    callee_name);
}

static void free_records(infer_record_t* records, size_t count,
  size_t alloc_size)
{
  for(size_t i = 0; i < count; i++)
  {
    if(records[i].state == REC_BOUND)
      ast_free_unattached(records[i].bound.type);
    else if(records[i].state == REC_PINNED)
      ast_free_unattached(records[i].pinned.type);
  }

  ponyint_pool_free_size(alloc_size, records);
}

ast_t* infer_typeargs(pass_opt_t* opt, ast_t* typeparams,
  ast_t* other_typeparams, ast_t* params, ast_t* positional,
  ast_t* error_at, const char* callee_name)
{
  pony_assert(ast_id(typeparams) == TK_TYPEPARAMS);

  size_t tp_count = ast_childcount(typeparams);
  pony_assert(tp_count > 0);

  size_t alloc_size = tp_count * sizeof(infer_record_t);
  infer_record_t* records =
    (infer_record_t*)ponyint_pool_alloc_size(alloc_size);
  memset(records, 0, alloc_size);

  ast_t* tp = ast_child(typeparams);
  for(size_t i = 0; i < tp_count; i++)
  {
    records[i].state = REC_UNBOUND;
    records[i].typeparam = tp;
    records[i].unbound.reason = REASON_NO_POSITION;
    records[i].unbound.anchor = tp;
    records[i].unbound.detail = NULL;
    tp = ast_sibling(tp);
  }

  ast_t* param = ast_child(params);
  ast_t* arg = ast_child(positional);
  bool has_untyped = false;

  while(param != NULL && arg != NULL)
  {
    ast_t* p_type = ast_childidx(param, 1);

    ast_t* tp_node = ast_child(typeparams);
    for(size_t i = 0; i < tp_count; i++)
    {
      if(records[i].state != REC_UNBOUND)
      {
        tp_node = ast_sibling(tp_node);
        continue;
      }

      if(bindable_position_one(p_type, tp_node))
      {
        if(records[i].unbound.reason == REASON_NO_POSITION)
        {
          // Upgrade to at least NO_ARGUMENT.
          if(type_mentions_one_typeparam(p_type, tp_node))
            records[i].unbound.reason = REASON_NO_ARGUMENT;
        }
      }

      tp_node = ast_sibling(tp_node);
    }

    if(ast_id(arg) == TK_NONE)
    {
      param = ast_sibling(param);
      arg = ast_sibling(arg);
      continue;
    }

    if(!infer_bindable_position(p_type, typeparams))
    {
      param = ast_sibling(param);
      arg = ast_sibling(arg);
      continue;
    }

    // Classification ORDER: (1) TK_NONE (above); (2) dependent;
    // (3) type error; (4) literal; (5) evidence.
    if(is_antecedent_dependent(arg))
    {
      tp_node = ast_child(typeparams);
      for(size_t i = 0; i < tp_count; i++)
      {
        if(records[i].state == REC_UNBOUND &&
          records[i].unbound.reason != REASON_DEPENDENT)
        {
          if(type_mentions_one_typeparam(p_type, tp_node))
          {
            records[i].unbound.reason = REASON_DEPENDENT;
            records[i].unbound.anchor = arg;
          }
        }
        tp_node = ast_sibling(tp_node);
      }

      param = ast_sibling(param);
      arg = ast_sibling(arg);
      continue;
    }

    ast_t* arg_type = ast_type(arg);
    if(is_typecheck_error(arg_type))
    {
      has_untyped = true;
      tp_node = ast_child(typeparams);
      for(size_t i = 0; i < tp_count; i++)
      {
        if(records[i].state == REC_UNBOUND &&
          type_mentions_one_typeparam(p_type, tp_node))
        {
          records[i].unbound.reason = REASON_UNTYPED_ARG;
          records[i].unbound.anchor = arg;
        }
        tp_node = ast_sibling(tp_node);
      }

      param = ast_sibling(param);
      arg = ast_sibling(arg);
      continue;
    }

    if(ast_id(arg_type) == TK_LITERAL || ast_id(arg_type) == TK_OPERATORLITERAL)
    {
      tp_node = ast_child(typeparams);
      for(size_t i = 0; i < tp_count; i++)
      {
        if(records[i].state == REC_UNBOUND &&
          records[i].unbound.reason != REASON_LITERAL_ONLY &&
          records[i].unbound.reason != REASON_DEPENDENT)
        {
          if(type_mentions_one_typeparam(p_type, tp_node))
          {
            records[i].unbound.reason = REASON_LITERAL_ONLY;
            records[i].unbound.anchor = arg;
          }
        }
        tp_node = ast_sibling(tp_node);
      }

      param = ast_sibling(param);
      arg = ast_sibling(arg);
      continue;
    }

    collect_evidence(records, typeparams, other_typeparams, p_type, arg, opt);

    param = ast_sibling(param);
    arg = ast_sibling(arg);
  }

  // Cache is_antecedent_dependent per argument for the default-first loop.
  size_t arg_count = ast_childcount(positional);
  bool* dep_cache = NULL;
  if(arg_count > 0)
  {
    size_t dep_size = arg_count * sizeof(bool);
    dep_cache = (bool*)ponyint_pool_alloc_size(dep_size);
    ast_t* a = ast_child(positional);
    for(size_t k = 0; k < arg_count; k++)
    {
      dep_cache[k] = is_antecedent_dependent(a);
      a = ast_sibling(a);
    }
  }

  // Resolution: default-first, in declaration order.
  BUILD(typeargs, error_at, NODE(TK_TYPEARGS));

  bool ok = true;

  for(size_t i = 0; i < tp_count; i++)
  {
    infer_record_t* rec = &records[i];
    ast_t* resolved = NULL;
    bool resolved_is_owned = false;

    if(rec->state == REC_CONFLICT)
    {
      report_conflict(opt, error_at, callee_name, rec);
      ok = false;
      break;
    }

    ast_t* tp_node = rec->typeparam;
    ast_t* defarg = ast_childidx(tp_node, 2);

    if(ast_id(defarg) != TK_NONE)
    {
      // Reify the default against the decided prefix.
      ast_t* reified_default = reify_default(tp_node, typeparams,
        typeargs, opt);

      if(reified_default != NULL)
      {
        // Postcondition: the reified default must not mention any type
        // parameter from position i onward.
        const char* dangling_name = find_dangling_default_ref(
          reified_default, typeparams, i);
        if(dangling_name != NULL)
        {
          if(rec->state == REC_BOUND || rec->state == REC_PINNED)
          {
            ast_free_unattached(reified_default);
            resolved = (rec->state == REC_BOUND) ?
              rec->bound.type : rec->pinned.type;
          }
          else
          {
                rec->state = REC_UNBOUND;
            rec->unbound.reason = REASON_DEFAULT_UNDECIDED;
            rec->unbound.anchor = tp_node;
            rec->unbound.detail = dangling_name;

            ast_free_unattached(reified_default);
            // Fall through to unbound reporting below.
            goto report_unbound_label;
          }
        }
        else
        {
          // Default is usable. Check if all arguments fit it.
          bool default_ok = true;

          if(rec->state == REC_BOUND || rec->state == REC_PINNED)
          {
            // Check argument assignability under the default.
            ast_t* p2 = ast_child(params);
            ast_t* a2 = ast_child(positional);
            size_t a2_idx = 0;

            while(p2 != NULL && a2 != NULL)
            {
              if(ast_id(a2) != TK_NONE)
              {
                ast_t* a2_type = ast_type(a2);

                if(!is_typecheck_error(a2_type) &&
                  ast_id(a2_type) != TK_LITERAL &&
                  ast_id(a2_type) != TK_OPERATORLITERAL &&
                  (dep_cache == NULL || !dep_cache[a2_idx]))
                {
                  ast_t* pt2 = ast_childidx(p2, 1);

                  if(type_mentions_typeparams(pt2, typeparams))
                  {
                    // Skip if this param also mentions undecided later params.
                    bool skip = false;
                    for(size_t j = i + 1; j < tp_count; j++)
                    {
                      if(records[j].state == REC_UNBOUND &&
                        type_mentions_one_typeparam(pt2,
                          records[j].typeparam))
                      {
                        skip = true;
                        break;
                      }
                    }

                    if(!skip && (other_typeparams == NULL ||
                      !type_mentions_typeparams(pt2, other_typeparams)))
                    {
                      // Reify the parameter type with the trial binding.
                      ast_t* trial_args = ast_dup(typeargs);
                      ast_append(trial_args, ast_dup(reified_default));
                      ast_t* trial_type = reify(pt2, typeparams,
                        trial_args, opt, true);

                      ast_t* wp = consume_type(trial_type, TK_NONE,
                        false, opt);

                      if(wp != NULL)
                      {
                        ast_t* aliased_arg = alias(a2_type, opt);
                        if(aliased_arg == NULL ||
                          ast_id(aliased_arg) == TK_ALIASED)
                        {
                          if(aliased_arg != NULL)
                            ast_free_unattached(aliased_arg);
                          aliased_arg = a2_type;
                        }

                        errorframe_t info = NULL;
                        if(!is_subtype(aliased_arg, wp, &info, opt))
                          default_ok = false;

                        errorframe_discard(&info);

                        if(aliased_arg != a2_type)
                          ast_free_unattached(aliased_arg);

                        ast_free_unattached(wp);
                      }
                      else
                      {
                        default_ok = false;
                      }

                      ast_free_unattached(trial_type);
                      ast_free_unattached(trial_args);
                    }
                  }
                }
              }

              p2 = ast_sibling(p2);
              a2 = ast_sibling(a2);
              a2_idx++;
            }
          }

          if(default_ok)
          {
            resolved = reified_default;
            resolved_is_owned = true;
          }
          else
          {
            ast_free_unattached(reified_default);
            reified_default = NULL;

            if(rec->state == REC_BOUND)
              resolved = rec->bound.type;
            else if(rec->state == REC_PINNED)
              resolved = rec->pinned.type;
          }

        }
      }
      else if(rec->state == REC_BOUND || rec->state == REC_PINNED)
      {
        resolved = (rec->state == REC_BOUND) ?
          rec->bound.type : rec->pinned.type;
      }
    }
    else
    {
      if(rec->state == REC_BOUND)
        resolved = rec->bound.type;
      else if(rec->state == REC_PINNED)
        resolved = rec->pinned.type;
    }

report_unbound_label:
    if(resolved == NULL && rec->state == REC_UNBOUND)
    {
      // An argument with no type already reported its own error; exit
      // silently so the inference message doesn't pile on.
      if(has_untyped)
      {
        ast_free_unattached(typeargs);
        free_records(records, tp_count, alloc_size);
        if(dep_cache != NULL)
          ponyint_pool_free_size(arg_count * sizeof(bool), dep_cache);
        return NULL;
      }

      ast_error(opt->check.errors, error_at,
        "could not infer the type arguments of '%s' from this call's "
        "arguments", callee_name);

      report_unbound(opt, error_at, callee_name, rec);

      ast_error_continue(opt->check.errors, error_at,
        "write the type arguments explicitly, as in '%s[T](...)'",
        callee_name);

      ok = false;
      break;
    }

    if(resolved == NULL)
    {
      // Should not get here — means conflict was already reported.
      ok = false;
      break;
    }

    ast_t* ta = ast_dup(resolved);

    if(resolved_is_owned)
      ast_free_unattached(resolved);

    if(rec->state == REC_BOUND && rec->bound.arg != NULL)
    {
      ast_setpos(ta, ast_source(rec->bound.arg),
        ast_line(rec->bound.arg), ast_pos(rec->bound.arg));
    }
    else if(rec->state == REC_PINNED && rec->pinned.arg != NULL)
    {
      ast_setpos(ta, ast_source(rec->pinned.arg),
        ast_line(rec->pinned.arg), ast_pos(rec->pinned.arg));
    }

    ast_append(typeargs, ta);
  }

  free_records(records, tp_count, alloc_size);
  if(dep_cache != NULL)
    ponyint_pool_free_size(arg_count * sizeof(bool), dep_cache);

  if(!ok)
  {
    ast_free_unattached(typeargs);
    return NULL;
  }

  return typeargs;
}
