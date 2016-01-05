#include "viewpoint.h"
#include "assemble.h"
#include "cap.h"
#include "../ast/astbuild.h"
#include <assert.h>

ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type)
{
  bool upper = false;

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

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* cap = cap_fetch(r_type);

      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
        case TK_BOX:
        case TK_ISO_BIND:
        case TK_TRN_BIND:
        case TK_REF_BIND:
        case TK_BOX_BIND:
          // A known refcap on the right can be compacted.
          upper = true;
          break;

        case TK_VAL:
        case TK_TAG:
        case TK_CAP_SHARE:
        case TK_VAL_BIND:
        case TK_TAG_BIND:
        case TK_CAP_SHARE_BIND:
          // No refcap on the left modifies these.
          return ast_dup(r_type);

        default: {}
      }
      break;
    }

    case TK_ARROW:
      break;

    default:
      assert(0);
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
        case TK_REF_BIND:
          // ref->T = T
          return ast_dup(r_type);

        case TK_CAP_SEND:
        case TK_CAP_SHARE:
        case TK_CAP_READ:
        case TK_CAP_ANY:
        case TK_CAP_SEND_BIND:
        case TK_CAP_SHARE_BIND:
        case TK_CAP_READ_BIND:
        case TK_CAP_ANY_BIND:
          // Don't compact through an unknown refcap.
          upper = false;
          break;

        default: {}
      }
      break;
    }

    case TK_THISTYPE:
      upper = false;
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
      assert(0);
      return NULL;
  }

  BUILD(arrow, l_type,
    NODE(TK_ARROW, TREE(ast_dup(l_type)) TREE(ast_dup(r_type))));

  if(upper)
  {
    ast_t* arrow_upper = viewpoint_upper(arrow);
    assert(arrow_upper != NULL);

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
  assert(ast_id(type) == TK_ARROW);
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
      assert(0);
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
      assert(0);
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
  assert(ast_id(type) == TK_ARROW);
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
      assert(0);
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
      assert(0);
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
          ast_replace(astp, ast_dup(with));
        break;
      }

      default:
        assert(0);
    }
  } else if(ast_id(ast) == TK_ARROW) {
    // Recalculate all arrow types.
    AST_GET_CHILDREN(ast, left, right);
    ast_t* r_type = viewpoint_type(left, right);
    ast_replace(astp, r_type);
  }
}

ast_t* viewpoint_replace(ast_t* ast, ast_t* target, ast_t* with)
{
  // Target is thistype or a typeparamref. With is a type (when replacing
  // `this` in a reified method signature) or a single capability (when
  // typechecking arrow types).
  assert(
    (ast_id(target) == TK_THISTYPE) ||
    (ast_id(target) == TK_TYPEPARAMREF));

  ast_t* r_ast = ast_dup(ast);
  replace_type(&r_ast, target, with);
  return r_ast;
}

ast_t* viewpoint_replacethis(ast_t* ast, ast_t* with)
{
  ast_t* thistype = ast_from(ast, TK_THISTYPE);
  ast_t* r_ast = viewpoint_replace(ast, thistype, with);
  ast_free_unattached(thistype);
  return r_ast;
}

static void replace_typeparam(ast_t* tuple, ast_t* type, ast_t* typeparamref,
  token_id cap, token_id eph)
{
  ast_t* r_tp = set_cap_and_ephemeral(typeparamref, cap, eph);
  ast_t* r_type = viewpoint_replace(type, typeparamref, r_tp);
  ast_append(tuple, r_type);
}

ast_t* viewpoint_reifytypeparam(ast_t* type, ast_t* typeparamref)
{
  assert(ast_id(typeparamref) == TK_TYPEPARAMREF);
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

  assert(0);
  return NULL;
}

ast_t* viewpoint_reifythis(ast_t* type)
{
  ast_t* tuple = ast_from(type, TK_TUPLETYPE);

  ast_t* this_ref = ast_from(type, TK_REF);
  ast_append(tuple, viewpoint_replacethis(type, this_ref));
  ast_free_unattached(this_ref);

  ast_t* this_val = ast_from(type, TK_VAL);
  ast_append(tuple, viewpoint_replacethis(type, this_val));
  ast_free_unattached(this_val);

  ast_t* this_box = ast_from(type, TK_BOX);
  ast_append(tuple, viewpoint_replacethis(type, this_box));
  ast_free_unattached(this_box);

  return tuple;
}
