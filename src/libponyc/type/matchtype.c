#include "matchtype.h"
#include "subtype.h"
#include "viewpoint.h"
#include "cap.h"
#include "alias.h"
#include <assert.h>

static bool is_interface_match_compatible(ast_t* match, ast_t* pattern)
{
  switch(ast_id(pattern))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(pattern);

      switch(ast_id(def))
      {
        case TK_PRIMITIVE:
        case TK_CLASS:
        case TK_ACTOR:
        {
          // The pattern type without capability must be a subtype of the
          // match type without capability.
          ast_t* t_match = viewpoint_tag(match);
          ast_t* t_pattern = viewpoint_tag(pattern);
          bool ok = is_subtype(t_pattern, t_match);
          ast_free_unattached(t_match);
          ast_free_unattached(t_pattern);

          if(!ok)
            return false;

          // An alias of the match cap must be a subtype of the pattern cap.
          ast_t* a_match = alias(match);
          token_id match_cap = cap_for_type(a_match);
          token_id pattern_cap = cap_for_type(pattern);
          ast_free_unattached(a_match);

          return is_cap_sub_cap(match_cap, pattern_cap);
        }

        case TK_TRAIT:
        {
          // An alias of the match cap must be a subtype of the pattern cap.
          ast_t* a_match = alias(match);
          token_id match_cap = cap_for_type(a_match);
          token_id pattern_cap = cap_for_type(pattern);
          ast_free_unattached(a_match);

          return is_cap_sub_cap(match_cap, pattern_cap);
        }

        default: {}
      }

      break;
    }

    case TK_STRUCTURAL:
    {
      // An alias of the match must be a subtype of the pattern.
      ast_t* a_match = alias(match);
      bool ok = is_subtype(a_match, pattern);
      ast_free_unattached(a_match);
      return ok;
    }

    case TK_UNIONTYPE:
    {
      // Must be compatible with some type in the pattern.
      ast_t* child = ast_child(pattern);

      while(child != NULL)
      {
        if(is_interface_match_compatible(match, child))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      // Must be compatible with all types in the pattern.
      ast_t* child = ast_child(pattern);

      while(child != NULL)
      {
        if(!is_interface_match_compatible(match, child))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_TUPLETYPE:
    {
      // A trait can never be a tuple. While a tuple can be passed as a {} tag,
      // a {} tag can't be pattern matched as a tuple because it can't be read,
      // which means it can't be destructured.
      return false;
    }

    case TK_ARROW:
    {
      // Must be compatible with the lower bounds of the pattern.
      ast_t* lower = viewpoint_lower(pattern);
      bool ok = is_interface_match_compatible(match, lower);
      ast_free_unattached(lower);
      return ok;
    }

    default: {}
  }

  assert(0);
  return false;
}

bool is_match_compatible(ast_t* match, ast_t* pattern)
{
  switch(ast_id(match))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(match);

      switch(ast_id(def))
      {
        case TK_PRIMITIVE:
        case TK_CLASS:
        case TK_ACTOR:
        {
          // With a concrete type, an alias of the match must be a subtype of
          // the pattern.
          ast_t* a_match = alias(match);
          bool ok = is_subtype(a_match, pattern);
          ast_free_unattached(a_match);
          return ok;
        }

        case TK_TRAIT:
          return is_interface_match_compatible(match, pattern);

        default: {}
      }
      break;
    }

    case TK_STRUCTURAL:
      return is_interface_match_compatible(match, pattern);

    case TK_TYPEPARAMREF:
    {
      // Check if our constraint is compatible.
      ast_t* def = (ast_t*)ast_data(match);
      ast_t* constraint = ast_childidx(def, 1);
      return is_match_compatible(constraint, pattern);
    }

    case TK_ARROW:
    {
      // Check the upper bounds of the right side.
      ast_t* right = ast_childidx(match, 1);
      ast_t* upper = viewpoint_upper(right);
      bool ok = is_match_compatible(upper, pattern);
      ast_free_unattached(upper);
      return ok;
    }

    case TK_UNIONTYPE:
    {
      // Some type must be compatible with the pattern.
      ast_t* child = ast_child(match);

      while(child != NULL)
      {
        if(is_match_compatible(child, pattern))
          return true;

        child = ast_sibling(child);
      }

      return false;
    }

    case TK_ISECTTYPE:
    {
      // All types must be compatible with the pattern.
      ast_t* child = ast_child(match);

      while(child != NULL)
      {
        if(!is_id_compatible(child, pattern))
          return false;

        child = ast_sibling(child);
      }

      return true;
    }

    case TK_TUPLETYPE:
    {
      // Must be pairwise compatible with the pattern.
      if(ast_id(pattern) != TK_TUPLETYPE)
        return false;

      ast_t* m_child = ast_child(match);
      ast_t* p_child = ast_child(pattern);

      while((m_child != NULL) && (p_child != NULL))
      {
        if(!is_match_compatible(m_child, p_child))
          return false;

        m_child = ast_sibling(m_child);
        p_child = ast_sibling(p_child);
      }

      return (m_child == NULL) && (p_child == NULL);
    }

    default: {}
  }

  assert(0);
  return false;
}
