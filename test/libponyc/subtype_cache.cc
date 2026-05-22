#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype_cache.h>
#include <ast/ast.h>
#include <limits.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class SubtypeCacheTest : public PassTest
{};


// Single shared source. Types are exposed via an interface method's
// parameter list (canonical pattern, see type_check_subtype.cc): each
// parameter has a unique name and a typed annotation, so type_of(name)
// returns one well-defined AST per call.
//
// Coverage targets correspond to fingerprint_node branches in
// subtype_cache.c:
//   - bare TK_NOMINAL                          → t_u32, t_str
//   - cap variation (TK_NOMINAL ast_id(cap))   → t_ref, t_val
//   - eph variation (TK_NOMINAL ast_id(eph))   → t_iso, t_iso_eph
//   - TK_NOMINAL with typeargs                 → t_arr
//   - TK_UNIONTYPE                             → t_union, t_union3
//   - TK_TUPLETYPE                             → t_tuple
//   - TK_TYPEPARAMREF                          → t_param (via TestGeneric[A])
//   - NULL nodes and inline-FP storage path    → exercised directly via
//                                                subtype_cache_lookup(NULL, NULL, 0)
static const char* TEST_SRC =
  "interface Test\n"
  "  fun z(t_u32: U32, t_str: String,\n"
  "        t_ref: String ref, t_val: String val,\n"
  "        t_iso: String iso, t_iso_eph: String iso^,\n"
  "        t_arr: Array[U32],\n"
  "        t_union: (U32 | String),\n"
  "        t_union3: (U32 | String | F32),\n"
  "        t_tuple: (U32, String))\n"
  "interface TestGeneric[A]\n"
  "  fun z2(t_param: A)";


// Clear timing. subtype_cache_clear runs at every is_x_sub_x depth-0
// entry; test_compile exercises subtype checks during typecheck and
// leaves cache state from compile behind. Each test calls
// subtype_cache_clear() after TEST_COMPILE (and any type_of calls) and
// immediately before its own first cache call so the test's writes are
// not contaminated by compile-time entries. The two accumulator tests
// (MinMatchIdxGetSetRoundtrip, SubtreePoisonedGetSetRoundtrip) skip
// TEST_COMPILE entirely — they touch only thread-local accumulators,
// not the hashmap, so no AST is needed.
//
// out reads on miss. subtype_cache_lookup leaves *out unmodified on
// miss (per the header docstring). Miss-asserting tests assert only on
// the return value and do not read out.result / out.kind after a miss.


TEST_F(SubtypeCacheTest, ClearEmptiesCache)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_u32, 0, true, CACHE_UNCONDITIONAL);

  // Positive control: entry is present before clear.
  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_u32, 0, &out));

  subtype_cache_clear();

  // Contract: clear drops every entry.
  ASSERT_FALSE(subtype_cache_lookup(t_u32, t_u32, 0, &out));
}


TEST_F(SubtypeCacheTest, InsertThenLookupHitsUnconditional)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, true, CACHE_UNCONDITIONAL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_str, 0, &out));
  EXPECT_TRUE(out.result);
  EXPECT_EQ(CACHE_UNCONDITIONAL, out.kind);
}


TEST_F(SubtypeCacheTest, InsertThenLookupHitsDependsOnTopLevel)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, false, CACHE_DEPENDS_ON_TOP_LEVEL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_str, 0, &out));
  EXPECT_FALSE(out.result);
  EXPECT_EQ(CACHE_DEPENDS_ON_TOP_LEVEL, out.kind);
}


TEST_F(SubtypeCacheTest, SubSuperOrderMatters)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, true, CACHE_UNCONDITIONAL);

  // Positive control: same-order key hits.
  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_str, 0, &out));

  // Contract: swapped-order key does not collide.
  ASSERT_FALSE(subtype_cache_lookup(t_str, t_u32, 0, &out));
}


TEST_F(SubtypeCacheTest, CheckCapDistinguishes)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, true, CACHE_UNCONDITIONAL);

  // Positive control: same check_cap hits.
  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_str, 0, &out));

  // Contract: different check_cap does not collide.
  ASSERT_FALSE(subtype_cache_lookup(t_u32, t_str, 1, &out));
}


TEST_F(SubtypeCacheTest, StructurallyEquivalentDistinctPointersHit)
{
  // The central MED-6 anchor: ast_dup produces structurally-identical
  // ASTs at different pointers (this is what typealias_unfold does via
  // reify -> ast_dup). The cache must hit despite the pointer change
  // because fingerprinting is structural, not pointer-keyed.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, true, CACHE_UNCONDITIONAL);

  ast_t* t_u32_dup = ast_dup(t_u32);
  ast_t* t_str_dup = ast_dup(t_str);
  ASSERT_NE(t_u32, t_u32_dup);
  ASSERT_NE(t_str, t_str_dup);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32_dup, t_str_dup, 0, &out));
  EXPECT_TRUE(out.result);
  EXPECT_EQ(CACHE_UNCONDITIONAL, out.kind);

  ast_free_unattached(t_u32_dup);
  ast_free_unattached(t_str_dup);
}


TEST_F(SubtypeCacheTest, StructurallyDifferentDoesNotHit)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_u32, 0, true, CACHE_UNCONDITIONAL);

  // Positive control: inserted key hits.
  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_u32, 0, &out));

  // Contract: different shape does not collide.
  ASSERT_FALSE(subtype_cache_lookup(t_u32, t_str, 0, &out));
  ASSERT_FALSE(subtype_cache_lookup(t_str, t_u32, 0, &out));
}


TEST_F(SubtypeCacheTest, CapDifferenceDistinguishes)
{
  // Locks in that cap is part of the fingerprint (eph is locked in
  // separately by EphDistinguishes). is_assumption_match omits both
  // cap and eph (cycle equality is weaker than answer equality), but
  // the cache stores answers and must distinguish them.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_ref = type_of("t_ref");
  ast_t* t_val = type_of("t_val");

  subtype_cache_clear();
  subtype_cache_insert(t_ref, t_ref, 0, true, CACHE_UNCONDITIONAL);

  // Positive control: same-cap key hits.
  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_ref, t_ref, 0, &out));

  // Contract: different cap on same def does not collide.
  ASSERT_FALSE(subtype_cache_lookup(t_val, t_val, 0, &out));
}


TEST_F(SubtypeCacheTest, InsertReplaces)
{
  TEST_COMPILE(TEST_SRC);

  ast_t* t_u32 = type_of("t_u32");
  ast_t* t_str = type_of("t_str");

  subtype_cache_clear();
  subtype_cache_insert(t_u32, t_str, 0, false, CACHE_UNCONDITIONAL);
  subtype_cache_insert(t_u32, t_str, 0, true, CACHE_DEPENDS_ON_TOP_LEVEL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_u32, t_str, 0, &out));
  EXPECT_TRUE(out.result);
  EXPECT_EQ(CACHE_DEPENDS_ON_TOP_LEVEL, out.kind);
}


TEST_F(SubtypeCacheTest, MinMatchIdxGetSetRoundtrip)
{
  subtype_cache_clear();

  subtype_cache_min_match_idx_set(42);
  EXPECT_EQ(42, subtype_cache_min_match_idx_get());

  subtype_cache_min_match_idx_set(INT_MAX);
  EXPECT_EQ(INT_MAX, subtype_cache_min_match_idx_get());

  subtype_cache_min_match_idx_set(7);
  subtype_cache_clear();
  EXPECT_EQ(INT_MAX, subtype_cache_min_match_idx_get());
}


TEST_F(SubtypeCacheTest, EphDistinguishes)
{
  // Same def, same cap, different eph. Locks in that ast_id(eph) is
  // part of the TK_NOMINAL fingerprint encoding.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_iso = type_of("t_iso");
  ast_t* t_iso_eph = type_of("t_iso_eph");

  subtype_cache_clear();
  subtype_cache_insert(t_iso, t_iso, 0, true, CACHE_UNCONDITIONAL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_iso, t_iso, 0, &out));
  ASSERT_FALSE(subtype_cache_lookup(t_iso_eph, t_iso_eph, 0, &out));
}


TEST_F(SubtypeCacheTest, TypeargNominalStructuralHit)
{
  // Array[U32] — TK_NOMINAL with a typearg child. Exercises the
  // typeargs recursion in fingerprint_node. ast_dup deep-clones the
  // typeargs subtree, so the dup'd Array[U32] has different pointers
  // but identical structure; the cache must still hit.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_arr = type_of("t_arr");

  subtype_cache_clear();
  subtype_cache_insert(t_arr, t_arr, 0, true, CACHE_UNCONDITIONAL);

  ast_t* t_arr_dup = ast_dup(t_arr);
  ASSERT_NE(t_arr, t_arr_dup);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_arr_dup, t_arr_dup, 0, &out));

  ast_free_unattached(t_arr_dup);
}


TEST_F(SubtypeCacheTest, UnionStructuralHit)
{
  // TK_UNIONTYPE structural roundtrip. Exercises the child-count byte
  // + child recursion path.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_union = type_of("t_union");

  subtype_cache_clear();
  subtype_cache_insert(t_union, t_union, 0, true, CACHE_UNCONDITIONAL);

  ast_t* t_union_dup = ast_dup(t_union);
  ASSERT_NE(t_union, t_union_dup);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_union_dup, t_union_dup, 0, &out));

  ast_free_unattached(t_union_dup);
}


TEST_F(SubtypeCacheTest, UnionChildCountDistinguishes)
{
  // (U32 | String) vs (U32 | String | F32) — same TK_UNIONTYPE id,
  // different child counts. Locks in the child-count byte added by
  // fingerprint_node for compound nodes (without it (A, B) and
  // (A, B, C) would hash differently only after the byte stream
  // diverged, but the count byte makes the divergence immediate and
  // explicit).
  TEST_COMPILE(TEST_SRC);

  ast_t* t_union = type_of("t_union");
  ast_t* t_union3 = type_of("t_union3");

  subtype_cache_clear();
  subtype_cache_insert(t_union, t_union, 0, true, CACHE_UNCONDITIONAL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_union, t_union, 0, &out));
  ASSERT_FALSE(subtype_cache_lookup(t_union3, t_union3, 0, &out));
}


TEST_F(SubtypeCacheTest, TupleStructuralHit)
{
  // TK_TUPLETYPE structural roundtrip. Same compound-node path as
  // UNIONTYPE but a different node id, so we exercise the branch
  // and verify the id is part of the encoding.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_tuple = type_of("t_tuple");

  subtype_cache_clear();
  subtype_cache_insert(t_tuple, t_tuple, 0, true, CACHE_UNCONDITIONAL);

  ast_t* t_tuple_dup = ast_dup(t_tuple);
  ASSERT_NE(t_tuple, t_tuple_dup);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_tuple_dup, t_tuple_dup, 0, &out));

  ast_free_unattached(t_tuple_dup);
}


TEST_F(SubtypeCacheTest, TypeparamRefStructuralHit)
{
  // TK_TYPEPARAMREF — type parameter reference. ast_dup preserves the
  // def pointer, so the dup'd typeparamref fingerprints identically.
  TEST_COMPILE(TEST_SRC);

  ast_t* t_param = type_of("t_param");

  subtype_cache_clear();
  subtype_cache_insert(t_param, t_param, 0, true, CACHE_UNCONDITIONAL);

  ast_t* t_param_dup = ast_dup(t_param);
  ASSERT_NE(t_param, t_param_dup);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(t_param_dup, t_param_dup, 0, &out));

  ast_free_unattached(t_param_dup);
}


TEST_F(SubtypeCacheTest, NullPairLookup)
{
  // NULL nodes go through the FP_NULL sentinel branch — the only
  // direct way to exercise that branch. Also exercises the inline
  // (FP_INLINE_SIZE) fingerprint storage path: the total fingerprint
  // for (NULL, NULL, 0) is 9 bytes (4 + 4 + 1), well under the
  // 32-byte inline threshold.
  subtype_cache_clear();
  subtype_cache_insert(NULL, NULL, 0, true, CACHE_UNCONDITIONAL);

  subtype_cache_value_t out;
  ASSERT_TRUE(subtype_cache_lookup(NULL, NULL, 0, &out));
  EXPECT_TRUE(out.result);
  EXPECT_EQ(CACHE_UNCONDITIONAL, out.kind);
}


TEST_F(SubtypeCacheTest, OutUnmodifiedOnMiss)
{
  // Header docstring contract: on miss, subtype_cache_lookup leaves
  // *out unmodified. Pre-populate out with sentinels and verify they
  // survive a miss.
  subtype_cache_clear();

  subtype_cache_value_t out;
  out.result = true;
  out.kind = CACHE_DEPENDS_ON_TOP_LEVEL;

  ASSERT_FALSE(subtype_cache_lookup(NULL, NULL, 0, &out));
  EXPECT_TRUE(out.result);
  EXPECT_EQ(CACHE_DEPENDS_ON_TOP_LEVEL, out.kind);
}


TEST_F(SubtypeCacheTest, SubtreePoisonedGetSetRoundtrip)
{
  subtype_cache_clear();

  subtype_cache_subtree_poisoned_set(true);
  EXPECT_TRUE(subtype_cache_subtree_poisoned_get());

  subtype_cache_subtree_poisoned_set(false);
  EXPECT_FALSE(subtype_cache_subtree_poisoned_get());

  subtype_cache_subtree_poisoned_set(true);
  subtype_cache_clear();
  EXPECT_FALSE(subtype_cache_subtree_poisoned_get());
}
