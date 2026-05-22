#include "subtype_cache.h"
#include "../ast/ast.h"
#include "../ast/token.h"
#include "../../libponyrt/ds/fun.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <limits.h>
#include <string.h>


// Inline-storage size for an entry's structural fingerprint.
// Fingerprints exceeding this length spill to a pool-allocated heap
// buffer (see entry_free / entry_fp); both inline and heap entries
// compare and free correctly. 32 bytes keeps the entry struct at
// exactly 64 bytes (one cache line on a 64-bit target); larger
// sizes push it to a second line. The fraction of entries that fit
// inline versus spill varies by workload; this is a sizing knob,
// not a correctness boundary.
#define FP_INLINE_SIZE 32

// Initial capacity of the thread-local fingerprint scratch buffer
// used for lookup keys. Grown on demand via realloc; never shrunk
// within a thread's lifetime. The starting size is chosen large
// enough that most lookup queries don't realloc; deeper nests pay
// one or more reallocs as the buffer grows.
#define FP_SCRATCH_INITIAL 128


typedef struct subtype_cache_entry_t
{
  size_t hash;
  size_t fp_len;
  uint8_t fp_inline[FP_INLINE_SIZE];
  uint8_t* fp_heap;       // NULL when fingerprint fits in fp_inline
  bool result;
  uint8_t kind;           // subtype_cache_kind_t, narrowed for size
} subtype_cache_entry_t;


static size_t entry_hash(subtype_cache_entry_t* e)
{
  return e->hash;
}


static const uint8_t* entry_fp(subtype_cache_entry_t* e)
{
  return (e->fp_heap != NULL) ? e->fp_heap : e->fp_inline;
}


static bool entry_cmp(subtype_cache_entry_t* a, subtype_cache_entry_t* b)
{
  if(a->hash != b->hash)
    return false;
  if(a->fp_len != b->fp_len)
    return false;
  return memcmp(entry_fp(a), entry_fp(b), a->fp_len) == 0;
}


static void entry_free(subtype_cache_entry_t* e)
{
  if(e->fp_heap != NULL)
    ponyint_pool_free_size(e->fp_len, e->fp_heap);
  POOL_FREE(subtype_cache_entry_t, e);
}


DECLARE_HASHMAP(subtype_cache_map, subtype_cache_map_t,
  subtype_cache_entry_t);
DEFINE_HASHMAP(subtype_cache_map, subtype_cache_map_t,
  subtype_cache_entry_t, entry_hash, entry_cmp, entry_free);


// One cache + accumulator + scratch buffer per thread. The cache lives
// for the thread's lifetime; subtype_cache_clear destroys and re-creates
// the hashmap with the same initial-capacity parameter (16) used at
// first-touch initialization. The hashmap doubles and rounds up to a
// power of two internally, so the bucket array is sized to keep room
// for that many elements without resizing — see ponyint_hashmap_init.
// The scratch buffer's heap allocation persists across clears: it is
// independent of the hashmap and is reset to length 0 only at the
// start of each fingerprint build, not by clear.
static __pony_thread_local subtype_cache_map_t cache;
static __pony_thread_local bool cache_initialized;

static __pony_thread_local int min_match_idx = INT_MAX;
static __pony_thread_local bool subtree_poisoned = false;

// Scratch fingerprint buffer used to build lookup keys without
// allocating a heap entry on every call. Grows monotonically.
static __pony_thread_local uint8_t* fp_scratch;
static __pony_thread_local size_t fp_scratch_cap;
static __pony_thread_local size_t fp_scratch_len;


static void scratch_reset(void)
{
  fp_scratch_len = 0;
}


static void scratch_grow(size_t needed)
{
  size_t new_cap = (fp_scratch_cap == 0) ? FP_SCRATCH_INITIAL : fp_scratch_cap;
  while(new_cap < needed)
    new_cap *= 2;

  fp_scratch = (uint8_t*)ponyint_pool_realloc_size(fp_scratch_cap, new_cap,
    fp_scratch);
  fp_scratch_cap = new_cap;
}


static void scratch_append(const void* src, size_t n)
{
  size_t needed = fp_scratch_len + n;
  if(needed > fp_scratch_cap)
    scratch_grow(needed);
  memcpy(fp_scratch + fp_scratch_len, src, n);
  fp_scratch_len = needed;
}


static void scratch_append_u32(uint32_t v)
{
  scratch_append(&v, sizeof(v));
}


static void scratch_append_ptr(const void* p)
{
  uintptr_t v = (uintptr_t)p;
  scratch_append(&v, sizeof(v));
}


static void scratch_append_u8(uint8_t v)
{
  scratch_append(&v, sizeof(v));
}


// Walk a type AST and append its structural fingerprint to the scratch
// buffer. The fingerprint must distinguish any two ASTs whose subtype
// answer differs.
//
// Why structural and not pointer-keyed. typealias_unfold returns fresh
// ASTs via reify -> ast_dup. Two unfolds of the same alias produce
// structurally-identical ASTs at different pointers, and the unfolded
// AST is freed (ast_free_unattached) before the top-level call exits.
// A pointer-keyed cache would miss every cross-unfold query (exactly
// the case the SCC descent generates) and dangle for any cached
// unfolded operand.
//
// Encoding:
//   - Every node: ast_id as uint32_t.
//   - Type-ref nodes (TK_NOMINAL, TK_TYPEALIASREF, TK_TYPEPARAMREF):
//     additionally append def-pointer + cap id + eph id. Cap and eph
//     are required because is_x_sub_x's answer depends on them — two
//     queries that differ only by cap can have different answers
//     (is_typeparam_sub_typeparam routes through is_sub_cap_and_eph;
//     typealias_unfold applies cap to the unfolded body). Diverges
//     intentionally from is_assumption_match (type_assume.c), which
//     stores cycle-detection assumptions rather than answers; see its
//     docstring for the soundness rationale.
//   - Compound nodes (UNION/ISECT/TUPLE): append child count, recurse
//     into each child. The count makes (A, B) and (A, B, C) hash
//     differently without a sentinel byte.
//   - TK_ARROW: recurse LHS then RHS (fixed shape, no count needed).
//   - All other nodes (cap leaves, TK_THISTYPE, TK_DONTCARETYPE,
//     TK_FUNTYPE, TK_INFERTYPE, TK_ERRORTYPE, TK_NONE, etc.): id only.
//     is_x_sub_x_impl resolves these to constant answers without
//     recursive descent, so the bare id distinguishes them.
//   - NULL nodes: a dedicated tag value (FP_NULL) so a missing
//     subtree never hashes the same as TK_NONE or any other token.
//
// Equality. The 64-bit hash gives ~10^-15 collision probability at
// cache sizes in the thousands, but to eliminate even rare false
// positives the cache stores a copy of the fingerprint sequence and
// compares it byte-for-byte before reporting a hit (see entry_cmp).
static void fingerprint_node(ast_t* node)
{
  // Sentinel for NULL — kept distinct from token_id values by using
  // a high bit no real token sets. token_id is an enum compiled as int
  // with values well below 0x80000000.
  static const uint32_t FP_NULL = 0xFFFFFFFFu;

  if(node == NULL)
  {
    scratch_append_u32(FP_NULL);
    return;
  }

  token_id id = ast_id(node);
  scratch_append_u32((uint32_t)id);

  switch(id)
  {
    case TK_NOMINAL:
    {
      scratch_append_ptr(ast_data(node));
      ast_t* cap = ast_childidx(node, 3);
      ast_t* eph = ast_childidx(node, 4);
      scratch_append_u32((uint32_t)ast_id(cap));
      scratch_append_u32((uint32_t)ast_id(eph));

      // Type arguments live as children of the typeargs sub-node at
      // index 2; that sub-node is TK_TYPEARGS when present and
      // TK_NONE when absent. ast_child of TK_NONE is NULL, yielding
      // count 0 — same as an empty typeargs list, which matches the
      // semantics.
      ast_t* typeargs = ast_childidx(node, 2);
      ast_t* arg = ast_child(typeargs);
      uint32_t count = 0;
      for(ast_t* c = arg; c != NULL; c = ast_sibling(c))
        count++;
      scratch_append_u32(count);

      while(arg != NULL)
      {
        fingerprint_node(arg);
        arg = ast_sibling(arg);
      }
      return;
    }

    case TK_TYPEALIASREF:
    {
      scratch_append_ptr(ast_data(node));
      ast_t* cap = ast_childidx(node, 2);
      ast_t* eph = ast_childidx(node, 3);
      scratch_append_u32((uint32_t)ast_id(cap));
      scratch_append_u32((uint32_t)ast_id(eph));

      ast_t* typeargs = ast_childidx(node, 1);
      ast_t* arg = ast_child(typeargs);
      uint32_t count = 0;
      for(ast_t* c = arg; c != NULL; c = ast_sibling(c))
        count++;
      scratch_append_u32(count);

      while(arg != NULL)
      {
        fingerprint_node(arg);
        arg = ast_sibling(arg);
      }
      return;
    }

    case TK_TYPEPARAMREF:
    {
      scratch_append_ptr(ast_data(node));
      ast_t* cap = ast_childidx(node, 1);
      ast_t* eph = ast_childidx(node, 2);
      scratch_append_u32((uint32_t)ast_id(cap));
      scratch_append_u32((uint32_t)ast_id(eph));
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* child = ast_child(node);
      uint32_t count = 0;
      for(ast_t* c = child; c != NULL; c = ast_sibling(c))
        count++;
      scratch_append_u32(count);

      while(child != NULL)
      {
        fingerprint_node(child);
        child = ast_sibling(child);
      }
      return;
    }

    case TK_ARROW:
    {
      ast_t* lhs = ast_child(node);
      ast_t* rhs = ast_sibling(lhs);
      fingerprint_node(lhs);
      fingerprint_node(rhs);
      return;
    }

    default:
      // Leaves with no further structure relevant to the subtype
      // answer: cap leaves under TK_ARROW, TK_THISTYPE,
      // TK_DONTCARETYPE, TK_FUNTYPE, TK_INFERTYPE, TK_ERRORTYPE, the
      // implicit TK_NONE used as a sentinel for missing optional
      // children. The id alone distinguishes them.
      return;
  }
}


// Build the fingerprint for (sub, super, check_cap) into the scratch
// buffer and return its hash. The caller may then build a temp entry
// pointing at the scratch buffer (for lookup) or copy it into a
// permanent entry (for insert).
static size_t build_fingerprint(ast_t* sub, ast_t* super, uint8_t check_cap)
{
  scratch_reset();
  fingerprint_node(sub);
  fingerprint_node(super);
  scratch_append_u8(check_cap);
  return ponyint_hash_block(fp_scratch, fp_scratch_len);
}


// Populate a stack-local entry from the current scratch buffer for use
// as a lookup key. The entry's fp storage points at the inline buffer
// when the fingerprint fits, otherwise temporarily borrows the heap
// scratch buffer (caller must not call any function that mutates the
// scratch buffer while the key is in use).
static void make_key(subtype_cache_entry_t* key, size_t hash)
{
  key->hash = hash;
  key->fp_len = fp_scratch_len;
  key->fp_heap = NULL;

  if(fp_scratch_len <= FP_INLINE_SIZE)
  {
    memcpy(key->fp_inline, fp_scratch, fp_scratch_len);
  }
  else
  {
    // Borrow the scratch buffer. The lookup completes before any other
    // scratch use, and we never insert directly from a key — insert
    // copies again from the scratch buffer.
    key->fp_heap = fp_scratch;
  }
}


static void ensure_initialized(void)
{
  if(!cache_initialized)
  {
    subtype_cache_map_init(&cache, 16);
    cache_initialized = true;
  }
}


void subtype_cache_clear(void)
{
  // Reset accumulator and poison flag unconditionally: even when the
  // cache hasn't been initialized on this thread, a prior top-level
  // call may have left them in a non-default state. is_x_sub_x's
  // save/restore at frame entry already isolates each top-level call
  // from the previous one in normal flow; this is the defensive
  // backstop.
  min_match_idx = INT_MAX;
  subtree_poisoned = false;

  if(!cache_initialized)
    return;

  // Iterate-and-free everything, then reset to an empty hashmap. We
  // call destroy/init rather than removeindex per slot because that
  // path is documented for the hashmap and avoids walking the bitmap
  // here.
  subtype_cache_map_destroy(&cache);
  subtype_cache_map_init(&cache, 16);
}


bool subtype_cache_lookup(ast_t* sub, ast_t* super, uint8_t check_cap,
  subtype_cache_value_t* out)
{
  ensure_initialized();

  size_t hash = build_fingerprint(sub, super, check_cap);

  subtype_cache_entry_t key;
  make_key(&key, hash);

  size_t index = HASHMAP_UNKNOWN;
  subtype_cache_entry_t* hit = subtype_cache_map_get(&cache, &key, &index);
  if(hit == NULL)
    return false;

  out->result = hit->result;
  out->kind = (subtype_cache_kind_t)hit->kind;
  return true;
}


void subtype_cache_insert(ast_t* sub, ast_t* super, uint8_t check_cap,
  bool result, subtype_cache_kind_t kind)
{
  ensure_initialized();

  size_t hash = build_fingerprint(sub, super, check_cap);

  subtype_cache_entry_t* entry = POOL_ALLOC(subtype_cache_entry_t);
  entry->hash = hash;
  entry->fp_len = fp_scratch_len;
  entry->result = result;
  entry->kind = (uint8_t)kind;

  if(fp_scratch_len <= FP_INLINE_SIZE)
  {
    memcpy(entry->fp_inline, fp_scratch, fp_scratch_len);
    entry->fp_heap = NULL;
  }
  else
  {
    entry->fp_heap = (uint8_t*)ponyint_pool_alloc_size(fp_scratch_len);
    memcpy(entry->fp_heap, fp_scratch, fp_scratch_len);
  }

  // _put returns the displaced entry (if a same-key entry already
  // existed) so we can free it. The hashmap's free_fn is not called
  // on _put displacement.
  subtype_cache_entry_t* displaced = subtype_cache_map_put(&cache, entry);
  if(displaced != NULL)
    entry_free(displaced);
}


int subtype_cache_min_match_idx_get(void)
{
  return min_match_idx;
}


void subtype_cache_min_match_idx_set(int v)
{
  min_match_idx = v;
}


bool subtype_cache_subtree_poisoned_get(void)
{
  return subtree_poisoned;
}


void subtype_cache_subtree_poisoned_set(bool v)
{
  subtree_poisoned = v;
}
