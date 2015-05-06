#include "names.h"
#include "../ast/astbuild.h"
#include "../type/alias.h"
#include "../type/reify.h"
#include "../pkg/package.h"
#include <assert.h>

static void names_applycap_index(ast_t* ast, ast_t* cap, ast_t* ephemeral,
  int index)
{
  ast_t* a_cap = ast_childidx(ast, index);
  ast_t* a_ephemeral = ast_sibling(a_cap);

  if(ast_id(cap) != TK_NONE)
    ast_replace(&a_cap, cap);

  if(ast_id(ephemeral) != TK_NONE)
    ast_replace(&a_ephemeral, ephemeral);
}

static bool names_applycap(ast_t* ast, ast_t* cap, ast_t* ephemeral)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      if(ast_id(cap) != TK_NONE)
      {
        ast_error(cap,
          "can't specify a capability for an alias to a type expression");
        return false;
      }

      if(ast_id(ephemeral) != TK_NONE)
      {
        ast_error(ephemeral,
          "can't specify ephemerality for an alias to a type expression");
        return false;
      }

      return true;
    }

    case TK_NOMINAL:
      names_applycap_index(ast, cap, ephemeral, 3);
      return true;

    case TK_ARROW:
      return names_applycap(ast_childidx(ast, 1), cap, ephemeral);

    default: {}
  }

  assert(0);
  return false;
}

static bool names_resolvealias(pass_opt_t* opt, ast_t* def, ast_t** type)
{
  ast_state_t state = (ast_state_t)((uint64_t)ast_data(def));

  switch(state)
  {
    case AST_STATE_INITIAL:
      ast_setdata(def, (void*)AST_STATE_INPROGRESS);
      break;

    case AST_STATE_INPROGRESS:
      ast_error(def, "type aliases can't be recursive");
      return false;

    case AST_STATE_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  if(ast_visit(type, NULL, pass_names, opt) != AST_OK)
    return false;

  ast_setdata(def, (void*)AST_STATE_DONE);
  return true;
}

static bool names_typealias(pass_opt_t* opt, ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, pkg, id, typeargs, cap, eph);

  // Make sure the alias is resolved,
  AST_GET_CHILDREN(def, alias_id, typeparams, def_cap, provides);
  ast_t* alias = ast_child(provides);

  if(!names_resolvealias(opt, def, &alias))
    return false;

  // Reify the alias.
  ast_t* r_alias = reify(typeparams, alias, typeparams, typeargs);

  if(r_alias == NULL)
    return false;

  // Apply our cap and ephemeral to the result.
  if(!names_applycap(r_alias, cap, eph))
  {
    ast_free_unattached(r_alias);
    return false;
  }

  // Maintain the position info of the original reference to aid error
  // reporting.
  ast_setpos(r_alias, ast_line(ast), ast_pos(ast));

  // Replace this with the alias.
  ast_replace(astp, r_alias);
  return true;
}

static bool names_typeparam(ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, package, type, typeargs, cap, ephemeral);
  assert(ast_id(package) == TK_NONE);

  if(ast_id(typeargs) != TK_NONE)
  {
    ast_error(typeargs, "can't qualify a type parameter with type arguments");
    return false;
  }

  // Change to a typeparamref.
  REPLACE(astp,
    NODE(TK_TYPEPARAMREF,
      TREE(type)
      TREE(cap)
      TREE(ephemeral)));

  ast_setdata(*astp, def);
  return true;
}

static bool names_type(typecheck_t* t, ast_t** astp, ast_t* def)
{
  ast_t* ast = *astp;
  AST_GET_CHILDREN(ast, package, id, typeparams, cap, eph);
  token_id tcap = ast_id(cap);

  if((tcap == TK_NONE) && (ast_id(def) == TK_PRIMITIVE))
  {
    // A primitive without a capability is a val, even if it is a constraint.
    tcap = TK_VAL;
  } else if(t->frame->constraint != NULL) {
    // A constraint is modified to a generic capability.
    switch(tcap)
    {
      case TK_NONE: tcap = TK_ANY_GENERIC; break;
      case TK_BOX: tcap = TK_BOX_GENERIC; break;
      case TK_TAG: tcap = TK_TAG_GENERIC; break;
      default: {}
    }
  } else if(tcap == TK_NONE) {
    // Otherwise, we use the default capability.
    tcap = ast_id(ast_childidx(def, 2));
  }

  ast_setid(cap, tcap);

  // Keep the actual package id.
  ast_append(ast, package);
  ast_replace(&package, package_id(def));

  // Store our definition for later use.
  ast_setdata(ast, def);
  return true;
}

bool names_nominal(pass_opt_t* opt, ast_t* scope, ast_t** astp)
{
  typecheck_t* t = &opt->check;
  ast_t* ast = *astp;

  if(ast_data(ast) != NULL)
    return true;

  AST_GET_CHILDREN(ast, package_id, type_id, typeparams, cap, eph);
  bool local_package;

  // Keep some stats.
  t->stats.names_count++;

  if(ast_id(cap) == TK_NONE)
    t->stats.default_caps_count++;

  // Find our actual package.
  if(ast_id(package_id) != TK_NONE)
  {
    const char* name = ast_name(package_id);

    if(name[0] == '$')
      scope = ast_get(ast_nearest(scope, TK_PROGRAM), name, NULL);
    else
      scope = ast_get(scope, name, NULL);

    if((scope == NULL) || (ast_id(scope) != TK_PACKAGE))
    {
      ast_error(package_id, "can't find package '%s'", name);
      return false;
    }

    local_package = scope == ast_nearest(ast, TK_PACKAGE);
  } else {
    local_package = true;
  }

  // Check for a private type.
  const char* name = ast_name(type_id);
  bool r = true;

  if(!local_package && (name[0] == '_'))
  {
    ast_error(type_id, "can't access a private type from another package");
    r = false;
  }

  // Find our definition.
  ast_t* def = ast_get(scope, name, NULL);

  if(def == NULL)
  {
    ast_error(type_id, "can't find definition of '%s'", name);
    r = false;
  } else {
    switch(ast_id(def))
    {
      case TK_TYPE:
        r = names_typealias(opt, astp, def);
        break;

      case TK_TYPEPARAM:
        r = names_typeparam(astp, def);
        break;

      case TK_INTERFACE:
      case TK_TRAIT:
      case TK_PRIMITIVE:
      case TK_CLASS:
      case TK_ACTOR:
        r = names_type(t, astp, def);
        break;

      default:
        ast_error(type_id, "definition of '%s' is not a type", name);
        r = false;
        break;
    }
  }

  return r;
}

static bool names_arrow(ast_t* ast)
{
  ast_t* left = ast_child(ast);

  switch(ast_id(left))
  {
    case TK_BOXTYPE:
    case TK_THISTYPE:
    case TK_TYPEPARAMREF:
      return true;

    default: {}
  }

  ast_error(left, "only 'this', 'box' and type parameters can be viewpoints");
  return false;
}

static bool names_sendable_params(ast_t* params)
{
  ast_t* param = ast_child(params);
  bool ok = true;

  while(param != NULL)
  {
    AST_GET_CHILDREN(param, id, type, def);

    if(!sendable(type))
    {
      ast_error(type, "this parameter must be sendable (iso, val or tag)");
      ok = false;
    }

    param = ast_sibling(param);
  }

  return ok;
}

static bool names_constructor(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_VAL:
      return names_sendable_params(params);

    default: {}
  }

  return true;
}

static bool names_async(ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body,
    docstring);

  return names_sendable_params(params);
}

ast_result_t pass_names(ast_t** astp, pass_opt_t* options)
{
  typecheck_t* t = &options->check;
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      if(!names_nominal(options, ast, astp))
        return AST_ERROR;
      break;

    case TK_ARROW:
      if(!names_arrow(ast))
        return AST_ERROR;
      break;

    case TK_NEW:
    {
      switch(ast_id(t->frame->type))
      {
        case TK_CLASS:
          if(!names_constructor(ast))
            return AST_ERROR;
          break;

        case TK_ACTOR:
          if(!names_async(ast))
            return AST_ERROR;
          break;

        default: {}
      }
      break;
    }

    case TK_BE:
      if(!names_async(ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
