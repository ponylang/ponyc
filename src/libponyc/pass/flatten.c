#include "flatten.h"
#include "../type/alias.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../type/subtype.h"
#include "../type/typeparam.h"
#include <assert.h>

ast_result_t flatten_typeparamref(ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, cap, eph);

  if(ast_id(cap) != TK_NONE)
  {
    ast_error(cap, "can't specify a capability on a type parameter");
    return AST_ERROR;
  }

  typeparam_set_cap(ast);
  return AST_OK;
}

static ast_result_t flatten_noconstraint(typecheck_t* t, ast_t* ast)
{
  if(t->frame->constraint != NULL)
  {
    switch(ast_id(ast))
    {
      case TK_TUPLETYPE:
        ast_error(ast, "tuple types can't be used as constraints");
        return AST_ERROR;

      case TK_ARROW:
        if(t->frame->method == NULL)
        {
          ast_error(ast, "arrow types can't be used as type constraints");
          return AST_ERROR;
        }
        break;

      default: {}
    }
  }

  return AST_OK;
}

static ast_result_t flatten_sendable_params(ast_t* params)
{
  ast_t* param = ast_child(params);
  ast_result_t r = AST_OK;

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type, def);

    if(!sendable(type))
    {
      ast_error(type, "this parameter must be sendable (iso, val or tag)");
      r = AST_ERROR;
    }

    param = ast_sibling(param);
  }

  return r;
}

static ast_result_t flatten_constructor(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
      return flatten_sendable_params(params);

    default: {}
  }

  return AST_OK;
}

static ast_result_t flatten_async(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  return flatten_sendable_params(params);
}

// Process the given provides type
static bool flatten_provided_type(ast_t* provides_type, ast_t* error_at,
  ast_t* list_parent, ast_t** list_end)
{
  assert(error_at != NULL);
  assert(provides_type != NULL);
  assert(list_parent != NULL);
  assert(list_end != NULL);

  switch(ast_id(provides_type))
  {
    case TK_PROVIDES:
    case TK_ISECTTYPE:
      // Flatten all children
      for(ast_t* p = ast_child(provides_type); p != NULL; p = ast_sibling(p))
      {
        if(!flatten_provided_type(p, error_at, list_parent, list_end))
          return false;
      }

      return true;

    case TK_NOMINAL:
    {
      // Check type is a trait or interface
      ast_t* def = (ast_t*)ast_data(provides_type);
      assert(def != NULL);

      if(ast_id(def) != TK_TRAIT && ast_id(def) != TK_INTERFACE)
      {
        ast_error(error_at, "can only provide traits and interfaces");
        ast_error(provides_type, "invalid type here");
        return false;
      }

      // Add type to new provides list
      ast_list_append(list_parent, list_end, provides_type);
      ast_setdata(*list_end, ast_data(provides_type));

      return true;
    }

    default:
      ast_error(error_at,
        "provides type may only be an intersect of traits and interfaces");
      ast_error(provides_type, "invalid type here");
      return false;
  }
}

// Flatten a provides type into a list, checking all types are traits or
// interfaces
static ast_result_t flatten_provides_list(ast_t* provider, int index)
{
  assert(provider != NULL);

  ast_t* provides = ast_childidx(provider, index);

  if(ast_id(provides) == TK_NONE)
    return AST_OK;

  ast_t* list = ast_from(provides, TK_PROVIDES);
  ast_t* list_end = NULL;

  if(!flatten_provided_type(provides, provider, list, &list_end))
  {
    ast_free(list);
    return AST_ERROR;
  }

  ast_replace(&provides, list);
  return AST_OK;
}

ast_result_t pass_flatten(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NEW:
    {
      switch(ast_id(t->frame->type))
      {
        case TK_CLASS:
          return flatten_constructor(ast);

        case TK_ACTOR:
          return flatten_async(ast);

        default: {}
      }
      break;
    }

    case TK_BE:
      return flatten_async(ast);

    case TK_TUPLETYPE:
    case TK_ARROW:
      return flatten_noconstraint(t, ast);

    case TK_TYPEPARAMREF:
      return flatten_typeparamref(ast);

    case TK_FVAR:
    case TK_FLET:
      return flatten_provides_list(ast, 3);

    case TK_EMBED:
    {
      AST_GET_CHILDREN(ast, id, type, init);

      // An embedded field must have a known, class type.
      if(!is_known(type) ||
        (!is_entity(type, TK_STRUCT) && !is_entity(type, TK_CLASS)))
      {
        ast_error(ast, "embedded fields must be classes or structs");
        return AST_ERROR;
      }

      return flatten_provides_list(ast, 3);
    }

    case TK_ACTOR:
    case TK_CLASS:
    case TK_STRUCT:
    case TK_PRIMITIVE:
    case TK_TRAIT:
    case TK_INTERFACE:
      return flatten_provides_list(ast, 3);

    case TK_OBJECT:
      return flatten_provides_list(ast, 0);

    default: {}
  }

  return AST_OK;
}
