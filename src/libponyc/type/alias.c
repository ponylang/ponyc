#include "alias.h"
#include "assemble.h"
#include "cap.h"
#include "compattype.h"
#include "typealias.h"
#include "viewpoint.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include "ponyassert.h"

typedef enum recovery_t
{
  RECOVERY_NONE,
  RECOVERY_IMMUTABLE,
  RECOVERY_MUTABLE
} recovery_t;

static ast_t* alias_single(ast_t* type)
{
  ast_t* cap = cap_fetch(type);
  ast_t* ephemeral = ast_sibling(cap);

  switch(ast_id(ephemeral))
  {
    case TK_EPHEMERAL:
      type = ast_dup(type);
      ephemeral = ast_sibling(cap_fetch(type));
      ast_setid(ephemeral, TK_NONE);
      return type;

    case TK_NONE:
    {
      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_TRN:
        case TK_CAP_SEND:
        case TK_CAP_ANY:
        case TK_NONE:
          type = ast_dup(type);
          ephemeral = ast_sibling(cap_fetch(type));
          ast_setid(ephemeral, TK_ALIASED);
          break;

        default: {}
      }

      return type;
    }

    case TK_ALIASED:
      return type;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static ast_t* recover_single(ast_t* type, token_id rcap,
  recovery_t* tuple_elem_recover)
{
  ast_t* cap = cap_fetch(type);
  token_id tcap = ast_id(cap);
  bool rec_eph = false;

  switch(tcap)
  {
    case TK_ISO:
    {
      if(rcap == TK_NONE)
        rcap = TK_ISO;

      // If ephemeral, any rcap is acceptable. Otherwise, only iso or tag is.
      // This is because the iso may have come from outside the recover
      // expression.
      ast_t* eph = ast_sibling(cap);

      if((ast_id(eph) != TK_EPHEMERAL) && (rcap != TK_ISO) && (rcap != TK_TAG))
        return NULL;
      break;
    }

    case TK_TRN:
    case TK_REF:
      // These recover as iso, so any rcap is acceptable. We also set the rcap
      // as ephemeral to allow direct recovery to iso or trn.
      if(rcap == TK_NONE)
        rcap = TK_ISO;

      if((tuple_elem_recover != NULL) && !cap_safetomove(rcap, tcap, WRITE))
      {
        switch(rcap)
        {
          case TK_ISO:
          case TK_TRN:
            if(*tuple_elem_recover != RECOVERY_NONE)
              return NULL;

            *tuple_elem_recover = RECOVERY_MUTABLE;
            break;

          case TK_VAL:
            if(*tuple_elem_recover == RECOVERY_MUTABLE)
              return NULL;

            *tuple_elem_recover = RECOVERY_IMMUTABLE;
            break;

          default: {}
        }
      }

      rec_eph = true;
      break;

    case TK_VAL:
    case TK_BOX:
    case TK_CAP_READ:
      // These recover as val, so mutable rcaps are not acceptable.
      if(rcap == TK_NONE)
        rcap = TK_VAL;

      switch(rcap)
      {
        case TK_ISO:
        case TK_TRN:
        case TK_REF:
        case TK_CAP_READ:
        case TK_CAP_SEND:
        case TK_CAP_ALIAS:
        case TK_CAP_ANY:
          return NULL;

        case TK_VAL:
          if((tuple_elem_recover != NULL) && (tcap != TK_VAL))
          {
            if(*tuple_elem_recover == RECOVERY_MUTABLE)
              return NULL;

            *tuple_elem_recover = RECOVERY_IMMUTABLE;
          }
          break;

        case TK_BOX:
        case TK_TAG:
        case TK_CAP_SHARE:
          break;

        default:
          pony_assert(0);
          return NULL;
      }
      break;

    case TK_TAG:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
    case TK_CAP_ALIAS:
    case TK_CAP_ANY:
      // Recovers as tag or itself only.
      if(rcap == TK_NONE)
        rcap = tcap;

      if((rcap != TK_TAG) && (rcap != tcap))
        return NULL;
      break;

    default:
      pony_assert(0);
      return NULL;
  }

  type = ast_dup(type);
  cap = cap_fetch(type);
  ast_setid(cap, rcap);

  if(rec_eph)
    ast_setid(ast_sibling(cap), TK_EPHEMERAL);

  return type;
}

static ast_t* consume_single(ast_t* type, token_id ccap, bool keep_double_ephemeral)
{
  type = ast_dup(type);
  ast_t* cap = cap_fetch(type);
  ast_t* eph = ast_sibling(cap);
  token_id tcap = ast_id(cap);
  token_id teph = ast_id(eph);
  ast_setid(cap, tcap);
  ast_setid(eph, teph);

  switch(teph)
  {
    case TK_NONE:
      ast_setid(eph, TK_EPHEMERAL);
      break;

    case TK_ALIASED:
      if(ccap == TK_ALIASED)
        ast_setid(eph, TK_NONE);
      break;

    case TK_EPHEMERAL:
      switch(tcap)
      {
        case TK_ISO:
        case TK_TRN:
          if (!keep_double_ephemeral)
          {
            ast_free_unattached(type);
            return NULL;
          }

        default: {}
      }

    default: {}
  }

  switch(ccap)
  {
    case TK_NONE:
    case TK_ALIASED:
      ccap = tcap;
      break;

    default: {}
  }

  // We're only modifying the cap, not the type, so the bounds are the same.
  if(!is_cap_sub_cap_bound(tcap, TK_EPHEMERAL, ccap, TK_NONE))
  {
    ast_free_unattached(type);
    return NULL;
  }

  ast_setid(cap, ccap);
  return type;
}

ast_t* alias(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // alias each element
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(r_type, alias(child));
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return alias_single(type);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return NULL;

      ast_t* result = alias(unfolded);

      if(result != unfolded)
        ast_free_unattached(unfolded);

      return result;
    }

    case TK_ARROW:
    {
      // Alias just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);

      // A val viewpoint produces only val or tag results for any right-hand
      // type, and both alias to themselves. Aliasing inside the arrow would
      // give val->(A!) which differs from (val->A)! when A=iso:
      // val->(iso!) = val->tag = tag, but (val->iso)! = val! = val.
      // Return the arrow unchanged to avoid this over-conservative result.
      if(ast_id(left) == TK_VAL)
        return type;

      BUILD(r_type, type,
        NODE(TK_ARROW,
          TREE(left)
          TREE(alias(right))));

      return r_type;
    }

    case TK_LAMBDATYPE:
    case TK_BARELAMBDATYPE:
    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
    case TK_DONTCARETYPE:
      return type;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

ast_t* consume_type(ast_t* type, token_id cap, bool keep_double_ephemeral)
{
  switch(ast_id(type))
  {
    case TK_DONTCARETYPE:
      return type;

    case TK_TUPLETYPE:
    {
      // Consume each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* r_right = consume_type(child, cap, keep_double_ephemeral);

        if(r_right == NULL)
        {
          ast_free_unattached(r_type);
          return NULL;
        }

        ast_append(r_type, r_right);
        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_ISECTTYPE:
    {
      // for intersection-only:
      // check compatibility, since we can't have a variable
      // which consists of incompatible caps
      ast_t* first = ast_child(type);
      ast_t* second = ast_sibling(first);
      if (!is_compat_type(first, second))
      {
        return NULL;
      }
    }
    // fall-through
    case TK_UNIONTYPE:
    {
      // Consume each element.
      ast_t* r_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* r_right = consume_type(child, cap, keep_double_ephemeral);

        if(r_right != NULL)
        {
          ast_append(r_type, r_right);
        }

        child = ast_sibling(child);
      }

      return r_type;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return consume_single(type, cap, keep_double_ephemeral);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return NULL;

      ast_t* result = consume_type(unfolded, cap, keep_double_ephemeral);

      if(result != unfolded)
        ast_free_unattached(unfolded);

      return result;
    }

    case TK_ARROW:
    {
      // Consume just the right side. The left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);

      ast_t* r_right = consume_type(right, cap, keep_double_ephemeral);
      if (r_right == NULL)
      {
        return NULL;
      }

      BUILD(r_type, type,
        NODE(TK_ARROW,
        TREE(left)
        TREE(r_right)));

      return r_type;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

static ast_t* recover_type_inner(ast_t* type, token_id cap,
  recovery_t* tuple_elem_recover);

static ast_t* recover_complex(ast_t* type, token_id cap,
  recovery_t* tuple_elem_recover)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      break;

    default:
      pony_assert(false);
      break;
  }

  // recover each element
  ast_t* r_type = ast_from(type, ast_id(type));
  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    ast_t* r_right = recover_type_inner(child, cap, tuple_elem_recover);

    if(r_right == NULL)
    {
      ast_free_unattached(r_type);
      return NULL;
    }

    ast_append(r_type, r_right);
    child = ast_sibling(child);
  }

  return r_type;
}

static ast_t* recover_type_inner(ast_t* type, token_id cap,
  recovery_t* tuple_elem_recover)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return recover_complex(type, cap, tuple_elem_recover);

    case TK_TUPLETYPE:
    {
      if(tuple_elem_recover)
        return recover_complex(type, cap, tuple_elem_recover);

      recovery_t elem_recover = RECOVERY_NONE;
      return recover_complex(type, cap, &elem_recover);
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      return recover_single(type, cap, tuple_elem_recover);

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return NULL;

      ast_t* result = recover_type_inner(unfolded, cap, tuple_elem_recover);

      if(result != unfolded)
        ast_free_unattached(unfolded);

      return result;
    }

    case TK_ARROW:
    {
      // recover just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);
      ast_t* r_right = recover_type_inner(right, cap, tuple_elem_recover);

      if(r_right == NULL)
        return NULL;

      ast_t* r_type = ast_from(type, TK_ARROW);
      ast_add(r_type, r_right);
      ast_add(r_type, left);
      return r_type;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

ast_t* recover_type(ast_t* type, token_id cap)
{
  return recover_type_inner(type, cap, NULL);
}

ast_t* chain_type(ast_t* type, token_id fun_cap, bool recovered_call)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      ast_t* c_type = ast_from(type, ast_id(type));
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_append(c_type, chain_type(child, fun_cap, recovered_call));
        child = ast_sibling(child);
      }

      return c_type;
    }

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return NULL;

      ast_t* result = chain_type(unfolded, fun_cap, recovered_call);

      if(result != unfolded)
        ast_free_unattached(unfolded);

      return result;
    }

    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
    {
      ast_t* cap = cap_fetch(type);
      ast_t* eph = ast_sibling(cap);

      switch(ast_id(cap))
      {
        case TK_REF:
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
        case TK_CAP_READ:
        case TK_CAP_SHARE:
        case TK_CAP_ALIAS:
          // If the receiver type aliases as itself, it stays the same after
          // chaining.
          return ast_dup(type);

        case TK_CAP_ANY:
          // If the receiver is #any, the call is on a tag method and we can
          // chain as #any.
          pony_assert(fun_cap == TK_TAG);
          return ast_dup(type);

        default: {}
      }

      if(ast_id(eph) == TK_NONE)
      {
        pony_assert(recovered_call || (fun_cap == TK_TAG));
        // If the receiver isn't ephemeral, we recovered the call and the type
        // stays the same.
        return ast_dup(type);
      }

      pony_assert(ast_id(eph) == TK_EPHEMERAL);

      // If the call was auto-recovered, no alias can exist outside of the
      // object's isolation boundary.
      if(recovered_call)
        return ast_dup(type);

      switch(ast_id(cap))
      {
        case TK_ISO:
        case TK_CAP_SEND:
          switch(fun_cap)
          {
            case TK_TAG:
              return ast_dup(type);

            case TK_ISO:
              return alias(alias(type));

            default: {}
          }
          break;

        case TK_TRN:
          switch(fun_cap)
          {
            case TK_TAG:
            case TK_BOX:
              return ast_dup(type);

            case TK_TRN:
              return alias(alias(type));

            default: {}
          }
          break;

        default:
          pony_assert(false);
          return NULL;
      }

      return set_cap_and_ephemeral(type, fun_cap, TK_NONE);
    }

    case TK_ARROW:
    {
      // Chain just the right side. the left side is either 'this' or a type
      // parameter, and stays the same.
      AST_GET_CHILDREN(type, left, right);
      ast_t* c_right = chain_type(right, fun_cap, recovered_call);

      ast_t* c_type = ast_from(type, TK_ARROW);
      ast_add(c_type, c_right);
      ast_add(c_type, left);
      return c_type;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return type;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

bool sendable(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      // Test each element.
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        if(!sendable(child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);

      // Determine if the left side has a generic cap that needs
      // case-splitting. For generic caps, viewpoint_upper computes a single
      // bound that can incorrectly classify non-sendable types as sendable
      // (e.g. this->(T #any !) with box receiver upper-bounds to tag, which
      // is sendable, but ref->(T #any !) = T #alias which is not).
      //
      // Case-splitting enumerates the concrete capabilities the left side
      // could be and checks sendability for each, matching the approach used
      // by viewpoint_reifythis and viewpoint_reifytypeparam for subtyping.
      token_id concrete_caps[6];
      int num_caps = 0;

      switch(ast_id(left))
      {
        case TK_THISTYPE:
          concrete_caps[0] = TK_REF;
          concrete_caps[1] = TK_VAL;
          concrete_caps[2] = TK_BOX;
          num_caps = 3;
          break;

        case TK_NOMINAL:
        case TK_TYPEPARAMREF:
        case TK_TYPEALIASREF:
        {
          ast_t* l_cap = cap_fetch(left);

          switch(ast_id(l_cap))
          {
            case TK_CAP_READ:
              concrete_caps[0] = TK_REF;
              concrete_caps[1] = TK_VAL;
              concrete_caps[2] = TK_BOX;
              num_caps = 3;
              break;

            case TK_CAP_SEND:
              concrete_caps[0] = TK_ISO;
              concrete_caps[1] = TK_VAL;
              concrete_caps[2] = TK_TAG;
              num_caps = 3;
              break;

            case TK_CAP_SHARE:
              concrete_caps[0] = TK_VAL;
              concrete_caps[1] = TK_TAG;
              num_caps = 2;
              break;

            case TK_CAP_ALIAS:
              concrete_caps[0] = TK_REF;
              concrete_caps[1] = TK_VAL;
              concrete_caps[2] = TK_BOX;
              concrete_caps[3] = TK_TAG;
              num_caps = 4;
              break;

            case TK_CAP_ANY:
              concrete_caps[0] = TK_ISO;
              concrete_caps[1] = TK_TRN;
              concrete_caps[2] = TK_REF;
              concrete_caps[3] = TK_VAL;
              concrete_caps[4] = TK_BOX;
              concrete_caps[5] = TK_TAG;
              num_caps = 6;
              break;

            default:
              break;
          }
          break;
        }

        default:
          break;
      }

      if(num_caps == 0)
      {
        // Single concrete cap: use viewpoint_upper directly.
        ast_t* upper = viewpoint_upper(type);

        if(upper == NULL)
          return false;

        bool ok = sendable(upper);
        ast_free_unattached(upper);
        return ok;
      }

      // Generic cap: check that ALL concrete instantiations are sendable.
      for(int i = 0; i < num_caps; i++)
      {
        ast_t* temp_left = ast_from(left, concrete_caps[i]);

        BUILD(temp_arrow, type,
          NODE(TK_ARROW,
            TREE(temp_left)
            TREE(ast_dup(right))));

        ast_t* upper = viewpoint_upper(temp_arrow);
        ast_free_unattached(temp_arrow);

        if(upper == NULL)
          return false;

        bool ok = sendable(upper);
        ast_free_unattached(upper);

        if(!ok)
          return false;
      }

      return true;
    }

    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, pkg, id, typeparams, cap, eph);
      return cap_sendable(ast_id(cap));
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, eph);
      return cap_sendable(ast_id(cap));
    }

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return false;

      bool ok = sendable(unfolded);
      ast_free_unattached(unfolded);
      return ok;
    }

    case TK_FUNTYPE:
    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      return false;

    default: {}
  }

  pony_assert(0);
  return false;
}
