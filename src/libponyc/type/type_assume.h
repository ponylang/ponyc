#ifndef TYPE_ASSUME_H
#define TYPE_ASSUME_H

#include <platform.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Operation key for the shared assume stack.
 *
 * Each operation that needs co-inductive cycle protection gets its own key.
 * Pushing an assumption under key X is invisible to checks under any other
 * key.
 */
typedef enum
{
  TYPE_ASSUME_SUBTYPE,
  TYPE_ASSUME_VIEWPOINT,

  // Sentinel: number of real keys. Not a valid op. The per-op stack
  // array is sized by this value.
  //
  // Add a key only for an op that can re-enter on recursive type
  // structure that PASS_TYPEALIAS_RECURSION (pass/typealias_recursion.c)
  // does not already bound. The legality pass rejects every alias shape
  // whose recursive arm sits outside a TK_NOMINAL typearg, so an op
  // that only decomposes unions, intersections, tuples, arrows, and
  // typealiasref unfolds cannot reach a cycle on legal Pony — adding a
  // key for one would be defense against unreachable code. Add new
  // keys before this line.
  TYPE_ASSUME_OP_COUNT
} type_assume_op_t;

/**
 * Cycle base-case soundness survey
 *
 * Every consumer of this API picks a value to return when a cycle is
 * detected. The choice has to be consistent: if we assume the result
 * for the cycle pair is X and the recursive computation produces a
 * result consistent with X under that assumption at every recurrence
 * point, then by co-induction the actual result is X.
 *
 * The set is exhaustive because PASS_TYPEALIAS_RECURSION
 * (pass/typealias_recursion.c) bounds every other recursive type-
 * structural op the compiler runs: it rejects alias shapes whose
 * recursive arm doesn't thread through a TK_NOMINAL typearg, and the
 * remaining ops decompose only non-nominal structure that bottoms out
 * after one unfold. Earlier rounds wrapped twelve other ops here as
 * defense in depth; once the legality pass was strengthened those
 * wrappers became unreachable and were removed.
 *
 * - **is_x_sub_x (subtype.c)** returns 'true'. Subtype is a greatest
 *   fixed point: assuming A <: B and showing all per-rule subgoals
 *   hold under that assumption gives A <: B by co-induction. Carries
 *   a divergence guard (SAME_DEF_LIMIT in subtype.c) for drifting
 *   same-def chains on recursive generic interfaces (ponylang/ponyc
 *   #1216). If a future op is found to exhibit drifting same-def
 *   recursion, add an analogous guard at its entry point using
 *   type_assume_same_def_count.
 *
 * - **viewpoint_type (viewpoint.c)** returns ast_dup(r_type).
 *   Viewpoint adaptation of a self-referent value propagates the rhs
 *   identity at the recurrence point.
 */

/**
 * Note on the shared API with only two consumers
 *
 * The shared op-keyed mechanism may look heavy for two consumers.
 * It stays shared because is_assumption_match — the structural pair
 * comparator in type_assume.c, ~90 lines of subtle equality logic
 * tuned to avoid re-entering the subtype machinery on every typearg
 * — is too critical to duplicate. Silent drift between two copies of
 * that matcher would be very hard to catch and very bad if it
 * happened.
 */

/**
 * Note on codegen/genname.c
 *
 * genname.c has its own thread-local cycle-protection mechanism for
 * type_append rather than using this API. Not an oversight — the two
 * solve different problems:
 *
 *   - This API tracks type pairs '(op, a, b)' by structural identity,
 *     returns a bool/sentinel to the caller on cycle, and has no
 *     depth bound.
 *
 *   - genname tracks alias defs by pointer (it only needs "have I
 *     started naming this def?"), and on cycle emits a placeholder
 *     name string mid-walk and continues. It also caps depth at a
 *     soft limit as a defensive guard against pathological non-cyclic
 *     alias chains, since codegen is producing a string and gracefully
 *     truncating is preferable to looping.
 *
 * Forcing genname through this API would pay for structural identity
 * it doesn't need, lose the depth cap, and reshape "emit placeholder
 * mid-string" into the bool/sentinel return shape. Leave it local.
 */

/**
 * Cycle-protection entry for a recursive type-structural operation.
 *
 * Use at every recursive entry point of the operation:
 *
 *     if(!type_assume_enter(op, a, b))
 *       return <co-inductive base case>;
 *     ... compute the recursive answer ...
 *     type_assume_leave(op);
 *
 * Returns false when the assumption '(a, b)' is already on the stack
 * for this operation — the caller has reached a cycle and must return
 * the operation's base case. Returning a base case that is consistent
 * at every recurrence point yields the answer by co-induction. The
 * cycle path has nothing to leave; calling type_assume_leave on the
 * false branch would underflow the stack and asserts.
 *
 * Returns true after pushing '(a, b)' onto the stack. The caller must
 * pair a true return with exactly one type_assume_leave for the same
 * op.
 *
 * 'a' is any type AST node. 'b' is either another type AST node
 * (pairwise operations) or NULL (single-argument operations). The
 * stack stores both as raw pointers; the caller MUST keep both AST
 * trees alive until the matching type_assume_leave returns. Every
 * existing consumer satisfies this automatically because the inputs
 * are caller's own arguments or local references that stay scoped
 * through the recursive call.
 *
 * Two assumptions match by structural identity (see is_assumption_match
 * in type_assume.c): same node kinds, equal definition pointers for
 * type-ref nodes, structurally equivalent typeargs and children, and
 * aligned NULL slots. Semantic equivalence (is_eqtype) is intentionally
 * not used — see the comment on is_assumption_match for why.
 *
 * The canonical use site is is_x_sub_x in subtype.c.
 *
 * Performance note. is_x_sub_x in subtype.c gates enter on a guard
 * predicate (TK_TYPEALIASREF or both-TK_NOMINAL operands) and only
 * pays for the stack walk and push on shapes that can actually form
 * cycles. viewpoint_type in viewpoint.c calls enter on every
 * invocation — adding a predicate is straightforward but the perf
 * win has not been measured to be material at typecheck scale.
 */
bool type_assume_enter(type_assume_op_t op, ast_t* a, ast_t* b);

/**
 * Index-returning variant of type_assume_enter.
 *
 * Returns -1 after pushing '(a, b)' onto op's stack (caller continues
 * the recursive computation and pairs with type_assume_leave). Returns
 * the matched stack index (>= 0) when the query already matches a
 * stored entry — caller is on a cycle and must return the operation's
 * base case without calling type_assume_leave.
 *
 * Same lifetime, matching, and pairing contracts as type_assume_enter.
 *
 * Callers that need to know *which* assumption fired the cycle (e.g.,
 * is_x_sub_x's intra-call cache, which classifies a frame's result by
 * the smallest stack index any cycle base case fired against) use this
 * variant. Callers that only need the bool can use type_assume_enter,
 * which is now a one-line wrapper.
 */
int type_assume_enter_indexed(type_assume_op_t op, ast_t* a, ast_t* b);

/**
 * Pop the most recent assumption from op's stack. Pairs with a true
 * return from type_assume_enter (or a -1 return from
 * type_assume_enter_indexed); calling on a cycle-hit path underflows
 * the per-op stack and asserts. The op kind passed here must match
 * the op the caller passed to enter.
 */
void type_assume_leave(type_assume_op_t op);

/**
 * Current depth of op's assumption stack on the calling thread.
 *
 * Returns the number of pushed assumptions for op (the index where
 * the next push would land). Used by is_x_sub_x to decide whether
 * it is at the top-level boundary (depth == 0) and to classify
 * cycle-hit indices relative to its own pushed entry.
 */
size_t type_assume_depth(type_assume_op_t op);

/**
 * Count entries on op's stack whose stored AST nodes have the
 * specified definition pointers.
 *
 * Walks the per-thread stack for op and counts entries where
 * 'ast_data(entry.a) == a_data'. 'a_data' must be non-NULL: this
 * function is meant for definition-pointer matching, and NULL is the
 * shared ast_data of every compound type (tuple, union, intersect),
 * which would be a meaningless match. For pairwise operations, also
 * requires 'ast_data(entry.b) == b_data'. The 'b_data' parameter
 * uses NULL-equality semantics: pass NULL when querying single-
 * argument operations (entries with NULL 'b' match), or when the
 * caller has no def pointer to compare against.
 *
 * Used by is_x_sub_x in subtype.c to detect "drifting same-def"
 * recursion chains. Recursive generic interfaces (ponylang/ponyc#1216)
 * produce subtype queries where each level shares the same def
 * pointer pair but has strictly larger typeargs, so the structural
 * matcher in is_assumption_match never fires a cycle hit. Counting
 * same-def ancestors lets the consumer bound such chains with an
 * empirical threshold.
 *
 * The walk is O(stack depth). Callers that pay this on every call
 * should gate it on a cheap predicate (the recurrence-prone shapes)
 * to avoid quadratic costs at typecheck scale.
 */
size_t type_assume_same_def_count(type_assume_op_t op, void* a_data,
  void* b_data);

PONY_EXTERN_C_END

#endif
