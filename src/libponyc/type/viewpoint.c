#include "viewpoint.h"
#include "alias.h"
#include "assemble.h"
#include "cap.h"
#include "../ast/astbuild.h"
#include "ponyassert.h"

enum
{
  VIEW_UPPER_NO,
  VIEW_UPPER_YES,
  VIEW_UPPER_FORCE
};

ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type)
{
  int upper = VIEW_UPPER_NO;

  switch(ast_id(r_type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Adapt each element.
      ast_t* type = ast_from(r_type, ast_id(r_type));
      ast_t* child = ast_child(r_type);

      while(child != NULL)
      {
        ast_append(type, viewpoint_type(l_type, child));
        child = ast_sibling(child);
      }

      return type;
    }

    case TK_TYPEPARAMREF:
      upper = VIEW_UPPER_NO;
      break;

    case TK_NOMINAL:
    {
      ast_t* cap = cap_fetch(r_type);

      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
        case TK_BOX:
          // A known refcap on the right can be compacted.
          upper = VIEW_UPPER_YES;
          break;

        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SHARE:
          // No refcap on the left modifies these.
          upper = VIEW_UPPER_FORCE;
          break;

        default: {}
      }
      break;
    }

    case TK_ARROW:
      break;

    default:
      pony_assert(0);
      return NULL;
  }

  switch(ast_id(l_type))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* cap = cap_fetch(l_type);

      switch(ast_id(cap))
      {
        case TK_REF:
          // ref->T = T
          return ast_dup(r_type);

        case TK_CAP_SEND:
        case TK_CAP_SHARE:
        case TK_CAP_READ:
        case TK_CAP_ALIAS:
        case TK_CAP_ANY:
          // Don't compact through an unknown refcap.
          if(upper == VIEW_UPPER_YES)
            upper = VIEW_UPPER_NO;
          break;

        default: {}
      }
      break;
    }

    case TK_THISTYPE:
      if(upper == VIEW_UPPER_YES)
        upper = VIEW_UPPER_NO;
      break;

    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      break;

    case TK_ARROW:
    {
      // (T1->T2)->T3 --> T1->(T2->T3)
      AST_GET_CHILDREN(l_type, left, right);
      ast_t* r_right = viewpoint_type(right, r_type);
      return viewpoint_type(left, r_right);
    }

    default:
      pony_assert(0);
      return NULL;
  }

  BUILD(arrow, l_type,
    NODE(TK_ARROW, TREE(ast_dup(l_type)) TREE(ast_dup(r_type))));

  if(upper != VIEW_UPPER_NO)
  {
    ast_t* arrow_upper = viewpoint_upper(arrow);

    if(arrow_upper == NULL)
      return arrow;

    if(arrow != arrow_upper)
    {
      ast_free_unattached(arrow);
      arrow = arrow_upper;
    }
  }

  return arrow;
}

ast_t* viewpoint_upper(ast_t* type)
{
  // T = N | A
  // s = {k}
  // upper(s p'->T k p) = isect[k' in s](T (k'->k) eph(s, p', p))
  // eph(s, p', p) = { unalias(p) if p' = ^, forall k in s . k in {iso, trn}
  //                 { p          otherwise
  pony_assert(ast_id(type) == TK_ARROW);
  AST_GET_CHILDREN(type, left, right);
  ast_t* r_right = right;

  switch(ast_id(right))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      break;

    case TK_ARROW:
      // Arrow types are right associative.
      r_right = viewpoint_upper(right);

      if(r_right == NULL)
        return NULL;
      break;

    default:
      pony_assert(0);
      return NULL;
  }

  token_id l_cap = TK_NONE;
  token_id l_eph = TK_NONE;

  switch(ast_id(left))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      l_cap = ast_id(left);
      break;

    case TK_THISTYPE:
      l_cap = TK_BOX;
      break;

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* left_cap = cap_fetch(left);
      ast_t* left_eph = ast_sibling(left_cap);

      l_cap = ast_id(left_cap);
      l_eph = ast_id(left_eph);
      break;
    }

    default:
      pony_assert(0);
      return NULL;
  }

  ast_t* right_cap = cap_fetch(r_right);
  ast_t* right_eph = ast_sibling(right_cap);

  token_id r_cap = ast_id(right_cap);
  token_id r_eph = ast_id(right_eph);

  // No result: left side could be a tag.
  if(!cap_view_upper(l_cap, l_eph, &r_cap, &r_eph))
    return NULL;

  ast_t* rr_right = set_cap_and_ephemeral(r_right, r_cap, r_eph);

  if(r_right != right)
    ast_free_unattached(r_right);

  return rr_right;
}

ast_t* viewpoint_lower(ast_t* type)
{
  // T = N | A
  // s = {k}
  // upper(s p'->T k p) = union[k' in s](T (k'->k) eph(s, p', p))
  // eph(s, p', p) = { unalias(p) if p' = ^, exists k in s . k in {iso, trn}
  //                 { p          otherwise
  pony_assert(ast_id(type) == TK_ARROW);
  AST_GET_CHILDREN(type, left, right);
  ast_t* r_right = right;

  switch(ast_id(right))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      break;

    case TK_ARROW:
      // Arrow types are right associative.
      r_right = viewpoint_lower(right);

      if(r_right == NULL)
        return NULL;
      break;

    default:
      pony_assert(0);
      return NULL;
  }

  token_id l_cap = TK_NONE;
  token_id l_eph = TK_NONE;

  switch(ast_id(left))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      l_cap = ast_id(left);
      break;

    case TK_THISTYPE:
      l_cap = TK_CAP_READ;
      break;

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* left_cap = cap_fetch(left);
      ast_t* left_eph = ast_sibling(left_cap);

      l_cap = ast_id(left_cap);
      l_eph = ast_id(left_eph);
      break;
    }

    default:
      pony_assert(0);
      return NULL;
  }

  ast_t* right_cap = cap_fetch(r_right);
  ast_t* right_eph = ast_sibling(right_cap);

  token_id r_cap = ast_id(right_cap);
  token_id r_eph = ast_id(right_eph);

  // No result: left side could be a tag.
  if(!cap_view_lower(l_cap, l_eph, &r_cap, &r_eph))
    return NULL;

  ast_t* rr_right = set_cap_and_ephemeral(r_right, r_cap, r_eph);

  if(r_right != right)
    ast_free_unattached(r_right);

  return rr_right;
}

static void replace_type(ast_t** astp, ast_t* target, ast_t* with)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    replace_type(&child, target, with);
    child = ast_sibling(child);
  }

  ast_t* node_type = ast_type(ast);

  if(node_type != NULL)
    replace_type(&node_type, target, with);

  if(ast_id(ast) == ast_id(target))
  {
    switch(ast_id(target))
    {
      case TK_THISTYPE:
        // Replace `this`.
        ast_replace(astp, ast_dup(with));
        break;

      case TK_TYPEPARAMREF:
      {
        // Replace a typeparamref with a reification.
        ast_t* target_def = (ast_t*)ast_data(target);
        ast_t* left_def = (ast_t*)ast_data(ast);

        if(target_def == left_def)
        {
          AST_GET_CHILDREN(ast, id, cap, eph);
          ast_t* a_with = with;

          switch(ast_id(eph))
          {
            case TK_EPHEMERAL:
            {
              ast_t* c_with = consume_type(with, TK_NONE, true);
              if (c_with != NULL)
              {
                a_with = c_with;
              }
              break;
            }

            case TK_ALIASED:
              a_with = alias(with);
              break;

            default: {}
          }

          if(a_with == with)
            a_with = ast_dup(with);

          ast_replace(astp, a_with);
        }
        break;
      }

      default:
        pony_assert(0);
    }
  } else if(ast_id(ast) == TK_ARROW) {
    // Recalculate all arrow types.
    AST_GET_CHILDREN(ast, left, right);
    ast_t* r_type = viewpoint_type(left, right);
    ast_replace(astp, r_type);
  }
}

ast_t* viewpoint_replace(ast_t* ast, ast_t* target, ast_t* with, bool duplicate)
{
  // Target is thistype or a typeparamref. With is a type (when replacing
  // `this` in a reified method signature) or a single capability (when
  // typechecking arrow types).
  pony_assert(
    (ast_id(target) == TK_THISTYPE) ||
    (ast_id(target) == TK_TYPEPARAMREF));

  ast_t* r_ast;

  if(duplicate)
    r_ast = ast_dup(ast);
  else
    r_ast = ast;

  replace_type(&r_ast, target, with);
  return r_ast;
}

ast_t* viewpoint_replacethis(ast_t* ast, ast_t* with, bool duplicate)
{
  ast_t* thistype = ast_from(ast, TK_THISTYPE);
  ast_t* r_ast = viewpoint_replace(ast, thistype, with, duplicate);
  ast_free_unattached(thistype);
  return r_ast;
}

static void replace_typeparam(ast_t* tuple, ast_t* type, ast_t* typeparamref,
  token_id cap, token_id eph)
{
  ast_t* r_tp = set_cap_and_ephemeral(typeparamref, cap, eph);
  ast_t* r_type = viewpoint_replace(type, typeparamref, r_tp, true);
  ast_append(tuple, r_type);
}

// Find the first typeparamref in ast that refers to the same type parameter
// definition as target. Returns NULL if not found.
static ast_t* find_typeparamref_in(ast_t* ast, ast_t* target)
{
  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    if(ast_data(ast) == ast_data(target))
      return ast;
    return NULL;
  }

  ast_t* child = ast_child(ast);
  while(child != NULL)
  {
    ast_t* found = find_typeparamref_in(child, target);
    if(found != NULL)
      return found;
    child = ast_sibling(child);
  }
  return NULL;
}

ast_t* viewpoint_reifytypeparam(ast_t* type, ast_t* typeparamref)
{
  pony_assert(ast_id(typeparamref) == TK_TYPEPARAMREF);

  // When type contains the same type parameter, use the version found in
  // type to get the correct ephemeral marker. The typeparamref argument may
  // come from a different context (e.g., the other side of a subtype check)
  // and carry a different ephemeral marker, which would produce an incorrect
  // reification. Only override when the cap constraint matches to avoid
  // changing the expansion basis. See #1798.
  ast_t* local_tp = find_typeparamref_in(type, typeparamref);
  if((local_tp != NULL) &&
    (ast_id(ast_childidx(local_tp, 1)) == ast_id(ast_childidx(typeparamref, 1))))
    typeparamref = local_tp;

  AST_GET_CHILDREN(typeparamref, id, cap, eph);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      return NULL;

    case TK_CAP_SEND:
    {
      ast_t* tuple = ast_from(type, TK_TUPLETYPE);
      replace_typeparam(tuple, type, typeparamref, TK_ISO, ast_id(eph));
      replace_typeparam(tuple, type, typeparamref, TK_VAL, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_TAG, TK_NONE);
      return tuple;
    }

    case TK_CAP_SHARE:
    {
      ast_t* tuple = ast_from(type, TK_TUPLETYPE);
      replace_typeparam(tuple, type, typeparamref, TK_VAL, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_TAG, TK_NONE);
      return tuple;
    }

    case TK_CAP_READ:
    {
      ast_t* tuple = ast_from(type, TK_TUPLETYPE);
      replace_typeparam(tuple, type, typeparamref, TK_REF, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_VAL, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_BOX, TK_NONE);
      return tuple;
    }

    case TK_CAP_ALIAS:
    {
      ast_t* tuple = ast_from(type, TK_TUPLETYPE);
      replace_typeparam(tuple, type, typeparamref, TK_REF, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_VAL, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_BOX, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_TAG, TK_NONE);
      return tuple;
    }

    case TK_CAP_ANY:
    {
      ast_t* tuple = ast_from(type, TK_TUPLETYPE);
      replace_typeparam(tuple, type, typeparamref, TK_ISO, ast_id(eph));
      replace_typeparam(tuple, type, typeparamref, TK_TRN, ast_id(eph));
      replace_typeparam(tuple, type, typeparamref, TK_REF, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_VAL, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_BOX, TK_NONE);
      replace_typeparam(tuple, type, typeparamref, TK_TAG, TK_NONE);
      return tuple;
    }

    default: {}
  }

  pony_assert(0);
  return NULL;
}

ast_t* viewpoint_reifythis(ast_t* type)
{
  ast_t* tuple = ast_from(type, TK_TUPLETYPE);

  ast_t* this_ref = ast_from(type, TK_REF);
  ast_append(tuple, viewpoint_replacethis(type, this_ref, true));
  ast_free_unattached(this_ref);

  ast_t* this_val = ast_from(type, TK_VAL);
  ast_append(tuple, viewpoint_replacethis(type, this_val, true));
  ast_free_unattached(this_val);

  ast_t* this_box = ast_from(type, TK_BOX);
  ast_append(tuple, viewpoint_replacethis(type, this_box, true));
  ast_free_unattached(this_box);

  return tuple;
}

bool viewpoint_reifypair(ast_t* a, ast_t* b, ast_t** r_a, ast_t** r_b)
{
  pony_assert(ast_id(a) == TK_ARROW);
  pony_assert(ast_id(b) == TK_ARROW);

  // Walk both arrows in parallel to find the first element that needs
  // reification. Each side must use its own typeparamref so the correct
  // ephemeral marker is used for each reification. Using a's typeparamref
  // for b would apply a's alias/ephemeral semantics to b's expansion,
  // which is unsound when the markers differ (see #1798).
  ast_t* test_a = a;
  ast_t* test_b = b;

  while(ast_id(test_a) == TK_ARROW)
  {
    AST_GET_CHILDREN(test_a, left_a, right_a);

    switch(ast_id(left_a))
    {
      case TK_THISTYPE:
      {
        // Reify on both sides.
        *r_a = viewpoint_reifythis(a);
        *r_b = viewpoint_reifythis(b);
        return true;
      }

      case TK_TYPEPARAMREF:
      {
        // If we can reify a, we can reify b.
        ast_t* r = viewpoint_reifytypeparam(a, left_a);

        if(r == NULL)
          break;

        *r_a = r;

        // Use b's own typeparamref at the corresponding position if it
        // refers to the same type parameter.
        ast_t* left_b = left_a;
        if(ast_id(test_b) == TK_ARROW)
        {
          ast_t* candidate = ast_child(test_b);
          if((ast_id(candidate) == TK_TYPEPARAMREF) &&
            (ast_data(candidate) == ast_data(left_a)))
            left_b = candidate;
        }

        *r_b = viewpoint_reifytypeparam(b, left_b);
        return true;
      }

      default: {}
    }

    test_a = right_a;
    if(ast_id(test_b) == TK_ARROW)
      test_b = ast_childidx(test_b, 1);
  }

  if(ast_id(test_a) == TK_TYPEPARAMREF)
  {
    ast_t* r = viewpoint_reifytypeparam(a, test_a);

    if(r == NULL)
      return false;

    *r_a = r;

    // Use b's own typeparamref at the corresponding position if it
    // refers to the same type parameter.
    ast_t* tp_b = test_a;
    if((ast_id(test_b) == TK_TYPEPARAMREF) &&
      (ast_data(test_b) == ast_data(test_a)))
      tp_b = test_b;

    *r_b = viewpoint_reifytypeparam(b, tp_b);
    return true;
  }

  return false;
}
