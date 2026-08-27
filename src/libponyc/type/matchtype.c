#include "matchtype.h"
#include "cap.h"
#include "assemble.h"
#include "reify.h"
#include "subtype.h"
#include "typealias.h"
#include "typeparam.h"
#include "viewpoint.h"
#include "ponyassert.h"
#include "alias.h"
#include "../ast/astbuild.h"
#include "../../libponyrt/mem/pool.h"

// Substitution tracked across a single match_typeargs_pairwise walk. When
// the same type parameter appears in more than one position of the two
// typeargs lists being compared, every occurrence must reify to the same
// type; without tracking, Pair[A, A] vs Pair[String, U8] would accept each
// position independently even though no reification of A is both String
// and U8.
//
// The typeparam_root(def) canonicalization keys each binding. The bound
// type is a raw pointer into the AST tree being walked. When a recursive
// call unfolds a type alias — typealias_unfold returns a freshly allocated
// tree — subtrees of that unfolded tree may be bound; the tree therefore
// has to outlive the walk, so subst_keep parks it on the substitution and
// subst_free releases it at the end.
typedef struct
{
  ast_t* def;
  ast_t* type;
} subst_entry_t;

typedef struct
{
  subst_entry_t* entries;
  size_t len;
  size_t cap;
  ast_t** keep;
  size_t keep_len;
  size_t keep_cap;
} subst_t;

static void subst_init(subst_t* s)
{
  s->entries = NULL;
  s->len = 0;
  s->cap = 0;
  s->keep = NULL;
  s->keep_len = 0;
  s->keep_cap = 0;
}

static void subst_free(subst_t* s)
{
  if(s->entries != NULL)
  {
    ponyint_pool_free_size(s->cap * sizeof(subst_entry_t), s->entries);
    s->entries = NULL;
    s->len = 0;
    s->cap = 0;
  }

  if(s->keep != NULL)
  {
    for(size_t i = 0; i < s->keep_len; i++)
      ast_free_unattached(s->keep[i]);

    ponyint_pool_free_size(s->keep_cap * sizeof(ast_t*), s->keep);
    s->keep = NULL;
    s->keep_len = 0;
    s->keep_cap = 0;
  }
}

static ast_t* subst_lookup(subst_t* s, ast_t* def)
{
  if(s == NULL)
    return NULL;

  for(size_t i = 0; i < s->len; i++)
  {
    if(s->entries[i].def == def)
      return s->entries[i].type;
  }

  return NULL;
}

static void subst_bind(subst_t* s, ast_t* def, ast_t* type)
{
  if(s == NULL)
    return;

  // Invariant: callers look up first and only bind on a miss.
  pony_assert(subst_lookup(s, def) == NULL);

  if(s->len == s->cap)
  {
    size_t new_cap = (s->cap == 0) ? 4 : (s->cap * 2);
    size_t old_size = s->cap * sizeof(subst_entry_t);
    size_t new_size = new_cap * sizeof(subst_entry_t);
    s->entries = (subst_entry_t*)ponyint_pool_realloc_size(old_size, new_size,
      s->entries);
    s->cap = new_cap;
  }

  s->entries[s->len].def = def;
  s->entries[s->len].type = type;
  s->len++;
}

static void subst_keep(subst_t* s, ast_t* ast)
{
  if(s == NULL)
  {
    ast_free_unattached(ast);
    return;
  }

  if(s->keep_len == s->keep_cap)
  {
    size_t new_cap = (s->keep_cap == 0) ? 4 : (s->keep_cap * 2);
    size_t old_size = s->keep_cap * sizeof(ast_t*);
    size_t new_size = new_cap * sizeof(ast_t*);
    s->keep = (ast_t**)ponyint_pool_realloc_size(old_size, new_size, s->keep);
    s->keep_cap = new_cap;
  }

  s->keep[s->keep_len++] = ast;
}

// Save/restore checkpoint. Used by compound arm-covering to make each arm
// attempt's bindings transient: one arm's bind must not leak to sibling
// arms (which would spuriously reject `(P | A)` vs `(Q | A)`, where A can
// reify to (P | Q)), and no arm's transient bind persists past the compound
// branch. Restore truncates entries AND frees the extra keep trees; the
// entries added since the checkpoint are the only references to those trees.
typedef struct
{
  size_t entries_len;
  size_t keep_len;
} subst_checkpoint_t;

static subst_checkpoint_t subst_save(subst_t* s)
{
  subst_checkpoint_t cp;

  if(s == NULL)
  {
    cp.entries_len = 0;
    cp.keep_len = 0;
  } else {
    cp.entries_len = s->len;
    cp.keep_len = s->keep_len;
  }

  return cp;
}

static void subst_restore(subst_t* s, subst_checkpoint_t cp)
{
  if(s == NULL)
    return;

  for(size_t i = cp.keep_len; i < s->keep_len; i++)
    ast_free_unattached(s->keep[i]);

  s->keep_len = cp.keep_len;
  s->len = cp.entries_len;
}

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt);

static matchtype_t is_union_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the operand union accepts a match, then the entire
        // operand union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      // If any type in the operand union denies a match, then the entire
      // operand union is denied a match.
      case MATCHTYPE_DENY_CAP:
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(operand);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(child, pattern, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "no element of %s can match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_isect_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(operand);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(child, pattern, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the operand isect rejects a match, then the entire
        // operand isect rejects match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the operand isect denies a match, then the entire
        // operand isect is denied a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(operand);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(child, pattern, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "not every element of %s can match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_x_match_union(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_REJECT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        // If any type in the pattern union accepts a match, the entire pattern
        // union accepts a match.
        ok = MATCHTYPE_ACCEPT;
        break;

      case MATCHTYPE_REJECT:
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the pattern union denies a match, the entire pattern
        // union denies a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(pattern);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(operand, child, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot match any element of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_x_match_isect(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = MATCHTYPE_ACCEPT;

  for(ast_t* child = ast_child(pattern);
    child != NULL;
    child = ast_sibling(child))
  {
    switch(is_x_match_x(operand, child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        // If any type in the pattern isect rejects a match, the entire pattern
        // isect rejects a match.
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        // If any type in the pattern isect denies a match, the entire pattern
        // isect denies a match.
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      break;
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    for(ast_t* child = ast_child(pattern);
      child != NULL;
      child = ast_sibling(child))
    {
      is_x_match_x(operand, child, errorf, report_reject, opt);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot match every element of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_tuple_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // Must be a pairwise match.
  if(ast_childcount(operand) != ast_childcount(pattern))
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: they have a different number of elements",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  ast_t* operand_child = ast_child(operand);
  ast_t* pattern_child = ast_child(pattern);
  matchtype_t ok = MATCHTYPE_ACCEPT;

  while(operand_child != NULL)
  {
    switch(is_x_match_x(operand_child, pattern_child, NULL, false, opt))
    {
      case MATCHTYPE_ACCEPT:
        break;

      case MATCHTYPE_REJECT:
        ok = MATCHTYPE_REJECT;
        break;

      case MATCHTYPE_DENY_CAP:
        ok = MATCHTYPE_DENY_CAP;
        break;

      case MATCHTYPE_DENY_NODESC:
        ok = MATCHTYPE_DENY_NODESC;
        break;
    }

    if(ok != MATCHTYPE_ACCEPT)
      break;

    operand_child = ast_sibling(operand_child);
    pattern_child = ast_sibling(pattern_child);
  }

  if((ok != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if((ok == MATCHTYPE_DENY_CAP) || (ok == MATCHTYPE_DENY_NODESC))
      report_reject = false;

    operand_child = ast_child(operand);
    pattern_child = ast_child(pattern);

    while(operand_child != NULL)
    {
      is_x_match_x(operand_child, pattern_child, errorf, report_reject, opt);

      operand_child = ast_sibling(operand_child);
      pattern_child = ast_sibling(pattern_child);
    }

    if(ok == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(ok == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern, "%s cannot pairwise match %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return ok;
}

static matchtype_t is_nominal_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(!is_top_type(operand, true, opt))
  {
    if((errorf != NULL) && report_reject)
    {
      ast_t* operand_def = (ast_t*)ast_data(operand);

      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type is a tuple",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      ast_error_frame(errorf, operand_def, "this might be possible if the "
        "match type were an empty interface, such as the Any type");
    }

    return MATCHTYPE_REJECT;
  }

  ast_t* child = ast_child(pattern);

  while(child != NULL)
  {
    matchtype_t r = is_x_match_x(operand, child, errorf, false, opt);
    pony_assert(r != MATCHTYPE_REJECT);

    if(r == MATCHTYPE_DENY_CAP)
    {
      if(errorf != NULL)
      {
        ast_error_frame(errorf, pattern,
          "matching %s with %s could violate capabilities",
          ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      }

      return r;
    } else if (r == MATCHTYPE_DENY_NODESC) {
      if(errorf != NULL)
      {
        ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      }

      return r;
    }

    child = ast_sibling(child);
  }

  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_typeparam_match_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)opt;
  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  // Dig through defs if there are multiple layers of directly-bound
  // type params (created through the collect_type_params function).
  operand_def = typeparam_root(operand_def);
  pattern_def = typeparam_root(pattern_def);

  ast_t* o_cap = cap_fetch(operand);
  ast_t* o_eph = ast_sibling(o_cap);
  ast_t* p_cap = cap_fetch(pattern);
  ast_t* p_eph = ast_sibling(p_cap);

  matchtype_t r = MATCHTYPE_REJECT;

  if(operand_def == pattern_def)
  {
    r = is_cap_sub_cap_bound(ast_id(o_cap), TK_EPHEMERAL,
      ast_id(p_cap), ast_id(p_eph)) ? MATCHTYPE_ACCEPT : MATCHTYPE_DENY_CAP;
  }

  if((r != MATCHTYPE_ACCEPT) && (errorf != NULL))
  {
    if(r == MATCHTYPE_DENY_CAP)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a bound subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    } else if (r == MATCHTYPE_DENY_NODESC) {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type descriptor",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    } else if(report_reject) {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: they are different type parameters",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }
  }

  return r;
}

static matchtype_t is_typeparam_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(ast_id(pattern) == TK_TYPEPARAMREF)
  {
    matchtype_t ok = is_typeparam_match_typeparam(operand, pattern, errorf,
      false, opt);

    if(ok != MATCHTYPE_REJECT)
      return ok;
  }

  ast_t* operand_upper = typeparam_upper(operand);

  // An unconstrained typeparam could match anything.
  if(operand_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Check if the constraint can match the pattern.
  matchtype_t ok = is_x_match_x(operand_upper, pattern, errorf, report_reject,
    opt);
  ast_free_unattached(operand_upper);
  return ok;
}

static matchtype_t is_arrow_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // upperbound(this->T1) match T2
  // ---
  // (this->T1) match T2

  // lowerbound(T1->T2) match T3
  // ---
  // (T1->T2) match T3

  ast_t* operand_view;

  AST_GET_CHILDREN(operand, left, right);

  if(ast_id(left) == TK_THISTYPE)
    operand_view = viewpoint_upper(operand, opt);
  else
    operand_view = viewpoint_lower(operand, opt);

  if(operand_view == NULL)
  {
    if(errorf != NULL)
    {
      // this->X always has an upper bound.
      pony_assert(ast_id(left) != TK_THISTYPE);

      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "the match type has no lower bounds",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  matchtype_t ok = is_x_match_x(operand_view, pattern, errorf, report_reject,
    opt);
  ast_free_unattached(operand_view);
  return ok;
}

static matchtype_t is_x_match_tuple(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_tuple_match_tuple(operand, pattern, errorf, report_reject, opt);

    case TK_NOMINAL:
      return is_nominal_match_tuple(operand, pattern, errorf, report_reject,
        opt);

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_tuple(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static bool contains_typeparam(ast_t* ast)
{
  if(ast_id(ast) == TK_TYPEPARAMREF)
    return true;

  if(ast_id(ast) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(ast);
    if(unfolded == NULL)
      return false;

    bool r = contains_typeparam(unfolded);
    if(unfolded != ast)
      ast_free_unattached(unfolded);
    return r;
  }

  ast_t* child = ast_child(ast);
  while(child != NULL)
  {
    if(contains_typeparam(child))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// Does the type parameter identified by tp_def appear anywhere inside
// other's structure, under a generative type constructor (a nominal,
// a tuple, or an arrow)?
//
// A pair like A vs Wrap[A] cannot unify: no substitution for A satisfies
// A = Wrap[A] without an infinite type. Direct occurrences in a union or
// intersection arm do NOT count — A vs (A | U8) is satisfied by any
// reification with U8 ⊆ A (e.g. A = (U8 | I32) makes (A | U8) = A).
// under_wrapper becomes true when the walk descends into a generative
// constructor. Canonicalizes each typeparamref def through typeparam_root
// — a raw def pointer may be one layer in a chain built by
// collect_type_params.
static bool typeparam_occurs_in(ast_t* tp_def, ast_t* other,
  bool under_wrapper)
{
  if(ast_id(other) == TK_TYPEPARAMREF)
  {
    if(!under_wrapper)
      return false;

    ast_t* other_def = typeparam_root((ast_t*)ast_data(other));
    return other_def == tp_def;
  }

  if(ast_id(other) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(other);
    if(unfolded == NULL)
      return false;

    bool r = typeparam_occurs_in(tp_def, unfolded, under_wrapper);
    if(unfolded != other)
      ast_free_unattached(unfolded);
    return r;
  }

  token_id id = ast_id(other);
  bool descend_wrapped = under_wrapper
    || (id == TK_NOMINAL) || (id == TK_TUPLETYPE) || (id == TK_ARROW);

  ast_t* child = ast_child(other);
  while(child != NULL)
  {
    if(typeparam_occurs_in(tp_def, child, descend_wrapped))
      return true;

    child = ast_sibling(child);
  }

  return false;
}

static bool typeargs_could_unify(subst_t* subst, ast_t* a, ast_t* b,
  pass_opt_t* opt);

// A concrete entity — class, primitive, actor, or struct — has no user-
// visible subtype other than itself: Pony has no class-extends-class
// inheritance. Two distinct concrete entities therefore share no common
// inhabitant, which lets constraints_could_overlap reject that pair
// precisely instead of over-approximating.
static bool nominal_is_concrete_entity(ast_t* nominal)
{
  if(ast_id(nominal) != TK_NOMINAL)
    return false;

  ast_t* def = (ast_t*)ast_data(nominal);
  switch(ast_id(def))
  {
    case TK_CLASS:
    case TK_PRIMITIVE:
    case TK_ACTOR:
    case TK_STRUCT:
      return true;

    default:
      return false;
  }
}

// Do two typeparam constraints share any common inhabitant? Used by the
// two-typeparam branch of typeargs_could_unify to decide whether some
// concrete reification could satisfy both constraints simultaneously.
//
//   - The subtype fast path in either direction accepts.
//   - A union on either side distributes: (X | Y) shares an inhabitant
//     with Z iff X shares one with Z or Y does.
//   - After distribution reaches two nominals, only two distinct concrete
//     entities can be proved disjoint. Any other pair — trait or
//     interface on either side, intersection constraint, type alias
//     reference, arrow, tuple, typeparamref — is over-approximated to
//     accept.
//
// Over-approximation trade-off: two disjoint traits with no common
// implementer in the program accept unconditionally. Precise trait
// inhabitation would require walking every class in the program, which is
// not cheap and not available at this pass.
static bool constraints_could_overlap(ast_t* a, ast_t* b, pass_opt_t* opt)
{
  if(is_subtype_ignore_cap(a, b, NULL, opt))
    return true;

  if(is_subtype_ignore_cap(b, a, NULL, opt))
    return true;

  if(ast_id(a) == TK_UNIONTYPE)
  {
    ast_t* arm = ast_child(a);
    while(arm != NULL)
    {
      if(constraints_could_overlap(arm, b, opt))
        return true;

      arm = ast_sibling(arm);
    }

    return false;
  }

  if(ast_id(b) == TK_UNIONTYPE)
  {
    ast_t* arm = ast_child(b);
    while(arm != NULL)
    {
      if(constraints_could_overlap(a, arm, opt))
        return true;

      arm = ast_sibling(arm);
    }

    return false;
  }

  if(nominal_is_concrete_entity(a) && nominal_is_concrete_entity(b))
    return false;

  return true;
}

// True if any arm of `compound` could unify with `other`. Each arm attempt
// snapshots subst on entry and restores on exit — bindings added by a
// failed attempt must not leak to the next attempt, and bindings from a
// successful attempt must not leak past the compound to the outer walk
// (a bind of A→P from one arm attempt would then block a sibling arm from
// binding A→Q that would otherwise succeed). Lookups still see outer
// bindings, so a typeparam already bound by an earlier position is honored
// inside the arm attempts.
static bool any_arm_could_unify(subst_t* subst, ast_t* compound, ast_t* other,
  pass_opt_t* opt)
{
  ast_t* arm = ast_child(compound);
  while(arm != NULL)
  {
    subst_checkpoint_t cp = subst_save(subst);
    bool r = typeargs_could_unify(subst, arm, other, opt);
    subst_restore(subst, cp);

    if(r)
      return true;

    arm = ast_sibling(arm);
  }

  return false;
}

// True if every arm of `compound` could unify with `other`. Same
// snapshot/restore discipline as any_arm_could_unify — bindings from one
// arm are transient. This is safe because compound-arm consistency is not
// enforced across arms here; outer bindings are still visible via lookup.
static bool every_arm_could_unify(subst_t* subst, ast_t* compound, ast_t* other,
  pass_opt_t* opt)
{
  ast_t* arm = ast_child(compound);
  while(arm != NULL)
  {
    subst_checkpoint_t cp = subst_save(subst);
    bool r = typeargs_could_unify(subst, arm, other, opt);
    subst_restore(subst, cp);

    if(!r)
      return false;

    arm = ast_sibling(arm);
  }

  return true;
}

// True if `other` is a subtype of every arm of `compound` (an
// intersection). Used for intersection-vs-non-compound: an intersection
// `(X & Y & ...)` could reify to a runtime type equal to `other` iff
// `other` is an inhabitant, i.e. a subtype of every arm.
// is_subtype_ignore_cap handles typeparam arms by checking against their
// constraints, so a typeparam arm A accepts `other` iff `other` is
// admitted by A's constraint.
static bool other_is_subtype_of_every_arm(ast_t* compound, ast_t* other,
  pass_opt_t* opt)
{
  ast_t* arm = ast_child(compound);
  while(arm != NULL)
  {
    if(!is_subtype_ignore_cap(other, arm, NULL, opt))
      return false;

    arm = ast_sibling(arm);
  }

  return true;
}

// Could two type-argument positions reify to equal types when at least one
// side is a union or intersection literal? Callers must have already ruled
// out the type-parameter branches — a bare type parameter on either side
// is handled by typeargs_could_unify's earlier branches, whose occurs
// check would otherwise be bypassed.
//
// `subst` is looked up but effectively read-only through this branch:
// any_arm_could_unify and every_arm_could_unify snapshot on each arm
// attempt and restore on exit. Outer bindings from an earlier position
// are honored inside arm attempts (so `Pair[A, (A | U8)]` vs
// `Pair[String val, (I32 | U8)]` rejects — position 1 sees A bound to
// String and no arm of the pattern matches). Bindings added inside an
// arm attempt don't leak — required for `(P | A)` vs `(Q | A)` to accept,
// which needs each arm to be tried with a fresh A.
//
// Rules:
//   - Same kind (both union or both intersection): each arm on either side
//     must have some arm on the other side that could unify.
//   - Mixed kinds (union vs intersection): any arm-vs-arm pair that could
//     unify accepts. A common subtype of both sides supplies a valid
//     reification.
//   - Union vs non-compound: every arm on the union side must could-unify
//     with the non-compound side. `(X | Y | ...) = W` requires every arm
//     to reify equal to W (union collapse).
//   - Intersection vs non-compound: `(X & Y & ...) = W` requires W to be
//     a subtype of every arm (so W inhabits the intersection). A
//     typeparam arm accepts W iff W is admitted by the arm's constraint,
//     which is_subtype_ignore_cap already models.
//
// Residual gap: a typeparam bound inside a compound arm doesn't constrain
// positions outside the compound. Fully closing that requires backtracking
// across arm pairings, which is combinatorial.
static bool compound_types_could_unify(subst_t* subst, ast_t* a, ast_t* b,
  pass_opt_t* opt)
{
  bool a_union = ast_id(a) == TK_UNIONTYPE;
  bool a_isect = ast_id(a) == TK_ISECTTYPE;
  bool b_union = ast_id(b) == TK_UNIONTYPE;
  bool b_isect = ast_id(b) == TK_ISECTTYPE;
  bool a_compound = a_union || a_isect;
  bool b_compound = b_union || b_isect;

  pony_assert(a_compound || b_compound);

  if((a_union && b_union) || (a_isect && b_isect))
  {
    ast_t* arm = ast_child(a);
    while(arm != NULL)
    {
      if(!any_arm_could_unify(subst, b, arm, opt))
        return false;

      arm = ast_sibling(arm);
    }

    arm = ast_child(b);
    while(arm != NULL)
    {
      if(!any_arm_could_unify(subst, a, arm, opt))
        return false;

      arm = ast_sibling(arm);
    }

    return true;
  }

  if(a_compound && b_compound)
  {
    ast_t* a_arm = ast_child(a);
    while(a_arm != NULL)
    {
      if(any_arm_could_unify(subst, b, a_arm, opt))
        return true;

      a_arm = ast_sibling(a_arm);
    }

    return false;
  }

  ast_t* compound = a_compound ? a : b;
  ast_t* other = a_compound ? b : a;

  if(ast_id(compound) == TK_UNIONTYPE)
    return every_arm_could_unify(subst, compound, other, opt);

  return other_is_subtype_of_every_arm(compound, other, opt);
}

// Two single-method interfaces with different def pointers can still be
// structurally equivalent — lambda types desugared at different source
// positions, or a user-defined interface vs a lambda. When
// typeargs_could_unify encounters two such nominals, it compares the
// interfaces structurally rather than by identity.
static bool single_method_interfaces_could_unify(subst_t* subst,
  ast_t* a, ast_t* a_def, ast_t* b, ast_t* b_def, pass_opt_t* opt)
{
  if(ast_id(a_def) != TK_INTERFACE || ast_id(b_def) != TK_INTERFACE)
    return false;

  // Both must have exactly one member.
  ast_t* a_members = ast_childidx(a_def, 4);
  ast_t* b_members = ast_childidx(b_def, 4);
  ast_t* a_method = ast_child(a_members);
  ast_t* b_method = ast_child(b_members);

  if(a_method == NULL || b_method == NULL)
    return false;

  if(ast_sibling(a_method) != NULL || ast_sibling(b_method) != NULL)
    return false;

  // Reify both methods with their class-level type arguments.
  ast_t* a_typeparams = ast_childidx(a_def, 1);
  ast_t* a_typeargs = ast_childidx(a, 2);
  ast_t* b_typeparams = ast_childidx(b_def, 1);
  ast_t* b_typeargs = ast_childidx(b, 2);

  ast_t* r_a = reify_method_def(a_method, a_typeparams, a_typeargs, opt);
  if(r_a == NULL) return false;

  ast_t* r_b = reify_method_def(b_method, b_typeparams, b_typeargs, opt);
  if(r_b == NULL)
  {
    if(r_a != a_method) ast_free_unattached(r_a);
    return false;
  }

  bool result = false;

  // Structural shape checks.
  token_id t_a = ast_id(r_a);
  token_id t_b = ast_id(r_b);

  if(((t_a == TK_NEW) || (t_b == TK_NEW)) && (t_a != t_b))
    goto cleanup;

  AST_GET_CHILDREN(r_a, a_cap, a_id, a_tps, a_params, a_result, a_throws);
  AST_GET_CHILDREN(r_b, b_cap, b_id, b_tps, b_params, b_result, b_throws);

  if(ast_name(a_id) != ast_name(b_id))
    goto cleanup;

  if((ast_id(a_cap) == TK_AT) != (ast_id(b_cap) == TK_AT))
    goto cleanup;

  if(ast_childcount(a_tps) != ast_childcount(b_tps))
    goto cleanup;

  if(ast_childcount(a_params) != ast_childcount(b_params))
    goto cleanup;

  // If method-level type params exist, reify a's method with b's type params
  // so both are in the same variable space (mirrors structural_could_match_pattern).
  ast_t* rr_a = r_a;
  if(ast_id(b_tps) != TK_NONE)
  {
    BUILD(typeargs, b_tps, NODE(TK_TYPEARGS));
    ast_t* tp = ast_child(b_tps);
    while(tp != NULL)
    {
      AST_GET_CHILDREN(tp, tp_id, tp_constraint);
      BUILD(typearg, tp, NODE(TK_TYPEPARAMREF, TREE(tp_id) NONE NONE));
      ast_t* def = ast_get(tp, ast_name(tp_id), NULL);
      ast_setdata(typearg, def);
      typeparam_set_cap(typearg);
      ast_append(typeargs, typearg);
      tp = ast_sibling(tp);
    }

    rr_a = reify_method_def(r_a, a_tps, typeargs, opt);
    ast_free_unattached(typeargs);

    a_result = ast_childidx(rr_a, 4);
    a_throws = ast_childidx(rr_a, 5);
    a_params = ast_childidx(rr_a, 3);
  }

  // Throws must match for unification.
  if((ast_id(a_throws) == TK_QUESTION) != (ast_id(b_throws) == TK_QUESTION))
    goto cleanup_rr;

  // Result types must be unifiable.
  if(!typeargs_could_unify(subst, a_result, b_result, opt))
    goto cleanup_rr;

  // Parameters.
  {
    ast_t* ap = ast_child(a_params);
    ast_t* bp = ast_child(b_params);
    while(ap != NULL && bp != NULL)
    {
      ast_t* a_type = ast_childidx(ap, 1);
      ast_t* b_type = ast_childidx(bp, 1);

      if(!typeargs_could_unify(subst, a_type, b_type, opt))
        goto cleanup_rr;

      ap = ast_sibling(ap);
      bp = ast_sibling(bp);
    }
  }

  result = true;

cleanup_rr:
  if(rr_a != r_a)
    subst_keep(subst, rr_a);

cleanup:
  if(r_a != a_method) subst_keep(subst, r_a);
  if(r_b != b_method) subst_keep(subst, r_b);
  return result;
}

// Could two type-argument positions reify to equal types for some
// substitution of the type parameters they contain?
//
// The is_eqtype fast path is cap-aware. The type-parameter fallback
// deliberately ignores capabilities: the outer entity descriptor identity
// check at runtime discriminates on the fully reified type arguments'
// caps, so a compile-time cap check on the pair itself would spuriously
// reject matches the runtime would accept.
//
// `subst` threads a substitution through the recursion so repeated
// occurrences of the same type parameter across positions must reify to
// the same type. Compound branches thread subst too, but each arm attempt
// snapshots and restores it (see any_arm_could_unify) so within-compound
// bindings are transient.
//
//   - eqtype pairs unify.
//   - a bare type parameter unifies with anything the other side satisfies
//     as a reification of the parameter's constraint (subtype, ignoring
//     caps); an unconstrained parameter unifies with anything. The
//     parameter's def must not appear inside a generative constructor on
//     the other side (occurs check): no reification satisfies A = f(A)
//     without an infinite type. Occurrences in a union or intersection
//     arm are not generative — A vs (A | U8) can unify. If the parameter
//     is already bound in subst, the recorded binding must in turn unify
//     with the other side; on a miss we record the binding.
//   - two type parameters unify when either side is bound in subst and its
//     binding unifies with the other side, or when neither side is bound
//     and their constraints share any common inhabitant (see
//     constraints_could_overlap for the check and its over-approximation
//     trade-offs). The two-typeparam case doesn't add its own binding —
//     that would require unifying two unification variables — but honors
//     bindings recorded by earlier positions.
//   - a union or intersection literal on either side (with neither side a
//     bare type parameter) distributes over the arms (see
//     compound_types_could_unify).
//   - an arrow type (viewpoint adaptation) on either side: when both
//     sides are arrows with the same left-hand side, the viewpoint is
//     deterministic so the arrows unify iff the right-hand sides do.
//     Otherwise, each arrow is reduced to its viewpoint bound
//     (viewpoint_upper for this->X, viewpoint_lower for P->X) and the
//     bounds are compared recursively.
//   - two same-definition nominals unify when every pair of type arguments
//     recursively unifies.
//   - two tuples unify when they have the same arity and every element
//     pair recursively unifies.
//   - a type alias reference is unfolded and re-tried.
//   - anything else does not unify.
static bool typeargs_could_unify(subst_t* subst, ast_t* a, ast_t* b,
  pass_opt_t* opt)
{
  if(is_eqtype(a, b, NULL, opt))
    return true;

  if(ast_id(a) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(a);
    if(unfolded == NULL)
      return false;

    bool r = typeargs_could_unify(subst, unfolded, b, opt);
    if(unfolded != a)
      subst_keep(subst, unfolded);
    return r;
  }

  if(ast_id(b) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(b);
    if(unfolded == NULL)
      return false;

    bool r = typeargs_could_unify(subst, a, unfolded, opt);
    if(unfolded != b)
      subst_keep(subst, unfolded);
    return r;
  }

  bool a_tp = ast_id(a) == TK_TYPEPARAMREF;
  bool b_tp = ast_id(b) == TK_TYPEPARAMREF;

  if(a_tp && b_tp)
  {
    ast_t* a_def = typeparam_root((ast_t*)ast_data(a));
    ast_t* b_def = typeparam_root((ast_t*)ast_data(b));

    // If either side is bound in subst, recurse through the binding — a
    // later position that pairs an already-bound typeparam with another
    // typeparam must check the bound value, not just constraint overlap.
    ast_t* a_bound = subst_lookup(subst, a_def);
    ast_t* b_bound = subst_lookup(subst, b_def);
    if(a_bound != NULL && b_bound != NULL)
      return typeargs_could_unify(subst, a_bound, b_bound, opt);
    if(a_bound != NULL)
      return typeargs_could_unify(subst, a_bound, b, opt);
    if(b_bound != NULL)
      return typeargs_could_unify(subst, a, b_bound, opt);

    ast_t* a_up = typeparam_upper(a);
    ast_t* b_up = typeparam_upper(b);
    bool r;

    if((a_up == NULL) || (b_up == NULL))
      r = true;
    else
      r = constraints_could_overlap(a_up, b_up, opt);

    if(a_up != NULL) ast_free_unattached(a_up);
    if(b_up != NULL) ast_free_unattached(b_up);
    return r;
  }

  if(a_tp || b_tp)
  {
    ast_t* tp = a_tp ? a : b;
    ast_t* other = a_tp ? b : a;

    ast_t* tp_def = typeparam_root((ast_t*)ast_data(tp));
    if(typeparam_occurs_in(tp_def, other, false))
      return false;

    ast_t* bound = subst_lookup(subst, tp_def);
    if(bound != NULL)
      return typeargs_could_unify(subst, bound, other, opt);

    ast_t* upper = typeparam_upper(tp);
    if(upper != NULL)
    {
      bool ok = is_subtype_ignore_cap(other, upper, NULL, opt);
      ast_free_unattached(upper);
      if(!ok)
        return false;
    }

    subst_bind(subst, tp_def, other);
    return true;
  }

  if((ast_id(a) == TK_UNIONTYPE) || (ast_id(a) == TK_ISECTTYPE)
    || (ast_id(b) == TK_UNIONTYPE) || (ast_id(b) == TK_ISECTTYPE))
  {
    return compound_types_could_unify(subst, a, b, opt);
  }

  if(ast_id(a) == TK_ARROW || ast_id(b) == TK_ARROW)
  {
    if(ast_id(a) == TK_ARROW && ast_id(b) == TK_ARROW)
    {
      ast_t* a_left = ast_child(a);
      ast_t* b_left = ast_child(b);

      bool same_lhs = false;

      if(ast_id(a_left) == ast_id(b_left))
      {
        switch(ast_id(a_left))
        {
          case TK_THISTYPE:
            same_lhs = true;
            break;

          case TK_TYPEPARAMREF:
            same_lhs = typeparam_root((ast_t*)ast_data(a_left)) ==
                        typeparam_root((ast_t*)ast_data(b_left));
            break;

          default:
            same_lhs = is_eqtype(a_left, b_left, NULL, opt);
            break;
        }
      }

      if(same_lhs)
      {
        ast_t* a_right = ast_sibling(a_left);
        ast_t* b_right = ast_sibling(b_left);
        return typeargs_could_unify(subst, a_right, b_right, opt);
      }
    }

    ast_t* r_a = a;
    ast_t* r_b = b;

    if(ast_id(a) == TK_ARROW)
    {
      AST_GET_CHILDREN(a, a_left, a_right);
      if(ast_id(a_left) == TK_THISTYPE)
        r_a = viewpoint_upper(a, opt);
      else
        r_a = viewpoint_lower(a, opt);

      if(r_a == NULL)
        return false;
    }

    if(ast_id(b) == TK_ARROW)
    {
      AST_GET_CHILDREN(b, b_left, b_right);
      if(ast_id(b_left) == TK_THISTYPE)
        r_b = viewpoint_upper(b, opt);
      else
        r_b = viewpoint_lower(b, opt);

      if(r_b == NULL)
      {
        if(r_a != a)
          ast_free_unattached(r_a);
        return false;
      }
    }

    bool result = typeargs_could_unify(subst, r_a, r_b, opt);

    if(r_a != a) subst_keep(subst, r_a);
    if(r_b != b) subst_keep(subst, r_b);

    return result;
  }

  if((ast_id(a) == TK_NOMINAL) && (ast_id(b) == TK_NOMINAL))
  {
    ast_t* a_def = (ast_t*)ast_data(a);
    ast_t* b_def = (ast_t*)ast_data(b);

    if(a_def != b_def)
      return single_method_interfaces_could_unify(subst, a, a_def, b, b_def,
        opt);

    ast_t* a_typeargs = ast_childidx(a, 2);
    ast_t* b_typeargs = ast_childidx(b, 2);

    ast_t* a_arg = ast_child(a_typeargs);
    ast_t* b_arg = ast_child(b_typeargs);

    while((a_arg != NULL) && (b_arg != NULL))
    {
      if(!typeargs_could_unify(subst, a_arg, b_arg, opt))
        return false;

      a_arg = ast_sibling(a_arg);
      b_arg = ast_sibling(b_arg);
    }

    return (a_arg == NULL) && (b_arg == NULL);
  }

  if((ast_id(a) == TK_TUPLETYPE) && (ast_id(b) == TK_TUPLETYPE))
  {
    ast_t* a_elem = ast_child(a);
    ast_t* b_elem = ast_child(b);

    while((a_elem != NULL) && (b_elem != NULL))
    {
      if(!typeargs_could_unify(subst, a_elem, b_elem, opt))
        return false;

      a_elem = ast_sibling(a_elem);
      b_elem = ast_sibling(b_elem);
    }

    return (a_elem == NULL) && (b_elem == NULL);
  }

  return false;
}

// Pairwise match of two type-argument lists for same-definition nominals.
//
// For each pair:
//   - is_eqtype: the pair matches.
//   - both sides fully concrete but not eqtype: reject.
//   - a type parameter appears in either side:
//       - struct_pattern: deny_nodesc. A struct has no runtime descriptor,
//         so the type argument cannot be checked; the runtime would treat
//         any match as unconditional.
//       - otherwise: check whether the pair could unify under some
//         reification of the type parameters.
//
// A deny_nodesc from any pair propagates; a single failing pair otherwise
// rejects the whole list.
static matchtype_t match_typeargs_pairwise(ast_t* o_typeargs, ast_t* p_typeargs,
  bool struct_pattern, pass_opt_t* opt)
{
  ast_t* o_arg = ast_child(o_typeargs);
  ast_t* p_arg = ast_child(p_typeargs);

  matchtype_t ok = MATCHTYPE_ACCEPT;
  subst_t subst;
  subst_init(&subst);

  while((o_arg != NULL) && (p_arg != NULL))
  {
    matchtype_t pair;

    if(is_eqtype(o_arg, p_arg, NULL, opt))
      pair = MATCHTYPE_ACCEPT;
    else if(!contains_typeparam(o_arg) && !contains_typeparam(p_arg))
      pair = MATCHTYPE_REJECT;
    else if(struct_pattern)
      pair = MATCHTYPE_DENY_NODESC;
    else if(typeargs_could_unify(&subst, o_arg, p_arg, opt))
      pair = MATCHTYPE_ACCEPT;
    else
      pair = MATCHTYPE_REJECT;

    if(pair == MATCHTYPE_DENY_NODESC)
    {
      subst_free(&subst);
      return MATCHTYPE_DENY_NODESC;
    }

    if((pair == MATCHTYPE_REJECT) && (ok == MATCHTYPE_ACCEPT))
      ok = MATCHTYPE_REJECT;

    o_arg = ast_sibling(o_arg);
    p_arg = ast_sibling(p_arg);
  }

  subst_free(&subst);
  return ok;
}

static matchtype_t is_nominal_match_entity(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // We say the pattern provides the operand if it is a subtype without taking
  // capabilities into account.
  bool provides = is_subtype_ignore_cap(pattern, operand, NULL, opt);

  // If the strict subtype check rejects but the operand and pattern share
  // the same entity definition, the rejection may be caused solely by type
  // arguments that cannot be proved equivalent when a type parameter is
  // involved. Re-check the type arguments pairwise; a type parameter can
  // reify to make the pair equal at runtime, and a same-definition entity
  // pattern is discriminated at runtime by descriptor identity of the fully
  // reified type. See #723.
  if(!provides)
  {
    ast_t* operand_def = (ast_t*)ast_data(operand);
    ast_t* pattern_def = (ast_t*)ast_data(pattern);

    if(operand_def == pattern_def
      && match_typeargs_pairwise(o_typeargs, p_typeargs, false, opt)
        == MATCHTYPE_ACCEPT)
    {
      provides = true;
    }
  }

  // If the pattern doesn't provide the operand, reject the match.
  if(!provides)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: %s isn't a subtype of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type_no_cap(pattern, opt->strtab), ast_print_type_no_cap(operand, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), ast_id(o_eph),
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));

        if(is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL, ast_id(p_cap),
          ast_id(p_eph)))
          ast_error_frame(errorf, o_cap,
            "this would be possible if the subcap were more ephemeral. "
            "Perhaps you meant to consume this variable");
  }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_nominal_match_struct(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // A struct pattern can only be used if the operand is the same struct.
  // Otherwise, since there is no type descriptor, there is no way to
  // determine a match at runtime.
  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  // This must be a deny to prevent a union or intersection type that includes
  // the same struct as the pattern from matching.
  if(operand_def != pattern_def)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type is a struct",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
      ast_error_frame(errorf, pattern,
        "since a struct has no type descriptor, pattern matching at runtime "
        "would be impossible");
    }

    return MATCHTYPE_DENY_NODESC;
  }

  // Same-definition struct: a type parameter inside a type argument cannot
  // be discriminated at runtime (a struct has no descriptor), so accepting
  // the pair would make the runtime match unconditional. Deny here; the
  // concrete-argument cases fall through to the entity path, which reports
  // reject or accept as it does for any other same-definition nominal.
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  if(match_typeargs_pairwise(o_typeargs, p_typeargs, true, opt)
    == MATCHTYPE_DENY_NODESC)
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s is not possible, since a struct lacks a type "
        "descriptor",
        ast_print_type(operand, opt->strtab),
        ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_DENY_NODESC;
  }

  return is_nominal_match_entity(operand, pattern, errorf, report_reject, opt);
}

// Same-def match in an open-ended provides walk: the operand's declared
// provides list is followed transitively; at any reified provided nominal
// whose def matches the pattern's, the type-argument pair is checked with
// match_typeargs_pairwise, which accepts when a type parameter could reify
// to make the pair equal. See #5859.
//
// Handles nominal provides only. Structural interface satisfaction is
// handled separately in structural_could_match_pattern below.
//
// The walk terminates because Pony rejects cyclic trait/interface provides
// declarations at compile time ("traits and interfaces can't be recursive"),
// so the provides DAG the walk follows is finite.
static bool provides_could_match_pattern(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  if(ast_id(operand) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(operand);
    if(unfolded == NULL)
      return false;
    bool r = provides_could_match_pattern(unfolded, pattern, opt);
    if(unfolded != operand)
      ast_free_unattached(unfolded);
    return r;
  }

  if(ast_id(operand) != TK_NOMINAL)
    return false;

  ast_t* operand_def = (ast_t*)ast_data(operand);
  ast_t* pattern_def = (ast_t*)ast_data(pattern);
  ast_t* o_typeargs = ast_childidx(operand, 2);
  ast_t* p_typeargs = ast_childidx(pattern, 2);

  if(operand_def == pattern_def)
  {
    return match_typeargs_pairwise(o_typeargs, p_typeargs, false, opt)
      == MATCHTYPE_ACCEPT;
  }

  AST_GET_CHILDREN(operand_def, o_id, o_typeparams, o_defcap, o_provides);
  ast_t* child = ast_child(o_provides);
  while(child != NULL)
  {
    ast_t* r_child = reify(child, o_typeparams, o_typeargs, opt, true);
    pony_assert(r_child != NULL);

    bool matched = provides_could_match_pattern(r_child, pattern, opt);

    if(r_child != child)
      ast_free_unattached(r_child);

    if(matched)
      return true;

    child = ast_sibling(child);
  }

  return false;
}

// Loose structural match: could the operand entity structurally satisfy the
// pattern interface for some reification of type parameters in their type
// arguments?
//
// Mirrors is_nominal_sub_structural in subtype.c but replaces strict subtype
// checks on method result types and parameter types with
// typeargs_could_unify. This avoids duplicating the four subtype recursion
// sites in is_reified_fun_sub_fun — we only need to know whether a
// reification COULD make the method signatures compatible, not whether they
// ARE compatible right now.
//
// Structural checks that remain strict: method existence, name match,
// fun/be/new kind compatibility, type-parameter count, parameter count,
// bareness, and throws. Receiver capability and type-parameter constraints
// are not checked here; any mismatch is caught by the full structural
// subtype check at runtime, so this is a safe over-approximation.
static bool structural_could_match_pattern(ast_t* operand, ast_t* pattern,
  pass_opt_t* opt)
{
  ast_t* sub_def = (ast_t*)ast_data(operand);
  ast_t* super_def = (ast_t*)ast_data(pattern);

  if(ast_has_annotation(sub_def, "nosupertype", opt->strtab))
    return false;

  if(is_bare(operand, opt) != is_bare(pattern, opt))
    return false;

  ast_t* sub_typeargs = ast_childidx(operand, 2);
  ast_t* sub_typeparams = ast_childidx(sub_def, 1);

  ast_t* super_typeargs = ast_childidx(pattern, 2);
  ast_t* super_typeparams = ast_childidx(super_def, 1);

  ast_t* super_members = ast_childidx(super_def, 4);
  ast_t* super_member = ast_child(super_members);

  subst_t subst;
  subst_init(&subst);

  while(super_member != NULL)
  {
    ast_t* super_member_id = ast_childidx(super_member, 1);
    ast_t* sub_member = ast_get(sub_def, ast_name(super_member_id), NULL);

    if((sub_member == NULL) || (ast_id(sub_member) != TK_FUN &&
      ast_id(sub_member) != TK_BE && ast_id(sub_member) != TK_NEW))
    {
      subst_free(&subst);
      return false;
    }

    // Reify both methods with their class-level type arguments.
    ast_t* r_sub = reify_method_def(sub_member, sub_typeparams,
      sub_typeargs, opt);
    pony_assert(r_sub != NULL);

    ast_t* r_super = reify_method_def(super_member, super_typeparams,
      super_typeargs, opt);
    pony_assert(r_super != NULL);

    // Structural shape checks (these don't depend on type-arg reification).
    token_id tsub = ast_id(r_sub);
    token_id tsuper = ast_id(r_super);

    // A constructor can only match a constructor.
    if(((tsub == TK_NEW) || (tsuper == TK_NEW)) && (tsub != tsuper))
    {
      if(r_sub != sub_member) ast_free_unattached(r_sub);
      if(r_super != super_member) ast_free_unattached(r_super);
      subst_free(&subst);
      return false;
    }

    AST_GET_CHILDREN(r_sub, sub_cap, sub_id, sub_tps, sub_params,
      sub_result, sub_throws);
    AST_GET_CHILDREN(r_super, super_cap, super_id, super_tps, super_params,
      super_result, super_throws);

    // Must have same number of type parameters and parameters.
    if(ast_childcount(sub_tps) != ast_childcount(super_tps)
      || ast_childcount(sub_params) != ast_childcount(super_params))
    {
      if(r_sub != sub_member) ast_free_unattached(r_sub);
      if(r_super != super_member) ast_free_unattached(r_super);
      subst_free(&subst);
      return false;
    }

    // Bareness must match.
    bool sub_bare = ast_id(sub_cap) == TK_AT;
    bool super_bare = ast_id(super_cap) == TK_AT;
    if(sub_bare != super_bare)
    {
      if(r_sub != sub_member) ast_free_unattached(r_sub);
      if(r_super != super_member) ast_free_unattached(r_super);
      subst_free(&subst);
      return false;
    }

    // If the super method has method-level type params, reify the sub method
    // with the super's type params so both are in the same variable space
    // (mirrors is_fun_sub_fun).
    ast_t* rr_sub = r_sub;
    if(ast_id(super_tps) != TK_NONE)
    {
      BUILD(typeargs, super_tps, NODE(TK_TYPEARGS));
      ast_t* stp = ast_child(super_tps);
      while(stp != NULL)
      {
        AST_GET_CHILDREN(stp, stp_id, stp_constraint);
        BUILD(typearg, stp, NODE(TK_TYPEPARAMREF, TREE(stp_id) NONE NONE));
        ast_t* def = ast_get(stp, ast_name(stp_id), NULL);
        ast_setdata(typearg, def);
        typeparam_set_cap(typearg);
        ast_append(typeargs, typearg);
        stp = ast_sibling(stp);
      }

      rr_sub = reify_method_def(r_sub, sub_tps, typeargs, opt);
      ast_free_unattached(typeargs);

      // Re-extract children from the re-reified sub method.
      sub_result = ast_childidx(rr_sub, 4);
      sub_throws = ast_childidx(rr_sub, 5);
      sub_params = ast_childidx(rr_sub, 3);
    }

    // Park reified trees on the substitution so typeargs_could_unify's
    // subst bindings (which point into these trees) stay valid across
    // method iterations. subst_free releases them at the end.
    if(rr_sub != r_sub) subst_keep(&subst, rr_sub);
    if(r_sub != sub_member) subst_keep(&subst, r_sub);
    if(r_super != super_member) subst_keep(&subst, r_super);

    // Covariant result: could the types unify?
    if(!typeargs_could_unify(&subst, sub_result, super_result, opt))
    {
      subst_free(&subst);
      return false;
    }

    // Contravariant parameters: could each pair unify?
    ast_t* sub_param = ast_child(sub_params);
    ast_t* super_param = ast_child(super_params);
    bool params_ok = true;
    while((sub_param != NULL) && (super_param != NULL))
    {
      ast_t* sub_type = ast_childidx(sub_param, 1);
      ast_t* super_type = ast_childidx(super_param, 1);

      if(!typeargs_could_unify(&subst, sub_type, super_type, opt))
      {
        params_ok = false;
        break;
      }

      sub_param = ast_sibling(sub_param);
      super_param = ast_sibling(super_param);
    }

    if(!params_ok)
    {
      subst_free(&subst);
      return false;
    }

    // Covariant throws.
    if((ast_id(sub_throws) == TK_QUESTION) &&
      (ast_id(super_throws) != TK_QUESTION))
    {
      subst_free(&subst);
      return false;
    }

    super_member = ast_sibling(super_member);
  }

  subst_free(&subst);
  return true;
}

static matchtype_t is_entity_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  bool provides = is_subtype_ignore_cap(operand, pattern, NULL, opt);

  // If the strict subtype check rejects, the rejection may be caused solely
  // by type arguments that cannot be proved equivalent when a type parameter
  // is involved. Walk the operand's `provides` transitively: at any reified
  // provided type that shares its def with the pattern, a type parameter
  // could reify to make the pair equal at runtime, and the reachable-trait
  // bitmap discriminates on the fully reified type. See #5859.
  //
  // Struct operands are excluded: is_struct_sub_trait denies unconditionally
  // because a struct has no descriptor to discriminate a trait/interface
  // match at runtime, and that denial is load-bearing — accepting here would
  // compile a match with no runtime check and can crash on dispatch.
  if(!provides
    && ast_id((ast_t*)ast_data(operand)) != TK_STRUCT
    && provides_could_match_pattern(operand, pattern, opt))
    provides = true;

  // If the nominal provides walk didn't find a match and the pattern is an
  // interface, the class may still satisfy it structurally (no `is I`
  // declaration). Check whether the entity's methods could match the
  // interface's methods for some reification of the type parameters. See
  // #5863.
  if(!provides
    && ast_id((ast_t*)ast_data(operand)) != TK_STRUCT
    && ast_id((ast_t*)ast_data(pattern)) == TK_INTERFACE
    && structural_could_match_pattern(operand, pattern, opt))
    provides = true;

  // If the operand doesn't provide the pattern (trait or interface), reject
  // the match.
  if(!provides)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: %s isn't a subtype of %s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type_no_cap(operand, opt->strtab), ast_print_type_no_cap(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  // If the operand does provide the pattern, but the operand refcap can't
  // match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL,
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_trait_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)report_reject;
  (void)opt;
  AST_GET_CHILDREN(operand, o_pkg, o_id, o_typeargs, o_cap, o_eph);
  AST_GET_CHILDREN(pattern, p_pkg, p_id, p_typeargs, p_cap, p_eph);

  // If the operand refcap can't match the pattern refcap, deny the match.
  if(!is_cap_sub_cap(ast_id(o_cap), TK_EPHEMERAL,
    ast_id(p_cap), ast_id(p_eph)))
  {
    if(errorf != NULL)
    {
      ast_error_frame(errorf, pattern,
        "matching %s with %s could violate capabilities: "
        "%s%s isn't a subcap of %s%s",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab),
        ast_print_type(o_cap, opt->strtab), ast_print_type(o_eph, opt->strtab),
        ast_print_type(p_cap, opt->strtab), ast_print_type(p_eph, opt->strtab));
    }

    return MATCHTYPE_DENY_CAP;
  }

  // Otherwise, accept the match.
  return MATCHTYPE_ACCEPT;
}

static matchtype_t is_nominal_match_trait(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  ast_t* operand_def = (ast_t*)ast_data(operand);

  switch(ast_id(operand_def))
  {
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return is_entity_match_trait(operand, pattern, errorf, report_reject,
        opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_trait_match_trait(operand, pattern, errorf, report_reject,
        opt);

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_nominal_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  ast_t* pattern_def = (ast_t*)ast_data(pattern);

  switch(ast_id(pattern_def))
  {
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      return is_nominal_match_entity(operand, pattern, errorf, report_reject,
        opt);

    case TK_STRUCT:
      return is_nominal_match_struct(operand, pattern, errorf, report_reject,
        opt);

    case TK_TRAIT:
    case TK_INTERFACE:
      return is_nominal_match_trait(operand, pattern, errorf, report_reject,
        opt);

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_tuple_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  (void)opt;

  if((errorf != NULL) && report_reject)
  {
    ast_error_frame(errorf, pattern,
      "%s cannot match %s: the match type is a tuple",
      ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
  }

  return MATCHTYPE_REJECT;
}

static matchtype_t is_x_match_nominal(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_tuple_match_nominal(operand, pattern, errorf, report_reject,
        opt);

    case TK_NOMINAL:
      return is_nominal_match_nominal(operand, pattern, errorf, report_reject,
        opt);

    case TK_TYPEPARAMREF:
      return is_typeparam_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_nominal(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_REJECT;

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_x_match_base_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  switch(ast_id(operand))
  {
    case TK_UNIONTYPE:
      return is_union_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_isect_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
    case TK_NOMINAL:
      return MATCHTYPE_REJECT;

    case TK_TYPEPARAMREF:
      return is_typeparam_match_typeparam(operand, pattern, errorf, false, opt);

    case TK_ARROW:
      return is_arrow_match_x(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(operand);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;
      matchtype_t ok = is_x_match_base_typeparam(unfolded, pattern, errorf,
        report_reject, opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_REJECT;

    default: {}
  }

  pony_assert(0);
  return MATCHTYPE_DENY_CAP;
}

static matchtype_t is_x_match_typeparam(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  matchtype_t ok = is_x_match_base_typeparam(operand, pattern, errorf,
    report_reject, opt);

  if(ok != MATCHTYPE_REJECT)
    return ok;

  ast_t* pattern_upper = typeparam_upper(pattern);

  // An unconstrained typeparam can match anything.
  if(pattern_upper == NULL)
    return MATCHTYPE_ACCEPT;

  // Otherwise, match the constraint.
  ok = is_x_match_x(operand, pattern_upper, errorf, report_reject, opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_arrow(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  // T1 match upperbound(T2->T3)
  // ---
  // T1 match T2->T3
  ast_t* pattern_upper = viewpoint_upper(pattern, opt);

  if(pattern_upper == NULL)
  {
    if((errorf != NULL) && report_reject)
    {
      ast_error_frame(errorf, pattern,
        "%s cannot match %s: the pattern type has no upper bounds",
        ast_print_type(operand, opt->strtab), ast_print_type(pattern, opt->strtab));
    }

    return MATCHTYPE_REJECT;
  }

  matchtype_t ok = is_x_match_x(operand, pattern_upper, errorf, report_reject,
    opt);
  ast_free_unattached(pattern_upper);
  return ok;
}

static matchtype_t is_x_match_x(ast_t* operand, ast_t* pattern,
  errorframe_t* errorf, bool report_reject, pass_opt_t* opt)
{
  if(ast_id(pattern) == TK_DONTCARETYPE)
    return MATCHTYPE_ACCEPT;

  switch(ast_id(pattern))
  {
    case TK_UNIONTYPE:
      return is_x_match_union(operand, pattern, errorf, report_reject, opt);

    case TK_ISECTTYPE:
      return is_x_match_isect(operand, pattern, errorf, report_reject, opt);

    case TK_TUPLETYPE:
      return is_x_match_tuple(operand, pattern, errorf, report_reject, opt);

    case TK_NOMINAL:
      return is_x_match_nominal(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEPARAMREF:
      return is_x_match_typeparam(operand, pattern, errorf, report_reject,
        opt);

    case TK_ARROW:
      return is_x_match_arrow(operand, pattern, errorf, report_reject, opt);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(pattern);
      if(unfolded == NULL)
        return MATCHTYPE_REJECT;

      matchtype_t ok = is_x_match_x(operand, unfolded, errorf, report_reject,
        opt);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
      return MATCHTYPE_DENY_CAP;

    default:
      pony_assert(0);
      return MATCHTYPE_DENY_CAP;
  }
}

matchtype_t is_matchtype(ast_t* operand, ast_t* pattern, errorframe_t* errorf,
  pass_opt_t* opt)
{
  return is_x_match_x(operand, pattern, errorf, true, opt);
}

matchtype_t is_matchtype_with_consumed_pattern(ast_t* operand, ast_t* pattern, errorframe_t* errorf,
  pass_opt_t* opt)
{
  ast_t* consumed_pattern = consume_type(pattern, TK_NONE, false, opt);
  if (consumed_pattern == NULL)
    return MATCHTYPE_REJECT;

  matchtype_t rslt = is_x_match_x(operand, consumed_pattern, errorf, true, opt);

  // TODO discuss with joe
  if (consumed_pattern != pattern)
    ast_free_unattached(consumed_pattern);

  return rslt;
}
