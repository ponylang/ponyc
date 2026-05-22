#ifndef SUBTYPE_CACHE_H
#define SUBTYPE_CACHE_H

#include <platform.h>
#include <stdbool.h>
#include <stdint.h>
#include "../ast/ast.h"

PONY_EXTERN_C_BEGIN

/**
 * Intra-call memoization for is_x_sub_x.
 *
 * The exponential cost in is_x_sub_x is call count: along
 * is_eq_typeargs's bidirectional fork, the same
 * (sub, super, check_cap) triple is computed many times. This cache
 * collapses repeated work to a single computation per distinct triple
 * within one top-level subtype call.
 *
 * Lifetime is per top-level call. The cache is cleared at every
 * is_x_sub_x entry where assume_stacks[SUBTYPE].len == 0 (see
 * subtype_cache_clear). Cross-top-level caching is deferred to a
 * future round.
 *
 * Hit rate on typical code is low (~1-5% measured across stdlib,
 * subtype-heavy synthetics, and mutually-recursive type alias
 * networks). That is by design: the cache exists to bound the worst
 * case, not to speed up the common case. A low hit rate means the
 * workload didn't trigger the pathological pattern; the cache is
 * still there to keep that pattern bounded if it does.
 *
 * State is per-thread; all functions in this module read and write a
 * single thread-local cache plus a thread-local accumulator
 * (subtype_min_match_idx, see below). Callers do not pass any handle.
 */

/**
 * Cache validity flag for an entry.
 *
 * CACHE_UNCONDITIONAL  — the cached result is valid for the full
 *   lifetime of the cache. Recorded when the descent that produced
 *   the result either had no cycle hits, or had cycle hits only on
 *   its own pushed assumption or descendants thereof.
 *
 * CACHE_DEPENDS_ON_TOP_LEVEL — the cached result is only valid while
 *   the depth-0 stack entry that was on the assumption stack at
 *   insert time is still on the stack. The eager-clear-at-my_depth==0
 *   rule (see the cache-clear call near the top of is_x_sub_x in
 *   subtype.c) ensures any consultation happens under the same
 *   depth-0 entry that was active at insert time; the caller-side
 *   stack.len > 0 gate is documented on subtype_cache_lookup. The
 *   full soundness argument lives at the cache-clear call site.
 */
typedef enum
{
  CACHE_UNCONDITIONAL,
  CACHE_DEPENDS_ON_TOP_LEVEL
} subtype_cache_kind_t;

/**
 * Lookup result. Tag-as-enum, not sentinel-as-int, so the two valid
 * states are the only representable ones.
 */
typedef struct
{
  bool result;
  subtype_cache_kind_t kind;
} subtype_cache_value_t;

/**
 * Drop every entry in the cache and reset all heap-spilled
 * fingerprint storage. O(cache_size). Safe to call repeatedly.
 *
 * Soundness anchor for CACHE_DEPENDS_ON_TOP_LEVEL: must be called at
 * every is_x_sub_x entry where my_depth == 0. Without this, a
 * conditional entry produced under one depth-0 assumption could be
 * consulted under a different depth-0 assumption, leading to a wrong
 * answer.
 */
void subtype_cache_clear(void);

/**
 * Look up a cached result for (sub, super, check_cap).
 *
 * Returns true on hit, with the cached value written to *out. Returns
 * false on miss; *out is unmodified.
 *
 * The caller is responsible for the conditional-entry validity check:
 * a hit with kind == CACHE_DEPENDS_ON_TOP_LEVEL is only valid when
 * the SUBTYPE assumption stack is non-empty at lookup time. The
 * caller checks this explicitly because it has the assumption-stack
 * depth on hand at the call site (see is_x_sub_x).
 *
 * 'check_cap' is uint8_t rather than the file-local check_cap_t enum
 * in subtype.c so the cache module can stay decoupled from
 * subtype.c's internal types. The caller casts at the boundary.
 */
bool subtype_cache_lookup(ast_t* sub, ast_t* super, uint8_t check_cap,
  subtype_cache_value_t* out);

/**
 * Insert a result into the cache, replacing any prior entry for the
 * same (sub, super, check_cap).
 *
 * Calling pattern:
 *
 *   - kind == CACHE_UNCONDITIONAL when the producing descent's
 *     min_match_idx was INT_MAX or >= my_depth.
 *   - kind == CACHE_DEPENDS_ON_TOP_LEVEL when the producing descent's
 *     min_match_idx was 0.
 *   - Other min_match_idx values: do not insert (the entry would
 *     depend on an intermediate assumption that has no clean
 *     invalidation rule).
 */
void subtype_cache_insert(ast_t* sub, ast_t* super, uint8_t check_cap,
  bool result, subtype_cache_kind_t kind);

/**
 * Get the current value of the per-thread subtype_min_match_idx
 * accumulator.
 *
 * subtype_min_match_idx tracks the smallest assumption-stack index
 * that any cycle base case fired against during the current
 * is_x_sub_x frame's subtree. Threaded as a thread-local rather than
 * a parameter to keep is_x_sub_x_impl and its helpers
 * signature-clean; only the is_x_sub_x boundary save/restores it.
 *
 * Encoding:
 *   - INT_MAX: no cycle hits in the subtree.
 *   - 0: at least one cycle hit at the outermost stack entry.
 *   - n > 0: smallest cycle-hit index seen (used to classify the
 *     frame's result for caching).
 *
 * The recursion-divergence guard in is_x_sub_x signals "do not cache"
 * via a separate subtree_poisoned flag (see
 * subtype_cache_subtree_poisoned_get below) rather than overloading
 * this int domain.
 */
int subtype_cache_min_match_idx_get(void);

/**
 * Set the per-thread subtype_min_match_idx accumulator.
 *
 * Used by is_x_sub_x at frame entry (reset to INT_MAX before
 * descending) and frame exit (propagate min(saved, my_min) to the
 * caller). Used at cycle base case to record the matched index
 * (taking min with the existing value).
 */
void subtype_cache_min_match_idx_set(int v);

/**
 * Get the current value of the per-thread subtree_poisoned flag.
 *
 * subtree_poisoned tracks whether any descendant in the current
 * is_x_sub_x frame's subtree tripped the recursion-divergence guard
 * (see ponylang/ponyc#1216). The bailed `false` from that guard is a
 * conservative answer ("can't decide; default false"), not a real
 * subtype refutation, so caching it could give wrong answers in
 * another stack context within the same top-level call.
 *
 * is_x_sub_x reads this flag after its impl returns. When it is true,
 * the frame's result is not cached. The flag is propagated upward via
 * `saved_poisoned || my_poisoned` so ancestors that pick up a
 * poisoned subtree also skip caching. Sibling frames that reset the
 * flag to false before their own subtree runs cache normally on
 * their own subtree's result.
 *
 * Cleared by subtype_cache_clear; the soundness anchor is the same
 * as for the cache itself (clear runs at every depth-0 entry).
 */
bool subtype_cache_subtree_poisoned_get(void);

/**
 * Set the per-thread subtree_poisoned flag.
 *
 * Used by is_x_sub_x at frame entry (save the caller's value and
 * reset to false before descending) and frame exit (propagate
 * `saved_poisoned || my_poisoned` to the caller). Used by the
 * recursion-divergence guard in is_x_sub_x to set the flag when
 * bailing.
 */
void subtype_cache_subtree_poisoned_set(bool v);

PONY_EXTERN_C_END

#endif
