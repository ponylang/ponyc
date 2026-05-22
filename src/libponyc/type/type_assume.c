#include "type_assume.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"


// One entry on the assumption stack. The stored ASTs are raw pointers
// into caller-owned AST trees; the caller is contractually required to
// keep them alive until the matching type_assume_leave returns. See the
// API docstring in type_assume.h for the full lifetime contract.
typedef struct
{
  ast_t* a;
  ast_t* b;  // NULL for single-argument operations
} assume_entry_t;


// Per-op assumption stack. Grow-and-keep: the buffer's capacity is
// preserved between top-level subtype-machinery calls so subsequent
// calls reuse the allocation. The stack is logically empty between
// top-level entries (every push pairs with a pop), but the buffer
// stays for the lifetime of the thread.
typedef struct
{
  assume_entry_t* entries;
  size_t len;
  size_t cap;
} assume_stack_t;


static __pony_thread_local assume_stack_t assume_stacks[TYPE_ASSUME_OP_COUNT];


// True when the query '(a, b)' matches a stored entry. NULL on either
// side is the sentinel for a single-argument operation; matching NULL
// requires the stored slot to be NULL too.
//
// The comparison is purely structural: type-ref kinds match on ast_data
// def-pointer plus structurally-equivalent typeargs; compound types
// match by walking children pairwise. Cap and eph are intentionally
// omitted from the type-ref comparison — two queries that differ only
// by cap or eph match as the same assumption. Cycle equality is
// weaker than answer equality: a false-positive cycle hit returns the
// co-inductive `true`, and the surrounding is_x_sub_x_impl machinery's
// own cap/eph reasoning still arbitrates the final answer. The subtype
// cache's fingerprint (subtype_cache.c::fingerprint_node) does include
// cap and eph because the cache stores answers, not assumptions, and
// answer equality is the stronger relation.
//
// Cycle detection only needs to recognize the same query starting
// again — semantic equivalence (is_eqtype) is not required, and using
// it here would re-enter the subtype machinery on every typearg, which
// on mutually-recursive alias SCCs branches exponentially through the
// ring (an SCC-3 ring of size N=3 produced ~25M type_assume_enter
// calls and 10M is_eqtype re-entries through this matcher before the
// matcher was made structural). Structural matching is conservative
// in the other direction too — it may fail to recognize an
// equivalent-but-not-identical query as a cycle and let the recursion
// descend an extra step before some other check terminates it, which
// is safe; the alternative (treating equivalent queries as the same)
// is what the cost came from.
static bool is_assumption_match(ast_t* query, ast_t* stored)
{
  if((query == NULL) || (stored == NULL))
    return query == stored;

  token_id qid = ast_id(query);
  if(qid != ast_id(stored))
    return false;

  switch(qid)
  {
    case TK_NONE:
      return true;

    case TK_NOMINAL:
    case TK_TYPEALIASREF:
    {
      if(ast_data(query) != ast_data(stored))
        return false;

      // typeargs at index 2 for TK_NOMINAL, index 1 for TK_TYPEALIASREF
      size_t idx = (qid == TK_NOMINAL) ? 2 : 1;
      ast_t* q_arg = ast_child(ast_childidx(query, idx));
      ast_t* s_arg = ast_child(ast_childidx(stored, idx));

      while((q_arg != NULL) && (s_arg != NULL))
      {
        if(!is_assumption_match(q_arg, s_arg))
          return false;

        q_arg = ast_sibling(q_arg);
        s_arg = ast_sibling(s_arg);
      }

      return (q_arg == NULL) && (s_arg == NULL);
    }

    case TK_TYPEPARAMREF:
      return ast_data(query) == ast_data(stored);

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* q_child = ast_child(query);
      ast_t* s_child = ast_child(stored);

      while((q_child != NULL) && (s_child != NULL))
      {
        if(!is_assumption_match(q_child, s_child))
          return false;

        q_child = ast_sibling(q_child);
        s_child = ast_sibling(s_child);
      }

      return (q_child == NULL) && (s_child == NULL);
    }

    case TK_ARROW:
    {
      ast_t* q_lhs = ast_child(query);
      ast_t* q_rhs = ast_sibling(q_lhs);
      ast_t* s_lhs = ast_child(stored);
      ast_t* s_rhs = ast_sibling(s_lhs);

      return is_assumption_match(q_lhs, s_lhs)
        && is_assumption_match(q_rhs, s_rhs);
    }

    // Capability and viewpoint leaves. They appear as the LHS of a
    // TK_ARROW, where two arrows with matching LHS caps and matching
    // RHS types are the same assumption. Two leaf nodes of the same
    // kind have no children to compare further, so kind equality
    // (already established by the qid check above) is the answer.
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
    case TK_CAP_READ:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ALIAS:
    case TK_CAP_ANY:
    case TK_THISTYPE:
      return true;

    default:
      return false;
  }
}


// Append '(a, b)' to op's stack, growing the buffer if needed.
static void push_assumption(type_assume_op_t op, ast_t* a, ast_t* b)
{
  pony_assert(op < TYPE_ASSUME_OP_COUNT);
  pony_assert(a != NULL);

  assume_stack_t* stack = &assume_stacks[op];

  if(stack->len == stack->cap)
  {
    size_t new_cap = (stack->cap == 0) ? 8 : (stack->cap * 2);
    size_t old_size = stack->cap * sizeof(assume_entry_t);
    size_t new_size = new_cap * sizeof(assume_entry_t);
    stack->entries = (assume_entry_t*)ponyint_pool_realloc_size(
      old_size, new_size, stack->entries);
    stack->cap = new_cap;
  }

  stack->entries[stack->len].a = a;
  stack->entries[stack->len].b = b;
  stack->len++;
}


int type_assume_enter_indexed(type_assume_op_t op, ast_t* a, ast_t* b)
{
  pony_assert(op < TYPE_ASSUME_OP_COUNT);
  pony_assert(a != NULL);

  // Walk existing entries first; if any matches the query, the caller
  // is on a cycle and we return its index without pushing.
  // is_assumption_match is purely structural and never re-enters the
  // subtype machinery, so walking before pushing is safe (older
  // revisions pushed first because is_eqtype-based matching could
  // recursively call back into this API).
  assume_stack_t* stack = &assume_stacks[op];
  for(size_t i = 0; i < stack->len; i++)
  {
    if(is_assumption_match(a, stack->entries[i].a) &&
      is_assumption_match(b, stack->entries[i].b))
    {
      return (int)i;
    }
  }

  push_assumption(op, a, b);
  return -1;
}


bool type_assume_enter(type_assume_op_t op, ast_t* a, ast_t* b)
{
  return type_assume_enter_indexed(op, a, b) < 0;
}


void type_assume_leave(type_assume_op_t op)
{
  pony_assert(op < TYPE_ASSUME_OP_COUNT);

  // Stack must be non-empty: caller pairs leave only with a true return
  // from enter for this op. An empty stack means the caller either
  // passed the wrong op or didn't check the enter return.
  assume_stack_t* stack = &assume_stacks[op];
  pony_assert(stack->len > 0);

  stack->len--;
}


size_t type_assume_depth(type_assume_op_t op)
{
  pony_assert(op < TYPE_ASSUME_OP_COUNT);
  return assume_stacks[op].len;
}


size_t type_assume_same_def_count(type_assume_op_t op, void* a_data,
  void* b_data)
{
  pony_assert(op < TYPE_ASSUME_OP_COUNT);
  // Definition pointer equality has no meaningful answer when 'a_data'
  // is NULL — every entry whose stored 'a' has NULL ast_data (compound
  // types like TK_TUPLETYPE) would match, which is never what a caller
  // wants. Force callers to pass non-NULL.
  pony_assert(a_data != NULL);

  assume_stack_t* stack = &assume_stacks[op];
  size_t count = 0;
  for(size_t i = 0; i < stack->len; i++)
  {
    if(ast_data(stack->entries[i].a) != a_data)
      continue;

    // NULL-equality on the b-side: matching NULL requires both to be
    // NULL. For two-argument operations the caller passes the queried
    // def pointer; for single-argument operations the caller passes
    // NULL and the entry's NULL 'b' matches.
    ast_t* stored_b = stack->entries[i].b;
    if(b_data == NULL)
    {
      if(stored_b != NULL)
        continue;
    }
    else
    {
      if((stored_b == NULL) || (ast_data(stored_b) != b_data))
        continue;
    }

    count++;
  }

  return count;
}
