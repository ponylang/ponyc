#include "compattype.h"
#include "cap.h"
#include "viewpoint.h"
#include <assert.h>

static bool is_nominal_compat_nominal(ast_t* a, ast_t* b)
{
  // k1 ~ k2
  // ---
  // N1 k1 ~ N2 k2
  AST_GET_CHILDREN(a, a_pkg, a_id, a_typeparams, a_cap, a_eph);
  AST_GET_CHILDREN(b, b_pkg, b_id, b_typeparams, b_cap, b_eph);

  return is_cap_compat_cap(ast_id(a_cap), ast_id(a_eph),
    ast_id(b_cap), ast_id(b_eph));
}

static bool is_nominal_compat_typeparam(ast_t* a, ast_t* b)
{
  // k1 ~ k2
  // ---
  // N1 k1 ~ A2 k2
  AST_GET_CHILDREN(a, a_pkg, a_id, a_typeparams, a_cap, a_eph);
  AST_GET_CHILDREN(b, b_id, b_cap, b_eph);

  return is_cap_compat_cap(ast_id(a_cap), ast_id(a_eph),
    ast_id(b_cap), ast_id(b_eph));
}

static bool is_typeparam_compat_typeparam(ast_t* a, ast_t* b)
{
  // k1 ~ k2
  // ---
  // A1 k1 ~ A2 k2
  AST_GET_CHILDREN(a, a_id, a_cap, a_eph);
  AST_GET_CHILDREN(b, b_id, b_cap, b_eph);

  return is_cap_compat_cap(ast_id(a_cap), ast_id(a_eph),
    ast_id(b_cap), ast_id(b_eph));
}

static bool is_arrow_compat_nominal(ast_t* a, ast_t* b)
{
  // lowerbound(T1->T2) ~ N k
  // ---
  // T1->T2 ~ N k
  ast_t* a_lower = viewpoint_lower(a);

  if(a == NULL)
    return false;

  bool ok = is_compat_type(a_lower, b);
  ast_free_unattached(a_lower);
  return ok;
}

static bool is_arrow_compat_typeparam(ast_t* a, ast_t* b)
{
  // forall k' in k . T1->T2 {A k |-> A k'} ~ A k'
  // ---
  // T1->T2 ~ A k
  ast_t* r_a = viewpoint_reifytypeparam(a, b);
  ast_t* r_b = viewpoint_reifytypeparam(b, b);

  if(r_a != NULL)
  {
    bool ok = is_compat_type(r_a, r_b);
    ast_free_unattached(r_a);
    ast_free_unattached(r_b);
    return ok;
  }

  // lowerbound(T1->T2) ~ A k
  // ---
  // T1->T2 ~ A k
  return is_arrow_compat_nominal(a, b);
}

static bool is_arrow_compat_arrow(ast_t* a, ast_t* b)
{
  // S = this | A {#read, #send, #share, #any}
  // K = N k | A {iso, trn, ref, val, box, tag} | K->K | (empty)
  // L = S | K
  // T = N k | A k | L->T
  //
  // forall K' in S . K->S->T1 {S |-> K'} ~ T2 {S |-> K'}
  // ---
  // K->S->T1 ~ T2
  ast_t* r_a;
  ast_t* r_b;

  if(viewpoint_reifypair(a, b, &r_a, &r_b))
  {
    bool ok = is_compat_type(r_a, r_b);
    ast_free_unattached(r_a);
    ast_free_unattached(r_b);
    return ok;
  }

  // No elements need reification.
  //
  // lowerbound(T1->T2) ~ lowerbound(T3->T4)
  // ---
  // T1->T2 ~ T3->T4
  r_a = viewpoint_lower(a);

  if(r_a == NULL)
    return false;

  r_b = viewpoint_lower(b);

  if(r_b == NULL)
  {
    ast_free_unattached(r_a);
    return false;
  }

  bool ok = is_compat_type(r_a, r_b);
  ast_free_unattached(r_a);
  ast_free_unattached(r_b);
  return ok;
}

static bool is_union_compat_x(ast_t* a, ast_t* b)
{
  // T1 ~ T3 or T2 ~ T3
  // ---
  // (T1 | T2) ~ T3
  for(ast_t* child = ast_child(a);
    child != NULL;
    child = ast_sibling(child))
  {
    if(is_compat_type(child, b))
      return true;
  }

  return false;
}

static bool is_isect_compat_x(ast_t* a, ast_t* b)
{
  // T1 ~ T3
  // T2 ~ T3
  // ---
  // (T1 & T2) ~ T3
  for(ast_t* child = ast_child(a);
    child != NULL;
    child = ast_sibling(child))
  {
    if(!is_compat_type(child, b))
      return false;
  }

  return true;
}

static bool is_tuple_compat_tuple(ast_t* a, ast_t* b)
{
  // T1 ~ T3
  // T2 ~ T4
  // ---
  // (T1, T2) ~ (T3, T4)
  ast_t* a_child = ast_child(a);
  ast_t* b_child = ast_child(b);

  while((a_child != NULL) && (b_child != NULL))
  {
    if(!is_compat_type(a_child, b_child))
      return false;

    a_child = ast_sibling(a_child);
    b_child = ast_sibling(b_child);
  }

  return (a_child == NULL) && (b_child == NULL);
}

static bool is_tuple_compat_x(ast_t* a, ast_t* b)
{
  switch(ast_id(b))
  {
    case TK_UNIONTYPE:
      return is_union_compat_x(b, a);

    case TK_ISECTTYPE:
      return is_isect_compat_x(b, a);

    case TK_TUPLETYPE:
      return is_tuple_compat_tuple(b, a);

    case TK_NOMINAL:
      return false;

    case TK_TYPEPARAMREF:
      return false;

    case TK_ARROW:
      return false;

    default: {}
  }

  assert(0);
  return false;
}

static bool is_nominal_compat_x(ast_t* a, ast_t* b)
{
  switch(ast_id(b))
  {
    case TK_UNIONTYPE:
      return is_union_compat_x(b, a);

    case TK_ISECTTYPE:
      return is_isect_compat_x(b, a);

    case TK_TUPLETYPE:
      return is_tuple_compat_x(b, a);

    case TK_NOMINAL:
      return is_nominal_compat_nominal(a, b);

    case TK_TYPEPARAMREF:
      return is_nominal_compat_typeparam(a, b);

    case TK_ARROW:
      return is_arrow_compat_nominal(b, a);

    default: {}
  }

  assert(0);
  return false;
}

static bool is_typeparam_compat_x(ast_t* a, ast_t* b)
{
  switch(ast_id(b))
  {
    case TK_UNIONTYPE:
      return is_union_compat_x(b, a);

    case TK_ISECTTYPE:
      return is_isect_compat_x(b, a);

    case TK_TUPLETYPE:
      return is_tuple_compat_x(b, a);

    case TK_NOMINAL:
      return is_nominal_compat_typeparam(b, a);

    case TK_TYPEPARAMREF:
      return is_typeparam_compat_typeparam(a, b);

    case TK_ARROW:
      return is_arrow_compat_typeparam(b, a);

    default: {}
  }

  assert(0);
  return false;
}

static bool is_arrow_compat_x(ast_t* a, ast_t* b)
{
  switch(ast_id(b))
  {
    case TK_UNIONTYPE:
      return is_union_compat_x(b, a);

    case TK_ISECTTYPE:
      return is_isect_compat_x(b, a);

    case TK_TUPLETYPE:
      return is_tuple_compat_x(b, a);

    case TK_NOMINAL:
      return is_arrow_compat_nominal(a, b);

    case TK_TYPEPARAMREF:
      return is_arrow_compat_typeparam(a, b);

    case TK_ARROW:
      return is_arrow_compat_arrow(a, b);

    default: {}
  }

  assert(0);
  return false;
}

bool is_compat_type(ast_t* a, ast_t* b)
{
  switch(ast_id(a))
  {
    case TK_UNIONTYPE:
      return is_union_compat_x(a, b);

    case TK_ISECTTYPE:
      return is_isect_compat_x(a, b);

    case TK_TUPLETYPE:
      return is_tuple_compat_x(a, b);

    case TK_NOMINAL:
      return is_nominal_compat_x(a, b);

    case TK_TYPEPARAMREF:
      return is_typeparam_compat_x(a, b);

    case TK_ARROW:
      return is_arrow_compat_x(a, b);

    default: {}
  }

  assert(0);
  return false;
}
