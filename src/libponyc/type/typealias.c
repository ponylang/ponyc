#include "typealias.h"
#include "cap.h"
#include "reify.h"
#include "../ast/source.h"
#include "../ast/token.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdio.h>
#include <string.h>

static void apply_cap_to_type(ast_t* type, token_id tcap, token_id teph)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      for(ast_t* child = ast_child(type);
        child != NULL;
        child = ast_sibling(child))
      {
        apply_cap_to_type(child, tcap, teph);
      }

      return;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    case TK_TYPEALIASREF:
    {
      ast_t* r_cap = cap_fetch(type);

      if(tcap != TK_NONE)
        ast_setid(r_cap, tcap);

      if(teph != TK_NONE)
      {
        ast_t* r_eph = ast_sibling(r_cap);
        ast_setid(r_eph, teph);
      }

      return;
    }

    case TK_ARROW:
      apply_cap_to_type(ast_childidx(type, 1), tcap, teph);
      return;

    default: {}
  }
}

static ast_t* unfold_one(ast_t* typealiasref)
{
  ast_t* def = (ast_t*)ast_data(typealiasref);
  pony_assert(def != NULL);
  pony_assert(ast_id(def) == TK_TYPE);

  AST_GET_CHILDREN(typealiasref, id, typeargs, cap, eph);
  AST_GET_CHILDREN(def, alias_id, typeparams, def_cap, provides);

  ast_t* alias = ast_child(provides);
  pony_assert(alias != NULL);

  ast_t* r_alias = reify(alias, typeparams, typeargs, NULL, true);

  if(r_alias == NULL)
    return NULL;

  token_id tcap = ast_id(cap);
  token_id teph = ast_id(eph);

  if((tcap != TK_NONE) || (teph != TK_NONE))
    apply_cap_to_type(r_alias, tcap, teph);

  return r_alias;
}

// Defensive cycle detection for chained-alias unfold. Termination is
// guaranteed by PASS_TYPEALIAS_RECURSION (which rejects bare alias-only
// cycles), but if the legality pass has a bug that lets such a cycle
// through, the unfold loop would spin forever. The visited set tracks
// the alias defs already encountered on this unfold; revisiting a def
// means we'd loop, so we abort with the offending source position.
//
// Bounded by chain depth, not capped at any constant: a 100,000-link
// non-recursive chain unfolds without issue, only true cycles trip
// the check. Realistic chains are 1-3 hops deep, so the initial
// buffer is small and grows on demand.
#define VISITED_INITIAL_CAP 8

ast_t* typealias_unfold(ast_t* typealiasref)
{
  pony_assert(ast_id(typealiasref) == TK_TYPEALIASREF);

  size_t v_cap = VISITED_INITIAL_CAP;
  size_t v_count = 0;
  ast_t** visited = (ast_t**)ponyint_pool_alloc_size(v_cap * sizeof(ast_t*));

  // Seed the visited set with the input's def so a self-cycle is
  // caught on the first re-encounter.
  visited[v_count++] = (ast_t*)ast_data(typealiasref);

  ast_t* r_alias = unfold_one(typealiasref);

  // If the head is itself a TK_TYPEALIASREF (chained alias like
  // `type A is B; type B is (U64, U64)`), unfold transitively until
  // the head is a non-alias. Termination: PASS_TYPEALIAS_RECURSION
  // rejects bare alias-only cycles, and any constructive recursion
  // threads through a TK_NOMINAL typearg position whose head isn't
  // itself an alias — so the loop reaches a non-alias head in
  // finitely many steps for any input that passed legality.
  while(r_alias != NULL && ast_id(r_alias) == TK_TYPEALIASREF)
  {
    ast_t* def = (ast_t*)ast_data(r_alias);

    // Have we seen this def before on this unfold? If so, an alias-
    // only cycle escaped PASS_TYPEALIAS_RECURSION — a compiler bug.
    bool cycle = false;
    for(size_t i = 0; i < v_count; i++)
    {
      if(visited[i] == def)
      {
        cycle = true;
        break;
      }
    }

    if(cycle)
    {
      // Report the offending source position before aborting so the
      // diagnostic isn't routed through LLVM's signal handler with
      // no language-level context.
      source_t* src = ast_source(typealiasref);
      const char* file = (src != NULL && src->file != NULL) ? src->file
                                                            : "<unknown>";
      fprintf(stderr,
        "%s:%zu:%zu: compiler internal error: PASS_TYPEALIAS_RECURSION let "
        "an alias-only cycle through; typealias_unfold cannot terminate\n",
        file, ast_line(typealiasref), ast_pos(typealiasref));
      pony_assert(false);
      // Unreachable: PONY_ALWAYS_ASSERT is defined for libponyc, so
      // pony_assert(false) always aborts.
    }

    if(v_count >= v_cap)
    {
      size_t new_cap = v_cap * 2;
      ast_t** new_visited = (ast_t**)ponyint_pool_alloc_size(
        new_cap * sizeof(ast_t*));
      memcpy(new_visited, visited, v_count * sizeof(ast_t*));
      ponyint_pool_free_size(v_cap * sizeof(ast_t*), visited);
      visited = new_visited;
      v_cap = new_cap;
    }
    visited[v_count++] = def;

    ast_t* next = unfold_one(r_alias);
    ast_free_unattached(r_alias);
    r_alias = next;
  }

  ponyint_pool_free_size(v_cap * sizeof(ast_t*), visited);
  return r_alias;
}
