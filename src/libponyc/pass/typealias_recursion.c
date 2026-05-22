#include "typealias_recursion.h"
#include "../ast/error.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../pkg/package.h"
#include "../../libponyrt/ds/fun.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

#include <stdlib.h>
#include <string.h>


typedef struct alias_node_t alias_node_t;

struct alias_node_t
{
  ast_t* def;             // TK_TYPE definition

  // Adjacency in the alias-reference graph: every alias referenced
  // anywhere in 'def's body, excluding type-parameter constraints (D4)
  // and TK_ARROW LHS positions (D5).
  alias_node_t** all_edges;
  size_t all_edge_count;
  size_t all_edge_cap;

  // Subset of all_edges where the reference is reached through a
  // constructor (TK_NOMINAL typearg). Such edges are "productive" —
  // values can be built at finite depth by building the constructor's
  // other components first, with the recursive reference living
  // behind the heap pointer. Edges through union/intersection
  // members, top-level (the alias body itself), or tuple elements
  // are non-constructive: those positions inline the recursive
  // reference into the alias's own layout, so no finite layout
  // exists.
  //
  // The two subsets are not mutually exclusive: a target can appear
  // in both if the source body references it once at a non-
  // constructive position and once inside a TK_NOMINAL typearg
  // (e.g., `type X is (X | None | Array[X])` — bare X in union plus
  // X inside Array). The base-case check in check_base_cases rejects
  // any cycle whose edge appears in non_constructive_edges, even if
  // the same edge is also in constructive_edges; the bare arm makes
  // the cycle unsoundly recursive regardless of the productive arm.
  alias_node_t** constructive_edges;
  size_t constructive_edge_count;
  size_t constructive_edge_cap;

  // Subset of all_edges where the reference is reached through a
  // non-constructive position (top-level, union/intersection/tuple
  // member, or arrow RHS without a wrapping nominal). The presence
  // of any intra-SCC entry here means the cycle is not productive,
  // because the bare arm lets the recursion be taken without
  // adding structure.
  alias_node_t** non_constructive_edges;
  size_t non_constructive_edge_count;
  size_t non_constructive_edge_cap;

  // Tarjan's SCC state.
  ssize_t tarjan_index;
  ssize_t tarjan_lowlink;
  bool on_stack;
  ssize_t scc_id;

  // Position within the per-SCC member array (groups[scc_id] in
  // check_base_cases). Populated in the grouping loop, used by the
  // intra-SCC inner scans in check_base_cases and find_cycle_path
  // to look up a target's index without an O(scc_size) search per
  // edge. Valid only after check_base_cases's grouping loop runs;
  // initialized to SIZE_MAX so a pre-grouping read trips the
  // `target_idx < scc_size` assertion at every consumer (and in
  // release builds, indexes wildly out of bounds rather than
  // silently aliasing scc_members[0]).
  size_t scc_pos;

  // Per-list dedup tokens used by add_edge. Each field records the
  // most recent value of alias_graph_t::add_edge_gen at which this
  // node was inserted into the named list of some source's edges.
  // The graph's gen counter is bumped before each source's edge
  // collection, so when a node already shows the current gen on its
  // matching list field, the prospective insertion is a duplicate
  // within the same source's edge collection and is skipped. This
  // replaces a previous linear scan over the existing edges list per
  // insertion; the scan was O(deg^2) and an O(deg) hotspot on
  // synthetic 5000-arm union bodies. The three lists need
  // independent tracking because a target can legitimately appear in
  // all_edges and constructive_edges (or all_edges and
  // non_constructive_edges) for the same source.
  size_t add_edge_gen_all;
  size_t add_edge_gen_cons;
  size_t add_edge_gen_ncons;
};


// Hash a node by its def pointer. Two nodes with the same def hash
// alike; different defs hash to different slots with overwhelming
// probability for pointer values. Used by alias_index, the
// def → alias_node_t* map.
static size_t alias_node_hash(alias_node_t* n)
{
  return ponyint_hash_ptr(n->def);
}

// Equality on the def pointer only. Sufficient because every
// alias_node_t in the graph is keyed by a unique TK_TYPE def — the
// graph is built one node per def. Probe lookups construct a stack
// alias_node_t with only the def field set; any other field a future
// reader might add must NOT be referenced from this comparator.
static bool alias_node_cmp(alias_node_t* a, alias_node_t* b)
{
  return a->def == b->def;
}

DECLARE_HASHMAP(alias_index, alias_index_t, alias_node_t);
DEFINE_HASHMAP(alias_index, alias_index_t, alias_node_t,
  alias_node_hash, alias_node_cmp, NULL);


typedef struct alias_graph_t
{
  alias_node_t* nodes;
  size_t count;
  // Map from TK_TYPE def to its alias_node_t. Built once at graph
  // init in pass_typealias_recursion, queried throughout edge
  // collection and base-case scanning. Replaces a previous linear
  // scan that gave O(M*N) graph construction; a 100k illegal SCC
  // spent ~62% of its wall time in that scan (perf record on the
  // ring shape from gen_illegal_huge.sh). Lifetime is the pass:
  // initialized just after g->nodes is allocated, destroyed just
  // before free_graph.
  alias_index_t index;
  // Generation counter for add_edge dedup. Bumped before each
  // source's edge collection; compared against each target's
  // add_edge_gen_{all,cons,ncons} fields to detect duplicates.
  // Monotonically increasing across all sources, never reset.
  size_t add_edge_gen;
} alias_graph_t;


// Find the alias_node_t for a given TK_TYPE def, or NULL if not found.
// O(1) hashmap lookup against alias_graph_t::index, populated once at
// graph init. The probe key is a stack alias_node_t with only its def
// field set; alias_node_cmp reads only def. The index out-parameter
// is required by the underlying hashmap API even when unused.
static alias_node_t* node_for_def(alias_graph_t* g, ast_t* def)
{
  alias_node_t key;
  key.def = def;
  size_t index;
  return alias_index_get(&g->index, &key, &index);
}


// Walk the program AST collecting every TK_TYPE alias definition.
static void collect_aliases(ast_t* node, ast_t*** out_array,
  size_t* out_count, size_t* out_cap)
{
  if(node == NULL)
    return;

  if(ast_id(node) == TK_TYPE)
  {
    if(*out_count == *out_cap)
    {
      size_t new_cap = (*out_cap == 0) ? 16 : (*out_cap * 2);
      ast_t** new_arr = (ast_t**)ponyint_pool_alloc_size(
        new_cap * sizeof(ast_t*));
      if(*out_array != NULL)
      {
        memcpy(new_arr, *out_array, *out_count * sizeof(ast_t*));
        ponyint_pool_free_size(*out_cap * sizeof(ast_t*), *out_array);
      }
      *out_array = new_arr;
      *out_cap = new_cap;
    }
    (*out_array)[(*out_count)++] = node;
    return;
  }

  for(ast_t* child = ast_child(node); child != NULL;
    child = ast_sibling(child))
  {
    collect_aliases(child, out_array, out_count, out_cap);
  }
}


// Add an edge to a node's adjacency list, growing the array as
// needed. Skips duplicate edges — an alias mentioned twice in a body
// still contributes a single edge. Dedup is O(1) per insertion via
// the per-target/per-list gen field: 'target_seen_field' is the
// target node's add_edge_gen_X field corresponding to the list this
// call is updating, and 'current_gen' is alias_graph_t::add_edge_gen,
// which the caller bumps once per source. When the field already
// holds current_gen, the target is already in this source's matching
// list and the insertion is a duplicate. Insertion order is
// preserved: the appended position is monotonic in arrival order,
// which keeps downstream DFS traversal stable across versions of
// this code (tests assert on specific cycle path renderings).
static void add_edge(alias_node_t*** edges, size_t* count, size_t* cap,
  alias_node_t* target, size_t* target_seen_field, size_t current_gen)
{
  if(*target_seen_field == current_gen)
    return;
  *target_seen_field = current_gen;

  if(*count == *cap)
  {
    size_t new_cap = (*cap == 0) ? 4 : (*cap * 2);
    alias_node_t** new_arr = (alias_node_t**)ponyint_pool_alloc_size(
      new_cap * sizeof(alias_node_t*));
    if(*edges != NULL)
    {
      memcpy(new_arr, *edges, *count * sizeof(alias_node_t*));
      ponyint_pool_free_size(*cap * sizeof(alias_node_t*), *edges);
    }
    *edges = new_arr;
    *cap = new_cap;
  }
  (*edges)[(*count)++] = target;
}


// Walk a type body collecting every TK_TYPEALIASREF reference and
// classifying each by whether it's reached through a constructor that
// breaks the recursion at a heap pointer. Such a constructor exists
// only at TK_NOMINAL — class instances live behind a pointer, so
// recursion through a class's typeargs has a finite layout regardless
// of what's inside the typeargs.
//
// The 'constructive' flag is true once we're inside any TK_NOMINAL's
// typeargs and stays true through any compound shape (TK_UNIONTYPE,
// TK_ISECTTYPE, TK_TUPLETYPE, TK_ARROW) that wraps the reference.
// That's correct under the layout argument: once a nominal has
// introduced the heap break, additional inline-shaped wrapping
// inside the typeargs doesn't reintroduce inline self-reference.
//
// Outside any nominal, the flag is false — the alias body itself
// starts non-constructive, and compound shapes (union, intersection,
// tuple, arrow) don't introduce a constructor.
//
// Tuples specifically are inline value types: a top-level tuple body
// inlines its elements into the alias's own layout, so a tuple-
// element reference to the containing alias would require the
// alias's layout to inline itself transitively, which has no finite
// size. That's why a tuple at the body root makes its children
// non-constructive.
//
// Skipped entirely (D4, D5): type-parameter constraints and the LHS of
// TK_ARROW. D5 is enforced inside this walker (the TK_ARROW case only
// recurses into the RHS). D4 is enforced at the call site: collect_edges
// must be invoked on alias_body(def), which strips the typeparams. If
// you ever call this on a wider subtree of the def, D4 will silently
// break.
//
// Both 'all_edges' and 'constructive_edges' get the reference; only
// 'constructive_edges' depends on the position.
//
// Body lookup helper. Forward-declared; defined at the bottom of the
// file with the other small accessors.
static ast_t* alias_body(ast_t* def);

// Visited-defs set for alias-body traversals (base-case scanning,
// typeparam-use analysis). Bounds the walk: each alias is followed at
// most once per top-level scan. Heap-backed via the pool allocator;
// growth correctly updates the caller's view because the struct is
// passed by pointer.
typedef struct visited_defs_t
{
  ast_t** defs;
  size_t count;
  size_t cap;
} visited_defs_t;

static bool visited_contains(visited_defs_t* v, ast_t* def)
{
  for(size_t i = 0; i < v->count; i++)
  {
    if(v->defs[i] == def)
      return true;
  }
  return false;
}

static void visited_push(visited_defs_t* v, ast_t* def)
{
  if(v->count == v->cap)
  {
    size_t old_size = v->cap * sizeof(ast_t*);
    size_t new_cap = (v->cap == 0) ? 8 : v->cap * 2;
    size_t new_size = new_cap * sizeof(ast_t*);
    ast_t** new_defs = (ast_t**)ponyint_pool_alloc_size(new_size);
    if(v->defs != NULL)
    {
      memcpy(new_defs, v->defs, v->count * sizeof(ast_t*));
      ponyint_pool_free_size(old_size, v->defs);
    }
    v->defs = new_defs;
    v->cap = new_cap;
  }
  v->defs[v->count++] = def;
}

static void visited_free(visited_defs_t* v)
{
  if(v->defs != NULL)
    ponyint_pool_free_size(v->cap * sizeof(ast_t*), v->defs);
  v->defs = NULL;
  v->count = 0;
  v->cap = 0;
}

// Tri-state classification of how a typeparam is used in an alias's
// body. See typeparam_use_in_body's documentation for definitions.
typedef enum
{
  TP_UNUSED = 0,
  TP_USED_CONSTRUCTIVE_ONLY,
  TP_USED_NON_CONSTRUCTIVE
} typeparam_use_t;

// Forward declaration; defined after subtree_has_base_case_union etc.
static typeparam_use_t typeparam_use_in_body(ast_t* body, ast_t* tp_def,
  visited_defs_t* visited, bool currently_constructive);

static void collect_edges(alias_graph_t* g, ast_t* node, bool constructive,
  alias_node_t*** all_edges, size_t* all_count, size_t* all_cap,
  alias_node_t*** cons_edges, size_t* cons_count, size_t* cons_cap,
  alias_node_t*** ncons_edges, size_t* ncons_count, size_t* ncons_cap)
{
  if(node == NULL)
    return;

  if(ast_id(node) == TK_TYPEALIASREF)
  {
    ast_t* def = (ast_t*)ast_data(node);
    alias_node_t* target = node_for_def(g, def);
    if(target != NULL)
    {
      add_edge(all_edges, all_count, all_cap, target,
        &target->add_edge_gen_all, g->add_edge_gen);
      if(constructive)
        add_edge(cons_edges, cons_count, cons_cap, target,
          &target->add_edge_gen_cons, g->add_edge_gen);
      else
        add_edge(ncons_edges, ncons_count, ncons_cap, target,
          &target->add_edge_gen_ncons, g->add_edge_gen);
    }

    // For each typearg position, classify how the referenced alias
    // uses the corresponding typeparam in its body:
    //   TP_UNUSED            — alias discards the typearg; this slot
    //                          contributes no edges.
    //   TP_USED_CONSTRUCTIVE_ONLY — every appearance is inside a
    //                          nominal class's typeargs, so passing X
    //                          here puts X behind a heap pointer.
    //   TP_USED_NON_CONSTRUCTIVE — at least one appearance is at a
    //                          bare union/intersection/tuple/top
    //                          position, so passing X here does NOT
    //                          guarantee X is wrapped.
    // The constructive flag for the typearg is the OR of the outer
    // context with TP_USED_CONSTRUCTIVE_ONLY only — a wrap alias whose
    // typeparam is used both ways (`Wrap[T] is (T | Array[T] | None)`)
    // doesn't relabel X as constructive even though one of its uses
    // wraps T in Array. Skipping TP_UNUSED entirely avoids spurious
    // edges from phantom typeargs (`Wrap[T] is (None | Array[U64])`).
    ast_t* typeargs = ast_childidx(node, 1);
    if(def != NULL && ast_id(def) == TK_TYPE)
    {
      ast_t* def_body = alias_body(def);
      ast_t* def_typeparams = ast_childidx(def, 1);
      ast_t* def_tp = ast_child(def_typeparams);
      ast_t* ta = ast_child(typeargs);

      visited_defs_t visited = { NULL, 0, 0 };

      while(ta != NULL)
      {
        typeparam_use_t use = TP_UNUSED;
        if(def_body != NULL && def_tp != NULL)
        {
          visited.count = 0;
          use = typeparam_use_in_body(def_body, def_tp, &visited, false);
        }

        if(use != TP_UNUSED)
        {
          bool ta_flag = constructive ||
            (use == TP_USED_CONSTRUCTIVE_ONLY);
          collect_edges(g, ta, ta_flag, all_edges, all_count, all_cap,
            cons_edges, cons_count, cons_cap,
            ncons_edges, ncons_count, ncons_cap);
        }

        ta = ast_sibling(ta);
        if(def_tp != NULL) def_tp = ast_sibling(def_tp);
      }

      visited_free(&visited);
    }
    else
    {
      // Definition not resolved (defensive); fall back to the old
      // permissive behavior so we don't lose edges.
      collect_edges(g, typeargs, true, all_edges, all_count, all_cap,
        cons_edges, cons_count, cons_cap,
        ncons_edges, ncons_count, ncons_cap);
    }
    return;
  }

  switch(ast_id(node))
  {
    case TK_NOMINAL:
    {
      // Children are pkg, id, typeargs, cap, eph. typeargs is index 2;
      // recurse only into it, with constructive=true.
      ast_t* typeargs = ast_childidx(node, 2);
      collect_edges(g, typeargs, true, all_edges, all_count, all_cap,
        cons_edges, cons_count, cons_cap,
        ncons_edges, ncons_count, ncons_cap);
      return;
    }

    case TK_TUPLETYPE:
    {
      // Tuples don't introduce a constructor — they inline their
      // elements into the surrounding layout. Children inherit
      // whatever constructive flag we had on entry: non-constructive
      // at the alias body's top level (so a tuple at the body root
      // makes its children non-constructive), constructive inside a
      // nominal's typeargs (so Array[(A, None)] keeps A in a
      // constructive position because Array breaks the recursion at
      // the heap pointer regardless of the tuple wrapping).
      for(ast_t* child = ast_child(node); child != NULL;
        child = ast_sibling(child))
      {
        collect_edges(g, child, constructive, all_edges, all_count, all_cap,
          cons_edges, cons_count, cons_cap,
          ncons_edges, ncons_count, ncons_cap);
      }
      return;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // Same shape as TK_TUPLETYPE: union/intersection nodes don't
      // introduce a constructor, so children inherit the current
      // flag. A reference under a top-level union is non-constructive
      // (the alias body is non-constructive); a reference under a
      // union inside a nominal's typeargs stays constructive.
      for(ast_t* child = ast_child(node); child != NULL;
        child = ast_sibling(child))
      {
        collect_edges(g, child, constructive, all_edges, all_count, all_cap,
          cons_edges, cons_count, cons_cap,
          ncons_edges, ncons_count, ncons_cap);
      }
      return;
    }

    case TK_ARROW:
    {
      // RHS only (D5). The arrow itself isn't a constructor.
      collect_edges(g, ast_childidx(node, 1), constructive,
        all_edges, all_count, all_cap,
        cons_edges, cons_count, cons_cap,
        ncons_edges, ncons_count, ncons_cap);
      return;
    }

    default:
    {
      // Generic fallback: walk children with the same flag.
      for(ast_t* child = ast_child(node); child != NULL;
        child = ast_sibling(child))
      {
        collect_edges(g, child, constructive, all_edges, all_count, all_cap,
          cons_edges, cons_count, cons_cap,
          ncons_edges, ncons_count, ncons_cap);
      }
      return;
    }
  }
}


// Find a directed cycle through scc_members's intra-SCC edges,
// starting and ending at scc_members[0]. Writes node indices (within
// scc_members[0..scc_size]) in edge order to path_out, length to
// path_len_out. path[0] is always 0 and an implicit closing edge goes
// from path[path_len-1] back to path[0].
//
// Iterative 3-color DFS through all_edges; on the first edge that
// returns to scc_members[0], the current path is the cycle. on_path
// is the gray set (current DFS path); done is the black set (fully
// explored without reaching head). Skipping done nodes makes the
// search O(V+E): without the black set, dead-end subtrees get re-
// explored on every parent-side path that reaches them. Self-loops
// (single node referencing itself) produce path = [0].
//
// Caller contract: every member of scc_members has scc_pos == its
// index in the array, and scc_members[0]->scc_id is the SCC id
// shared by every member. This holds because find_cycle_path is
// called only from check_base_cases via render_cycle_path, after
// the per-SCC grouping loop populates scc_pos.
static void find_cycle_path(alias_node_t** scc_members, size_t scc_size,
  size_t* path_out, size_t* path_len_out)
{
  pony_assert(scc_size >= 1);

  bool* on_path = (bool*)ponyint_pool_alloc_size(scc_size * sizeof(bool));
  bool* done = (bool*)ponyint_pool_alloc_size(scc_size * sizeof(bool));
  memset(on_path, 0, scc_size * sizeof(bool));
  memset(done, 0, scc_size * sizeof(bool));
  size_t* edge_idx = (size_t*)ponyint_pool_alloc_size(
    scc_size * sizeof(size_t));

  ssize_t scc_id = scc_members[0]->scc_id;

  size_t depth = 0;
  path_out[depth] = 0;
  edge_idx[depth] = 0;
  on_path[0] = true;
  depth++;

  bool found = false;
  while(depth > 0 && !found)
  {
    size_t cur = path_out[depth - 1];
    alias_node_t* n = scc_members[cur];

    bool advanced = false;
    while(edge_idx[depth - 1] < n->all_edge_count)
    {
      alias_node_t* target = n->all_edges[edge_idx[depth - 1]++];

      // O(1) intra-SCC lookup via scc_pos; skip if not in SCC.
      if(target->scc_id != scc_id)
        continue;
      size_t target_idx = target->scc_pos;
      pony_assert(target_idx < scc_size);
      pony_assert(scc_members[target_idx] == target);

      if(target_idx == 0)
      {
        // Closing edge back to head — cycle found.
        found = true;
        break;
      }

      if(on_path[target_idx])
        continue;  // visited on this path, would form a sub-cycle

      if(done[target_idx])
        continue;  // already proven not to reach head

      // Push.
      path_out[depth] = target_idx;
      edge_idx[depth] = 0;
      on_path[target_idx] = true;
      depth++;
      advanced = true;
      break;
    }

    if(!advanced && !found)
    {
      // Pop. Mark done so other paths skip this subtree.
      on_path[cur] = false;
      done[cur] = true;
      depth--;
    }
  }

  ponyint_pool_free_size(scc_size * sizeof(bool), on_path);
  ponyint_pool_free_size(scc_size * sizeof(bool), done);
  ponyint_pool_free_size(scc_size * sizeof(size_t), edge_idx);

  if(found)
  {
    *path_len_out = depth;
  }
  else
  {
    // Fallback: SCC member order. Reaches here only for a "cycle"
    // SCC where DFS from member 0 can't find a back-edge — shouldn't
    // happen for a genuine SCC, but the fallback keeps us from
    // emitting an empty path.
    for(size_t i = 0; i < scc_size; i++)
      path_out[i] = i;
    *path_len_out = scc_size;
  }
}


static printbuf_t* render_alias_head(ast_t* def);


// Render a cycle path as a string for inclusion in an error message.
// Caller must free with ponyint_pool_free_size.
// Render N0 -> N1 -> ... -> Nk -> N0, where the path is computed by
// find_cycle_path: a real edge-following DFS from scc_members[0],
// terminating when an edge returns to it. Consecutive aliases in the
// rendered path actually reference each other directly. Each step is
// rendered with the alias's typeparams (`Bad[T] -> Bad[T]`) so the
// cycle headline matches the suggestion frame, which uses the same
// format. '*size_out' receives the alloc size so the caller can free
// with the matching pool size class.
static char* render_cycle_path(alias_node_t** scc_members, size_t scc_size,
  size_t* size_out)
{
  pony_assert(scc_size >= 1);

  size_t* path = (size_t*)ponyint_pool_alloc_size(scc_size * sizeof(size_t));
  size_t path_len = 0;
  find_cycle_path(scc_members, scc_size, path, &path_len);
  pony_assert(path_len >= 1);

  printbuf_t* buf = printbuf_new();
  for(size_t i = 0; i < path_len; i++)
  {
    if(i > 0)
      printbuf(buf, " -> ");
    printbuf_t* step = render_alias_head(scc_members[path[i]]->def);
    printbuf(buf, "%s", step->m);
    printbuf_free(step);
  }
  printbuf(buf, " -> ");
  printbuf_t* head_step = render_alias_head(scc_members[path[0]]->def);
  printbuf(buf, "%s", head_step->m);
  printbuf_free(head_step);

  size_t total = strlen(buf->m) + 1;
  char* out = (char*)ponyint_pool_alloc_size(total);
  memcpy(out, buf->m, total);
  printbuf_free(buf);

  ponyint_pool_free_size(scc_size * sizeof(size_t), path);

  *size_out = total;
  return out;
}


// Tarjan's SCC algorithm, iterative.
//
// Uses an explicit work stack so the C call stack stays bounded
// regardless of input size. Each work-stack frame holds the node
// being processed and the index of the next outgoing edge to
// consider; recursion descents become pushes, returns become pops
// with a low-link update on the parent. A 100k-deep alias chain
// (`type A1 is A2; ... type AN is U64`) made the recursive form
// blow the C stack at ~130k links on Linux with default 8 MB; the
// iterative form is bounded by g->count for both the work stack
// and the SCC stack.
//
// 'g' is the graph; 'start' is the unvisited root to begin from.
// 'index' is the running index counter (caller-shared across
// roots). 'scc_stack' is the SCC member stack (Tarjan's
// "S"); 'top' is its current top. 'scc_count' is the running
// SCC id (caller-shared). All arrays are pre-allocated to g->count
// by the caller.
static void tarjan_strongconnect_iterative(alias_node_t* start,
  ssize_t* index, alias_node_t** scc_stack, ssize_t* scc_top,
  ssize_t* scc_count, alias_node_t** work_node,
  size_t* work_edge_idx)
{
  size_t work_top = 0;

  // Initial push of 'start'.
  start->tarjan_index = *index;
  start->tarjan_lowlink = *index;
  (*index)++;
  scc_stack[(*scc_top)++] = start;
  start->on_stack = true;

  work_node[work_top] = start;
  work_edge_idx[work_top] = 0;
  work_top++;

  while(work_top > 0)
  {
    alias_node_t* v = work_node[work_top - 1];

    bool descended = false;
    while(work_edge_idx[work_top - 1] < v->all_edge_count)
    {
      alias_node_t* w = v->all_edges[work_edge_idx[work_top - 1]++];

      if(w->tarjan_index == -1)
      {
        // Descend: visit w. The lowlink update for v happens on
        // pop (post-order).
        w->tarjan_index = *index;
        w->tarjan_lowlink = *index;
        (*index)++;
        scc_stack[(*scc_top)++] = w;
        w->on_stack = true;

        work_node[work_top] = w;
        work_edge_idx[work_top] = 0;
        work_top++;
        descended = true;
        break;
      }
      else if(w->on_stack)
      {
        if(w->tarjan_index < v->tarjan_lowlink)
          v->tarjan_lowlink = w->tarjan_index;
      }
    }

    if(descended)
      continue;

    // All edges of v processed. Form an SCC if v is a root, then
    // pop and propagate lowlink to the parent (if any).
    if(v->tarjan_lowlink == v->tarjan_index)
    {
      alias_node_t* w;
      do
      {
        w = scc_stack[--(*scc_top)];
        w->on_stack = false;
        w->scc_id = *scc_count;
      } while(w != v);
      (*scc_count)++;
    }

    work_top--;
    if(work_top > 0)
    {
      alias_node_t* parent = work_node[work_top - 1];
      if(v->tarjan_lowlink < parent->tarjan_lowlink)
        parent->tarjan_lowlink = v->tarjan_lowlink;
    }
  }
}


// True if 'subtree' transitively references any alias in the SCC
// identified by 'scc_id'. Used to decide whether a union member counts
// as a base case.
//
// Walks the syntactic subtree only — does not cross into the bodies
// of cross-SCC alias references (unlike subtree_has_base_case_union,
// which does). This asymmetry is sound: if a non-SCC alias's body
// contained any reference to an SCC member, that body would have
// added an edge to the SCC at edge-collection time, which would
// have pulled the alias into the SCC during Tarjan. So a
// genuinely-non-SCC alias's body has no SCC references to find.
// We catch SCC references that flow through a wrap alias's typeargs
// (e.g., the 'X' in 'Wrap[X]') because they appear directly in the
// syntactic subtree we already walk.
static bool subtree_references_scc(alias_graph_t* g, ast_t* subtree,
  ssize_t scc_id)
{
  if(subtree == NULL)
    return false;

  if(ast_id(subtree) == TK_TYPEALIASREF)
  {
    ast_t* def = (ast_t*)ast_data(subtree);
    alias_node_t* target = node_for_def(g, def);
    if((target != NULL) && (target->scc_id == scc_id))
      return true;
    // Fall through to also check inside this ref's typeargs.
  }

  for(ast_t* child = ast_child(subtree); child != NULL;
    child = ast_sibling(child))
  {
    if(subtree_references_scc(g, child, scc_id))
      return true;
  }
  return false;
}


// Classify how 'tp_def' (a TK_TYPEPARAM) appears in 'body':
//
//   TP_UNUSED                — tp_def doesn't appear at all
//   TP_USED_CONSTRUCTIVE_ONLY — every appearance is inside a nominal
//                               class's typeargs (or transitively
//                               reaches one through other aliases'
//                               constructive-only typeparams)
//   TP_USED_NON_CONSTRUCTIVE  — at least one appearance is at a bare
//                               position (top-level, union/isect/
//                               tuple member, or alias whose typeparam
//                               is itself non-constructive)
//
// The walk crosses through other TK_TYPEALIASREF bodies, recursively
// classifying their typeparam usage. The 'visited' set bounds the
// recursion at the alias-graph level. Pure function of (body, tp_def);
// the visited set is just for termination.
//
// Used by collect_edges to decide whether passing X as an alias's
// typearg position should classify the X edge as constructive or not,
// or skip it entirely (when the typeparam is unused).
static typeparam_use_t typeparam_use_in_body(ast_t* body, ast_t* tp_def,
  visited_defs_t* visited, bool currently_constructive)
{
  if(body == NULL)
    return TP_UNUSED;

  if(ast_id(body) == TK_TYPEPARAMREF)
  {
    if(ast_data(body) != tp_def)
      return TP_UNUSED;
    return currently_constructive
      ? TP_USED_CONSTRUCTIVE_ONLY : TP_USED_NON_CONSTRUCTIVE;
  }

  if(ast_id(body) == TK_NOMINAL)
  {
    ast_t* typeargs = ast_childidx(body, 2);
    return typeparam_use_in_body(typeargs, tp_def, visited, true);
  }

  if(ast_id(body) == TK_TYPEALIASREF)
  {
    ast_t* def = (ast_t*)ast_data(body);
    if(def == NULL || ast_id(def) != TK_TYPE)
      return TP_UNUSED;
    if(visited_contains(visited, def))
      return TP_UNUSED;

    // 'visited' is scratch within this branch only — it guards against
    // re-entering 'def' (and any alias entered transitively) during the
    // typeparam-use classification of 'def's own body and the typeargs
    // we pass to it. Push 'def' before recursing and pop on every exit
    // so caller siblings start each walk from the same baseline. Both
    // inner calls save/restore the count for the same reason: the
    // aliases either one transitively enters are part of the classifier's
    // scratch, not a permanent fact other walks should inherit.
    visited_push(visited, def);

    ast_t* def_body = alias_body(def);
    ast_t* def_typeparams = ast_childidx(def, 1);
    ast_t* def_tp = ast_child(def_typeparams);
    ast_t* ta = ast_child(ast_childidx(body, 1));

    typeparam_use_t result = TP_UNUSED;

    while(def_tp != NULL && ta != NULL)
    {
      // How does the referenced alias use def_tp in its own body?
      size_t saved = visited->count;
      typeparam_use_t def_tp_use = (def_body != NULL)
        ? typeparam_use_in_body(def_body, def_tp, visited, false)
        : TP_UNUSED;
      visited->count = saved;

      // If the referenced alias discards this typeparam, the typearg
      // doesn't propagate at all.
      if(def_tp_use == TP_UNUSED)
      {
        def_tp = ast_sibling(def_tp);
        ta = ast_sibling(ta);
        continue;
      }

      bool ta_constructive = currently_constructive
        || (def_tp_use == TP_USED_CONSTRUCTIVE_ONLY);
      typeparam_use_t ta_use = typeparam_use_in_body(ta, tp_def,
        visited, ta_constructive);
      visited->count = saved;

      // If the alias uses def_tp non-constructively somewhere AND
      // tp_def appears in this typearg, the propagation through this
      // slot is non-constructive even if some other use is
      // constructive.
      if(def_tp_use == TP_USED_NON_CONSTRUCTIVE && ta_use != TP_UNUSED)
        ta_use = TP_USED_NON_CONSTRUCTIVE;

      if(ta_use == TP_USED_NON_CONSTRUCTIVE)
      {
        result = TP_USED_NON_CONSTRUCTIVE;
        break;
      }
      if(ta_use == TP_USED_CONSTRUCTIVE_ONLY)
        result = TP_USED_CONSTRUCTIVE_ONLY;

      def_tp = ast_sibling(def_tp);
      ta = ast_sibling(ta);
    }

    // Pop 'def' so caller siblings don't inherit it.
    visited->count--;
    return result;
  }

  if(ast_id(body) == TK_ARROW)
  {
    return typeparam_use_in_body(ast_childidx(body, 1), tp_def, visited,
      currently_constructive);
  }

  // TK_UNIONTYPE / TK_ISECTTYPE / TK_TUPLETYPE / generic walk:
  // children inherit the current constructive flag.
  typeparam_use_t result = TP_UNUSED;
  for(ast_t* child = ast_child(body); child != NULL;
    child = ast_sibling(child))
  {
    typeparam_use_t child_use = typeparam_use_in_body(child, tp_def,
      visited, currently_constructive);
    if(child_use == TP_USED_NON_CONSTRUCTIVE)
      return TP_USED_NON_CONSTRUCTIVE;
    if(child_use == TP_USED_CONSTRUCTIVE_ONLY)
      result = TP_USED_CONSTRUCTIVE_ONLY;
  }
  return result;
}


// True if 'subtree' contains a TK_UNIONTYPE (at any depth, including
// inside the bodies of cross-SCC alias references) where at least one
// member transitively references no alias in 'scc_id'. Such a union is
// a base-case site for the SCC: the value can be constructed by
// choosing the non-recursive branch.
//
// Cross-SCC alias references are followed into their bodies — the base
// case for an alias may live inside a non-recursive wrapper alias that
// the recursive alias references. In-SCC alias references are not
// followed (they're part of the cycle we're trying to bottom out of).
// The visited set guards against revisiting the same wrapper alias
// twice during a single scan.
static bool subtree_has_base_case_union(alias_graph_t* g, ast_t* subtree,
  ssize_t scc_id, visited_defs_t* visited)
{
  if(subtree == NULL)
    return false;

  if(ast_id(subtree) == TK_UNIONTYPE)
  {
    for(ast_t* member = ast_child(subtree); member != NULL;
      member = ast_sibling(member))
    {
      if(!subtree_references_scc(g, member, scc_id))
        return true;
    }
  }

  if(ast_id(subtree) == TK_TYPEALIASREF)
  {
    ast_t* def = (ast_t*)ast_data(subtree);
    alias_node_t* target = node_for_def(g, def);
    if((target != NULL) && (target->scc_id != scc_id) &&
      !visited_contains(visited, def))
    {
      visited_push(visited, def);
      ast_t* body = alias_body(def);
      if((body != NULL) &&
        subtree_has_base_case_union(g, body, scc_id, visited))
        return true;
    }
    // Fall through to also walk this ref's typeargs.
  }

  for(ast_t* child = ast_child(subtree); child != NULL;
    child = ast_sibling(child))
  {
    if(subtree_has_base_case_union(g, child, scc_id, visited))
      return true;
  }
  return false;
}


// Get the 'provides' body (the alias's RHS) from a TK_TYPE def.
static ast_t* alias_body(ast_t* def)
{
  // TK_TYPE children: id, typeparams, cap, provides, members, at, doc
  ast_t* provides = ast_childidx(def, 3);
  if(ast_id(provides) != TK_PROVIDES)
    return NULL;
  return ast_child(provides);
}


typedef enum
{
  CYCLE_NO_CONSTRUCTIVE_EDGE,
  CYCLE_NO_BASE_CASE
} cycle_failure_t;


// Render a TK_TYPE def's head as it would appear at a use site:
// `Name` when the alias takes no typeparams, `Name[T1, T2, ...]` when
// it does. Used in single-alias cycle hints so a suggestion like
// `Array[Wrap[T]]` or `type Wrap[T] is (None | <body>)` is paste-able
// — the bare alias name doesn't compile in either spot for parametric
// aliases. Caller frees with printbuf_free.
//
// Typeparam constraints (e.g., `T: Constraint`) are intentionally not
// rendered: the hint is "thread through a class's typearg" or "add a
// base case" — bare `[T, U]` is the right level of detail, and the
// user picks the concrete shape.
static printbuf_t* render_alias_head(ast_t* def)
{
  AST_GET_CHILDREN(def, head_id, typeparams);
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s", ast_name(head_id));

  if(ast_id(typeparams) == TK_TYPEPARAMS)
  {
    printbuf(buf, "[");
    bool first = true;
    for(ast_t* tp = ast_child(typeparams); tp != NULL;
      tp = ast_sibling(tp))
    {
      ast_t* tp_id = ast_child(tp);
      if(!first)
        printbuf(buf, ", ");
      printbuf(buf, "%s", ast_name(tp_id));
      first = false;
    }
    printbuf(buf, "]");
  }

  return buf;
}


static void report_illegal_cycle(pass_opt_t* opt, alias_node_t** scc_members,
  size_t scc_size, cycle_failure_t failure)
{
  pony_assert(scc_size >= 1);
  AST_GET_CHILDREN(scc_members[0]->def, head_id);
  const char* head_name = ast_name(head_id);
  size_t path_size = 0;
  char* path = render_cycle_path(scc_members, scc_size, &path_size);

  errorframe_t frame = NULL;

  ast_error_frame(&frame, scc_members[0]->def,
    "type alias '%s' can't be infinitely recursive (cycle: %s)",
    head_name, path);
  // Cap the per-alias fan-out at a handful of frames. The cycle path
  // in the headline names every alias in order; the per-alias frames
  // just give source pointers, which stop being useful past the first
  // few.
  const size_t MAX_CYCLE_FAN_OUT_FRAMES = 3;
  size_t shown = scc_size - 1;
  if(shown > MAX_CYCLE_FAN_OUT_FRAMES)
    shown = MAX_CYCLE_FAN_OUT_FRAMES;
  for(size_t i = 1; i <= shown; i++)
  {
    AST_GET_CHILDREN(scc_members[i]->def, id);
    ast_error_frame(&frame, scc_members[i]->def,
      "alias '%s' is part of the same cycle", ast_name(id));
  }
  size_t hidden = scc_size - 1 - shown;
  if(hidden == 1)
  {
    ast_error_frame(&frame, scc_members[0]->def,
      "(and 1 more alias is part of this cycle)");
  }
  else if(hidden > 1)
  {
    ast_error_frame(&frame, scc_members[0]->def,
      "(and %zu more aliases are part of this cycle)", hidden);
  }

  // Hint the user toward a finite shape. Single-alias cycles get a
  // concrete suggestion; multi-alias cycles get the rule only because
  // the right place to add a class wrapper or base case isn't local
  // to one alias. The single-alias rendering includes the alias's
  // typeparams (`Wrap[T]` rather than bare `Wrap`) so the suggestion
  // is paste-able for parametric aliases.
  printbuf_t* head_buf = NULL;
  if(scc_size == 1)
    head_buf = render_alias_head(scc_members[0]->def);

  if(failure == CYCLE_NO_CONSTRUCTIVE_EDGE)
  {
    if(scc_size == 1)
      ast_error_frame(&frame, scc_members[0]->def,
        "the recursion must thread through a class's type argument, "
        "e.g., 'Array[%s]'", head_buf->m);
    else
      ast_error_frame(&frame, scc_members[0]->def,
        "the recursion must thread through a class's type argument "
        "(e.g., 'Array[<typearg>]'); recursion through union members "
        "or tuple elements has no finite layout");
  }
  else
  {
    if(scc_size == 1)
      ast_error_frame(&frame, scc_members[0]->def,
        "add a non-recursive alternative in a union, e.g., "
        "'type %s is (None | <body>)'", head_buf->m);
    else
      ast_error_frame(&frame, scc_members[0]->def,
        "add a non-recursive alternative in a union somewhere in the "
        "cycle (e.g., '(None | ...)') so the type has a base case");
  }

  errorframe_report(&frame, opt->check.errors);

  ponyint_pool_free_size(path_size, path);
  if(head_buf != NULL)
    printbuf_free(head_buf);
}


static bool check_base_cases(pass_opt_t* opt, alias_graph_t* g)
{
  // Group nodes by SCC. For each SCC of size >= 2, or a self-loop SCC
  // (size 1 with a self-edge), check that some alias in the SCC has a
  // TK_UNIONTYPE that provides a base case.
  bool ok = true;

  // Find max scc_id to size the grouping arrays.
  ssize_t max_scc = -1;
  for(size_t i = 0; i < g->count; i++)
  {
    if(g->nodes[i].scc_id > max_scc)
      max_scc = g->nodes[i].scc_id;
  }

  if(max_scc < 0)
    return true;

  size_t group_count = (size_t)(max_scc + 1);
  size_t* sizes = (size_t*)ponyint_pool_alloc_size(
    group_count * sizeof(size_t));
  memset(sizes, 0, group_count * sizeof(size_t));

  for(size_t i = 0; i < g->count; i++)
    sizes[g->nodes[i].scc_id]++;

  alias_node_t*** groups = (alias_node_t***)ponyint_pool_alloc_size(
    group_count * sizeof(alias_node_t**));
  for(size_t i = 0; i < group_count; i++)
  {
    groups[i] = (alias_node_t**)ponyint_pool_alloc_size(
      sizes[i] * sizeof(alias_node_t*));
  }

  size_t* fill = (size_t*)ponyint_pool_alloc_size(
    group_count * sizeof(size_t));
  memset(fill, 0, group_count * sizeof(size_t));

  for(size_t i = 0; i < g->count; i++)
  {
    ssize_t s = g->nodes[i].scc_id;
    g->nodes[i].scc_pos = fill[s];
    groups[s][fill[s]++] = &g->nodes[i];
  }

  for(size_t s = 0; s < group_count; s++)
  {
    bool is_cycle_scc = (sizes[s] >= 2);
    if(!is_cycle_scc && sizes[s] == 1)
    {
      // A singleton SCC is a cycle only if the node has a self-edge.
      alias_node_t* n = groups[s][0];
      for(size_t e = 0; e < n->all_edge_count; e++)
      {
        if(n->all_edges[e] == n)
        {
          is_cycle_scc = true;
          break;
        }
      }
    }

    if(!is_cycle_scc)
      continue;

    // Productivity check: in the subgraph of intra-SCC edges
    // restricted to NON-CONSTRUCTIVE references, there must be no
    // cycle. A cycle in that subgraph would mean a recursive branch
    // can be taken without ever crossing a heap-pointer boundary
    // (typealias_unfold loops, codegen has no finite layout).
    //
    // The "at least one constructive edge" version of this check
    // accepted 'type X is (X | None | Array[X])' because the
    // Array[X] arm gave a constructive edge — but the bare X arm
    // forms a non-constructive self-loop and downstream unfolding
    // loops on it. The "every intra-SCC edge is constructive"
    // version was too strict: it rejected the canonical wrap shape
    // 'type JsonValue is (String | ... | JsonArray); type JsonArray
    // is Array[JsonValue]' because JsonValue → JsonArray is non-
    // constructive — even though JsonArray's body is Array[...],
    // which heap-breaks back to JsonValue. The cycle JsonValue →
    // JsonArray → JsonValue traverses one constructive edge, so the
    // non-constructive-only subgraph (which only has JsonValue →
    // JsonArray) has no cycle. That program unfolds finitely.
    //
    // So: reject when the non-constructive intra-SCC subgraph has a
    // cycle. Equivalently, every cycle through this SCC must include
    // at least one constructive edge.
    //
    // Detection: 3-color DFS from each SCC node through non-
    // constructive intra-SCC edges. on_path is the gray set (current
    // DFS path); done is the black set (fully explored, no cycle
    // reachable). Hitting an on_path node = back-edge = cycle.
    // Skipping done nodes makes the pass O(V+E) per SCC: a forward
    // DAG of non-cons edges (dense bag of related sum-type aliases)
    // is acyclic, but without the black set every start re-explores
    // every path from itself, giving 2^V time.
    bool non_cons_cycle = false;
    {
      bool* on_path = (bool*)ponyint_pool_alloc_size(
        sizes[s] * sizeof(bool));
      bool* done = (bool*)ponyint_pool_alloc_size(
        sizes[s] * sizeof(bool));
      memset(on_path, 0, sizes[s] * sizeof(bool));
      memset(done, 0, sizes[s] * sizeof(bool));

      // Iterative DFS: stack of (node_idx, next_edge_to_consider).
      // Use sizes[s] as max depth (SCC size bounds path length).
      size_t* stack_node = (size_t*)ponyint_pool_alloc_size(
        sizes[s] * sizeof(size_t));
      size_t* stack_edge = (size_t*)ponyint_pool_alloc_size(
        sizes[s] * sizeof(size_t));

      for(size_t start = 0; start < sizes[s] && !non_cons_cycle; start++)
      {
        if(done[start])
          continue;

        size_t depth = 0;
        stack_node[depth] = start;
        stack_edge[depth] = 0;
        on_path[start] = true;
        depth++;

        while(depth > 0)
        {
          size_t cur = stack_node[depth - 1];
          alias_node_t* n = groups[s][cur];

          bool advanced = false;
          while(stack_edge[depth - 1] < n->non_constructive_edge_count)
          {
            alias_node_t* target = n->non_constructive_edges[
              stack_edge[depth - 1]++];
            if(target->scc_id != (ssize_t)s)
              continue;

            // O(1) intra-SCC lookup via scc_pos, populated above.
            size_t target_idx = target->scc_pos;
            pony_assert(target_idx < sizes[s]);
            pony_assert(groups[s][target_idx] == target);

            if(on_path[target_idx])
            {
              non_cons_cycle = true;
              break;
            }

            if(done[target_idx])
              continue;

            // Push.
            stack_node[depth] = target_idx;
            stack_edge[depth] = 0;
            on_path[target_idx] = true;
            depth++;
            advanced = true;
            break;
          }

          if(non_cons_cycle)
            break;

          if(!advanced)
          {
            // Pop. Mark done so future starts and other paths skip
            // this subtree.
            on_path[cur] = false;
            done[cur] = true;
            depth--;
          }
        }
      }

      ponyint_pool_free_size(sizes[s] * sizeof(size_t), stack_node);
      ponyint_pool_free_size(sizes[s] * sizeof(size_t), stack_edge);
      ponyint_pool_free_size(sizes[s] * sizeof(bool), on_path);
      ponyint_pool_free_size(sizes[s] * sizeof(bool), done);
    }

    // Also require at least one constructive intra-SCC edge — the
    // cycle has some productive arm. Without one, 'type A is A'
    // would pass: it has no non-constructive arm only because it has
    // no arms at all, but the cycle is degenerate.
    bool has_intra_constructive = false;
    for(size_t i = 0; i < sizes[s] && !has_intra_constructive; i++)
    {
      alias_node_t* n = groups[s][i];
      for(size_t e = 0; e < n->constructive_edge_count; e++)
      {
        if(n->constructive_edges[e]->scc_id == (ssize_t)s)
        {
          has_intra_constructive = true;
          break;
        }
      }
    }

    bool has_constructive_edge = !non_cons_cycle && has_intra_constructive;

    // Base-case check: at least one alias in the SCC must have a
    // TK_UNIONTYPE where some member transitively references no alias
    // in the SCC. Cross-SCC alias references are followed into their
    // bodies; visited bounds the walk and is shared across the SCC's
    // members. Sharing is sound because base-case detection is a pure
    // function of (body, scc_id) — visiting the same wrapper alias
    // body via a different SCC member couldn't reveal a base case the
    // first visit missed.
    bool has_base_case = false;
    visited_defs_t visited = { NULL, 0, 0 };
    for(size_t i = 0; i < sizes[s]; i++)
    {
      ast_t* body = alias_body(groups[s][i]->def);
      if(subtree_has_base_case_union(g, body, (ssize_t)s, &visited))
      {
        has_base_case = true;
        break;
      }
    }
    visited_free(&visited);

    if(!has_constructive_edge)
    {
      report_illegal_cycle(opt, groups[s], sizes[s],
        CYCLE_NO_CONSTRUCTIVE_EDGE);
      ok = false;
    }
    else if(!has_base_case)
    {
      report_illegal_cycle(opt, groups[s], sizes[s],
        CYCLE_NO_BASE_CASE);
      ok = false;
    }
  }

  for(size_t i = 0; i < group_count; i++)
    ponyint_pool_free_size(sizes[i] * sizeof(alias_node_t*), groups[i]);
  ponyint_pool_free_size(group_count * sizeof(alias_node_t**), groups);
  ponyint_pool_free_size(group_count * sizeof(size_t), sizes);
  ponyint_pool_free_size(group_count * sizeof(size_t), fill);

  return ok;
}


static void free_graph(alias_graph_t* g)
{
  // Destroy the index first; it stores raw pointers into g->nodes,
  // so it must not outlive them. NULL free_elem keeps the destructor
  // from touching the entries themselves.
  alias_index_destroy(&g->index);

  for(size_t i = 0; i < g->count; i++)
  {
    alias_node_t* n = &g->nodes[i];
    if(n->constructive_edges != NULL)
    {
      ponyint_pool_free_size(
        n->constructive_edge_cap * sizeof(alias_node_t*),
        n->constructive_edges);
    }
    if(n->non_constructive_edges != NULL)
    {
      ponyint_pool_free_size(
        n->non_constructive_edge_cap * sizeof(alias_node_t*),
        n->non_constructive_edges);
    }
    if(n->all_edges != NULL)
    {
      ponyint_pool_free_size(
        n->all_edge_cap * sizeof(alias_node_t*), n->all_edges);
    }
  }
  ponyint_pool_free_size(g->count * sizeof(alias_node_t), g->nodes);
}


ast_result_t pass_typealias_recursion(ast_t** astp, pass_opt_t* opt)
{
  ast_t* ast = *astp;
  // Pre-visitor invocation: this fires for every node in the AST
  // (TK_PACKAGE, TK_MODULE, TK_TYPE, TK_CLASS, TK_ACTOR, ...) during
  // both the initial program-level walk and any sub-AST catch-up
  // walks invoked by sugar/lambda's synthesizers. Run the analysis
  // once when we see TK_PROGRAM; skip for every other node.
  //
  // A sub-AST catch-up call CAN reach here with a non-TK_PROGRAM
  // root that contains TK_TYPE definitions — but only if the
  // synthesizer produced new typealiases. Today no synthesizer does
  // (sugar/lambda only produce TK_CLASS / TK_PRIMITIVE), so the
  // skip is correct. An assertion that catches the regression is
  // not feasible from this pre-visitor — the same callback receives
  // both the initial program walk's TK_PACKAGE/TK_MODULE nodes
  // (which legitimately contain TK_TYPE children) and any future
  // catch-up call (which shouldn't). If a synthesizer ever starts
  // producing typealiases, that synthesizer needs to either run
  // this pass at program scope on the synthesized output or call
  // the legality check directly on the new TK_TYPE nodes.
  if(ast_id(ast) != TK_PROGRAM)
    return AST_OK;

  // Step 1: enumerate all alias definitions.
  ast_t** alias_defs = NULL;
  size_t alias_count = 0;
  size_t alias_cap = 0;
  collect_aliases(ast, &alias_defs, &alias_count, &alias_cap);

  if(alias_count == 0)
  {
    if(alias_defs != NULL)
      ponyint_pool_free_size(alias_cap * sizeof(ast_t*), alias_defs);
    return AST_OK;
  }

  alias_graph_t g;
  g.count = alias_count;
  g.nodes = (alias_node_t*)ponyint_pool_alloc_size(
    alias_count * sizeof(alias_node_t));

  // Pre-size the index for the known node count to avoid rehash
  // churn during insertion. ponyint_hashmap_init takes a slot
  // hint, not an exact size.
  alias_index_init(&g.index, alias_count);

  for(size_t i = 0; i < alias_count; i++)
  {
    g.nodes[i].def = alias_defs[i];
    g.nodes[i].all_edges = NULL;
    g.nodes[i].all_edge_count = 0;
    g.nodes[i].all_edge_cap = 0;
    g.nodes[i].constructive_edges = NULL;
    g.nodes[i].constructive_edge_count = 0;
    g.nodes[i].constructive_edge_cap = 0;
    g.nodes[i].non_constructive_edges = NULL;
    g.nodes[i].non_constructive_edge_count = 0;
    g.nodes[i].non_constructive_edge_cap = 0;
    g.nodes[i].tarjan_index = -1;
    g.nodes[i].tarjan_lowlink = -1;
    g.nodes[i].on_stack = false;
    g.nodes[i].scc_id = -1;
    g.nodes[i].scc_pos = SIZE_MAX;
    g.nodes[i].add_edge_gen_all = 0;
    g.nodes[i].add_edge_gen_cons = 0;
    g.nodes[i].add_edge_gen_ncons = 0;
    alias_index_put(&g.index, &g.nodes[i]);
  }
  g.add_edge_gen = 0;
  ponyint_pool_free_size(alias_cap * sizeof(ast_t*), alias_defs);

  // Step 2: compute reference-graph edges, classifying each by whether
  // it's reached through a constructor (productive recursion) or not.
  for(size_t i = 0; i < g.count; i++)
  {
    ast_t* body = alias_body(g.nodes[i].def);
    if(body == NULL)
      continue;

    // Bump the dedup gen before each source's collection. add_edge
    // skips a target whose matching gen field already equals this
    // value, eliminating the prior O(deg) linear scan per insertion.
    g.add_edge_gen++;

    // The body itself is the top of the alias's value structure;
    // anything reached without crossing into a nominal typearg is
    // non-constructive.
    collect_edges(&g, body, false,
      &g.nodes[i].all_edges, &g.nodes[i].all_edge_count,
      &g.nodes[i].all_edge_cap,
      &g.nodes[i].constructive_edges, &g.nodes[i].constructive_edge_count,
      &g.nodes[i].constructive_edge_cap,
      &g.nodes[i].non_constructive_edges,
      &g.nodes[i].non_constructive_edge_count,
      &g.nodes[i].non_constructive_edge_cap);
  }

  // Step 3: Tarjan's SCC over the reference graph.
  // Iterative implementation; both the SCC stack and the explicit
  // work stack are bounded by g.count, so a deep alias chain
  // (`type A1 is A2; ...`) won't blow the C stack.
  ssize_t index = 0;
  ssize_t scc_top = 0;
  ssize_t scc_count = 0;
  alias_node_t** scc_stack = (alias_node_t**)ponyint_pool_alloc_size(
    g.count * sizeof(alias_node_t*));
  alias_node_t** work_node = (alias_node_t**)ponyint_pool_alloc_size(
    g.count * sizeof(alias_node_t*));
  size_t* work_edge_idx = (size_t*)ponyint_pool_alloc_size(
    g.count * sizeof(size_t));

  for(size_t i = 0; i < g.count; i++)
  {
    if(g.nodes[i].tarjan_index == -1)
      tarjan_strongconnect_iterative(&g.nodes[i], &index,
        scc_stack, &scc_top, &scc_count, work_node, work_edge_idx);
  }

  ponyint_pool_free_size(g.count * sizeof(alias_node_t*), scc_stack);
  ponyint_pool_free_size(g.count * sizeof(alias_node_t*), work_node);
  ponyint_pool_free_size(g.count * sizeof(size_t), work_edge_idx);

  // Step 4: per-SCC base-case check.
  bool base_ok = check_base_cases(opt, &g);

  free_graph(&g);

  return base_ok ? AST_OK : AST_FATAL;
}
