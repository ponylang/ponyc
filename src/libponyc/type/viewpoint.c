#include "viewpoint.h"
#include "assemble.h"
#include "cap.h"
#include <assert.h>

static ast_t* make_single_arrow(ast_t* left, ast_t* right)
{
  switch(ast_id(left))
  {
    case TK_ARROW:
    {
      ast_t* arrow = ast_dup(left);
      ast_t* child = ast_childidx(arrow, 1);

      // Arrow is right associative.
      while(ast_id(child) == TK_ARROW)
        child = ast_childidx(child, 1);

      ast_t* view = viewpoint_type(child, right);

      if(view == NULL)
      {
        ast_free_unattached(arrow);
        return NULL;
      }

      ast_replace(&child, view);
      return arrow;
    }

    default: {}
  }

  ast_t* arrow = ast_from(left, TK_ARROW);
  ast_add(arrow, right);
  ast_add(arrow, left);
  return arrow;
}

static ast_t* make_arrow_type(ast_t* left, ast_t* right)
{
  switch(ast_id(right))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* type = ast_from(right, ast_id(right));
      ast_t* child = ast_child(right);

      while(child != NULL)
      {
        ast_append(type, make_arrow_type(left, child));
        child = ast_sibling(child);
      }

      return type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      switch(cap_single(right))
      {
        case TK_VAL:
        case TK_TAG:
          return right;

        default: {}
      }

      return make_single_arrow(left, right);
    }

    case TK_ARROW:
      return make_single_arrow(left, right);

    default: {}
  }

  assert(0);
  return NULL;
}

static ast_t* viewpoint_lower_for_nominal(ast_t* type)
{
  ast_t* cap = ast_childidx(type, 3);
  token_id tcap = ast_id(cap);

  // For any chain of arrows, return a capability that is a subtype of the
  // resulting capability.
  // ref->iso = iso, val->iso = val, box->iso = tag => iso
  // ref->trn = trn, val->trn = val, box->trn = box => trn
  // ref->ref = ref, val->ref = val, box->ref = box => trn
  // ref->val = val, val->val = val, box->val = val => val
  // ref->box = box, val->box = val, box->box = box => val
  // ref->tag = tag, val->tag = tag, box->tag = tag => tag
  switch(tcap)
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
    case TK_TAG:
      return type;

    case TK_REF:
      tcap = TK_TRN;
      break;

    case TK_BOX:
      tcap = TK_VAL;
      break;

    default:
      assert(0);
      return NULL;
  }

  type = ast_dup(type);
  cap = ast_childidx(type, 3);
  ast_setid(cap, tcap);
  return type;
}

static ast_t* viewpoint_lower_for_typeparam(ast_t* type)
{
  AST_GET_CHILDREN(type, id, cap, eph);
  token_id tcap = ast_id(cap);

  // What's a subtype of every possible result?
  // ref->#read = trn, val->#read = val, box->#read = val => trn
  // ref->#share = val, val->#share = val, box->#share = val => val
  // ref->#any = iso, val->#any = val, box->#any = val => iso
  switch(tcap)
  {
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      return type;

    case TK_CAP_READ:
      tcap = TK_TRN;
      break;

    case TK_CAP_SHARE:
      tcap = TK_VAL;
      break;

    case TK_CAP_SEND:
    case TK_CAP_ANY:
      tcap = TK_ISO;
      break;

    default:
      assert(0);
      return NULL;
  }

  type = ast_dup(type);
  cap = ast_childidx(type, 1);
  ast_setid(cap, tcap);
  return type;
}

static ast_t* viewpoint_for_type(token_id view, token_id eph, ast_t* type,
  int cap_index)
{
  ast_t* cap = ast_childidx(type, cap_index);
  token_id tcap = ast_id(cap);
  token_id rcap = cap_viewpoint(view, tcap);

  if(rcap == TK_NONE)
    return NULL;

  if((tcap != rcap) || (eph == TK_EPHEMERAL))
  {
    type = ast_dup(type);
    cap = ast_childidx(type, cap_index);
    ast_setid(cap, rcap);

    if(eph == TK_EPHEMERAL && ((view == TK_ISO) || (view == TK_TRN)))
    {
      // If we're adapting from an ephemeral type, make this type ephemeral.
      // iso^->iso = iso^, previous views were iso, alias(iso^) ~ alias(iso)
      // iso^->trn = trn^, previous views were tag, alias(trn^) ~ alias(tag)
      // trn^->iso = iso^, previous views were iso, alias(iso^) ~ alias(iso)
      // trn^->trn = trn^, previous views were trn, alias(trn^) ~ alias(trn)
      // alias(iso)->x isn't alllowed
      // alias(trn)->x = box->x
      // alias(iso^) ~ alias(box->iso)
      // alias(trn^) ~ alias(box->trn)
      // Doesn't work for ref, isn't needed for val, box or tag.
      ast_t* ephemeral = ast_sibling(cap);
      ast_setid(ephemeral, eph);
    }
  }

  return type;
}

ast_t* viewpoint_cap(token_id cap, token_id eph, ast_t* type)
{
  if(cap == TK_TAG)
    return NULL;

  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Adapt all elements.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, viewpoint_cap(cap, eph, child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return viewpoint_for_type(cap, eph, type, 3);

    case TK_TYPEPARAMREF:
      return viewpoint_for_type(cap, eph, type, 1);

    case TK_ARROW:
    {
      // Adapt the lower bounds.
      ast_t* lower = viewpoint_lower(type);
      ast_t* r_type = viewpoint_cap(cap, eph, lower);

      if(r_type != lower)
        ast_free_unattached(lower);

      return r_type;
    }

    default: {}
  }

  assert(0);
  return NULL;
}

ast_t* viewpoint_type(ast_t* l_type, ast_t* r_type)
{
  switch(ast_id(l_type))
  {
    case TK_TUPLETYPE:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // Only occurs in codegen.
      return r_type;
    }

    case TK_NOMINAL:
    {
      if(ast_id(r_type) == TK_ARROW)
        return make_arrow_type(l_type, r_type);

      AST_GET_CHILDREN(l_type, pkg, id, typeargs, cap, eph);
      return viewpoint_cap(ast_id(cap), ast_id(eph), r_type);
    }

    case TK_TYPEPARAMREF:
    {
      if(ast_id(r_type) == TK_ARROW)
        return make_arrow_type(l_type, r_type);

      // If the left side is a type parameter, return an arrow type if the
      // capability is #read, otherwise adapt the type.
      AST_GET_CHILDREN(l_type, id, cap, eph);
      token_id tcap = ast_id(cap);

      if(tcap == TK_CAP_READ)
        return make_arrow_type(l_type, r_type);

      return viewpoint_cap(tcap, ast_id(eph), r_type);
    }

    case TK_ARROW:
    case TK_BOXTYPE:
    case TK_THISTYPE:
      return make_arrow_type(l_type, r_type);

    default: {}
  }

  return NULL;
}

ast_t* viewpoint_upper(ast_t* type)
{
  if(ast_id(type) == TK_ARROW)
  {
    AST_GET_CHILDREN(type, left, right);
    ast_t* r_right = viewpoint_upper(right);
    return viewpoint_cap(TK_BOX, TK_NONE, r_right);
  }

  return type;
}

ast_t* viewpoint_lower(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_TUPLETYPE:
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      // Adapt all elements.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, viewpoint_lower(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
      return viewpoint_lower_for_nominal(type);

    case TK_TYPEPARAMREF:
      return viewpoint_lower_for_typeparam(type);

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);

      // If left it a boxtype, right's lower bounds is its actual type.
      if(ast_id(left) == TK_BOXTYPE)
        return viewpoint_cap(TK_BOX, TK_NONE, right);

      // If left is a thistype or a typeparamref type, right's lower bounds is
      // calculated on its own.
      return viewpoint_lower(right);
    }

    default: {}
  }

  return NULL;
}

void replace_thistype(ast_t** astp, ast_t* type)
{
  ast_t* ast = *astp;

  if(ast_id(ast) == TK_THISTYPE)
  {
    ast_replace(astp, type);
    return;
  }

  ast_t* node_type = ast_type(ast);

  if(node_type != NULL)
    replace_thistype(&node_type, type);

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    replace_thistype(&child, type);
    child = ast_sibling(child);
  }
}

bool flatten_arrows(ast_t** astp, bool errors)
{
  ast_t* ast = *astp;
  ast_t* node_type = ast_type(ast);

  if(node_type != NULL)
  {
    if(!flatten_arrows(&node_type, errors))
      return false;
  }

  if(ast_id(ast) == TK_ARROW)
  {
    AST_GET_CHILDREN(ast, left, right);
    ast_t* flat;

    if(!flatten_arrows(&right, errors))
      return false;

    if((ast_id(left) == TK_BOXTYPE) && (ast_id(right) != TK_ARROW))
      flat = viewpoint_cap(TK_BOX, TK_NONE, right);
    else
      flat = viewpoint_type(left, right);

    if(flat == NULL)
    {
      if(errors)
        ast_error(ast, "can't flatten arrow type");

      return false;
    }

    ast_replace(astp, flat);
    return true;
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(!flatten_arrows(&child, errors))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool safe_field_write(token_id cap, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Safe to write if every component is safe to write.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!safe_field_write(cap, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      ast_t* upper = viewpoint_upper(type);
      bool ok = safe_field_write(cap, upper);

      if(upper != type)
        ast_free_unattached(upper);

      return ok;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return cap_safetowrite(cap, cap_single(type));

    default: {}
  }

  assert(0);
  return false;
}

bool safe_to_write(ast_t* ast, ast_t* type)
{
  switch(ast_id(ast))
  {
    case TK_VAR:
    case TK_LET:
    case TK_VARREF:
    case TK_DONTCARE:
      return true;

    case TK_FVARREF:
    case TK_FLETREF:
    case TK_EMBEDREF:
    {
      // If the ast is x.f, we need the type of x, which will be a nominal
      // type or an arrow type, since we were able to lookup a field on it.
      AST_GET_CHILDREN(ast, left, right);
      ast_t* l_type = ast_type(left);

      // Any viewpoint adapted type will not be safe to write to.
      if(ast_id(l_type) != TK_NOMINAL)
        return false;

      token_id l_cap = cap_single(l_type);

      // If the RHS is safe to write, we're done.
      if(safe_field_write(l_cap, type))
        return true;

      // If the field type (without adaptation) is safe, then it's ok as
      // well. So iso.tag = ref should be allowed.
      ast_t* r_type = ast_type(right);
      return safe_field_write(l_cap, r_type);
    }

    case TK_TUPLE:
    {
      // At this point, we know these will be the same length.
      assert(ast_id(type) == TK_TUPLETYPE);
      ast_t* child = ast_child(ast);
      ast_t* type_child = ast_child(type);

      while(child != NULL)
      {
        if(!safe_to_write(child, type_child))
          return false;

        child = ast_sibling(child);
        type_child = ast_sibling(type_child);
      }

      assert(type_child == NULL);
      return true;
    }

    case TK_SEQ:
    {
      // Occurs when there is a tuple on the left. Each child of the tuple will
      // be a sequence, but only sequences with a single writeable child are
      // valid. Other types won't appear here.
      return safe_to_write(ast_child(ast), type);
    }

    default: {}
  }

  assert(0);
  return false;
}

bool safe_to_autorecover(ast_t* receiver_type, ast_t* type)
{
  switch(ast_id(receiver_type))
  {
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(receiver_type);

      while(child != NULL)
      {
        if(safe_to_autorecover(child, type))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_UNIONTYPE:
    {
      ast_t* child = ast_child(receiver_type);

      while(child != NULL)
      {
        if(!safe_to_autorecover(child, type))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return safe_field_write(cap_single(receiver_type), type);

    case TK_ARROW:
    {
      ast_t* upper = viewpoint_upper(receiver_type);
      bool ok = safe_to_autorecover(upper, type);

      if(upper != receiver_type)
        ast_free_unattached(upper);

      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}
