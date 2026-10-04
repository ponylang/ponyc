#include "fun.h"
#include "../type/alias.h"
#include "../type/typealias.h"
#include "../type/cap.h"
#include "../type/compattype.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../type/typeparam.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>

static bool verify_calls_runtime_override(pass_opt_t* opt, ast_t* ast)
{
  token_id tk = ast_id(ast);
  if((tk == TK_NEWREF) || (tk == TK_NEWBEREF) ||
     (tk == TK_FUNREF) || (tk == TK_BEREF))
  {
    ast_t* receiver = ast_child(ast);
    ast_t* method = ast_sibling(receiver);

    if(ast_id(receiver) == ast_id(ast))
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);

    // Look up the original method definition for this method call.
    deferred_reification_t* method_def = lookup(opt, ast, ast_type(receiver),
      ast_name(method));
    ast_t* method_ast = method_def->ast;

    // The deferred reification doesn't own the underlying AST so we can free it
    // safely.
    deferred_reify_free(method_def);

    ast_t* method_parent = ast_parent(method_ast);
    ast_t* method_entity = ast_parent(method_parent);

    if(ast_id(method_parent) == TK_METHODGROUP)
      method_entity = ast_parent(method_entity);

    if(ast_id(method_entity) != TK_PRIMITIVE)
    {
      ast_error(opt->check.errors, ast,
        "the runtime_override_defaults method of the Main actor can only call functions on primitives");
      return false;
    }
    else
    {
      // recursively check function call tree for other non-primitive method calls
      if(!verify_calls_runtime_override(opt, method_ast))
        return false;
    }
  }

  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    // recursively check all child nodes for non-primitive method calls
    if(!verify_calls_runtime_override(opt, child))
      return false;

    child = ast_sibling(child);
  }
  return true;
}

static bool verify_main_runtime_override_defaults(pass_opt_t* opt, ast_t* ast)
{
  if(ast_id(opt->check.frame->type) != TK_ACTOR)
    return true;

  ast_t* type_id = ast_child(opt->check.frame->type);

  if(strcmp(ast_name(type_id), "Main"))
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, guard, can_error,
    body);
  ast_t* parent = ast_parent(ast);
  ast_t* type = ast_parent(parent);

  if(ast_id(parent) == TK_METHODGROUP)
    type = ast_parent(type);

  if(strcmp(ast_name(id), "runtime_override_defaults"))
    return true;

  bool ok = true;

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast,
      "the runtime_override_defaults method of the Main actor must be a function");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams,
      "the runtime_override_defaults method of the Main actor must not take type parameters");
    ok = false;
  }

  if(ast_childcount(params) != 1)
  {
    if(ast_pos(params) == ast_pos(type))
      ast_error(opt->check.errors, params,
        "The Main actor must have a runtime_override_defaults method which takes only a "
        "single RuntimeOptions parameter");
    else
      ast_error(opt->check.errors, params,
        "the runtime_override_defaults method of the Main actor must take only a single "
        "RuntimeOptions parameter");
    ok = false;
  }

  ast_t* param = ast_child(params);

  if(param != NULL)
  {
    ast_t* p_type = ast_childidx(param, 1);

    if(!is_runtime_options(p_type))
    {
      ast_error(opt->check.errors, p_type, "must be of type RuntimeOptions");
      ok = false;
    }
  }

  if(!is_none(result))
  {
    ast_error(opt->check.errors, result,
      "the runtime_override_defaults method of the Main actor must return None");
    ok = false;
  }

  bool bare = ast_id(cap) == TK_AT;

  if(!bare)
  {
    ast_error(opt->check.errors, ast,
      "the runtime_override_defaults method of the Main actor must be a bare function");
    ok = false;
  }

  // check to make sure no function calls on non-primitives
  if(!verify_calls_runtime_override(opt, body))
  {
    ok = false;
  }

  return ok;
}

static bool verify_main_create(pass_opt_t* opt, ast_t* ast)
{
  if(ast_id(opt->check.frame->type) != TK_ACTOR)
    return true;

  ast_t* type_id = ast_child(opt->check.frame->type);

  if(strcmp(ast_name(type_id), "Main"))
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, guard, can_error);
  ast_t* mparent = ast_parent(ast);
  ast_t* type = ast_parent(mparent);

  if(ast_id(mparent) == TK_METHODGROUP)
    type = ast_parent(type);

  if(strcmp(ast_name(id), "create"))
    return true;

  bool ok = true;

  if(ast_id(ast) != TK_NEW)
  {
    ast_error(opt->check.errors, ast,
      "the create method of the Main actor must be a constructor");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams,
      "the create constructor of the Main actor must not take type parameters");
    ok = false;
  }

  if(ast_childcount(params) != 1)
  {
    if(ast_pos(params) == ast_pos(type))
      ast_error(opt->check.errors, params,
        "The Main actor must have a create constructor which takes only a "
        "single Env parameter");
    else
      ast_error(opt->check.errors, params,
        "the create constructor of the Main actor must take only a single Env "
        "parameter");
    ok = false;
  }

  ast_t* param = ast_child(params);

  if(param != NULL)
  {
    ast_t* p_type = ast_childidx(param, 1);

    if(!is_env(p_type))
    {
      ast_error(opt->check.errors, p_type, "must be of type Env");
      ok = false;
    }
  }

  return ok;
}

static bool verify_primitive_init(pass_opt_t* opt, ast_t* ast)
{
  if(ast_id(opt->check.frame->type) != TK_PRIMITIVE)
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, guard, can_error);

  if(strcmp(ast_name(id), "_init"))
    return true;

  bool ok = true;

  if(ast_id(ast_childidx(opt->check.frame->type, 1)) != TK_NONE)
  {
    ast_error(opt->check.errors, ast,
      "a primitive with type parameters cannot have an _init method");
    ok = false;
  }

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast,
      "a primitive _init method must be a function");
    ok = false;
  }

  if(ast_id(cap) != TK_BOX)
  {
    ast_error(opt->check.errors, cap,
      "a primitive _init method must use box as the receiver capability");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams,
      "a primitive _init method must not take type parameters");
    ok = false;
  }

  if(ast_childcount(params) != 0)
  {
    ast_error(opt->check.errors, params,
      "a primitive _init method must take no parameters");
    ok = false;
  }

  if(!is_none(result))
  {
    ast_error(opt->check.errors, result,
      "a primitive _init method must return None");
    ok = false;
  }

  if(ast_id(can_error) != TK_NONE)
  {
    ast_error(opt->check.errors, can_error,
      "a primitive _init method cannot be a partial function");
    ok = false;
  }

  return ok;
}

static bool verify_any_final(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, guard, can_error,
    body);

  if(strcmp(ast_name(id), "_final"))
    return true;

  bool ok = true;

  if(ast_id(opt->check.frame->type) == TK_STRUCT)
  {
    ast_error(opt->check.errors, ast, "a struct cannot have a _final method");
    ok = false;
  } else if((ast_id(opt->check.frame->type) == TK_PRIMITIVE) &&
    (ast_id(ast_childidx(opt->check.frame->type, 1)) != TK_NONE)) {
    ast_error(opt->check.errors, ast,
      "a primitive with type parameters cannot have a _final method");
    ok = false;
  }

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast, "a _final method must be a function");
    ok = false;
  }

  if(ast_id(cap) != TK_BOX)
  {
    ast_error(opt->check.errors, cap,
      "a _final method must use box as the receiver capability");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams,
      "a _final method must not take type parameters");
    ok = false;
  }

  if(ast_childcount(params) != 0)
  {
    ast_error(opt->check.errors, params,
      "a _final method must take no parameters");
    ok = false;
  }

  if(!is_none(result))
  {
    ast_error(opt->check.errors, result, "a _final method must return None");
    ok = false;
  }

  if(ast_id(can_error) != TK_NONE)
  {
    ast_error(opt->check.errors, can_error,
      "a _final method cannot be a partial function");
    ok = false;
  }

  return ok;
}

static bool is_receiver_enclosing_type(pass_opt_t* opt, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return (ast_t*)ast_data(type) == opt->check.frame->type;

    case TK_ARROW:
      return is_receiver_enclosing_type(opt, ast_childidx(type, 1));

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return false;

      bool result = is_receiver_enclosing_type(opt, unfolded);
      ast_free_unattached(unfolded);
      return result;
    }

    default:
      return false;
  }
}

static bool is_call_to_method(pass_opt_t* opt, ast_t* ast, ast_t* method)
{
  pony_assert((ast_id(ast) == TK_FUNREF) || (ast_id(ast) == TK_FUNCHAIN) ||
    (ast_id(ast) == TK_NEWREF));

  AST_GET_CHILDREN(ast, receiver, method_name);

  if(ast_id(receiver) == ast_id(ast))
    AST_GET_CHILDREN_NO_DECL(receiver, receiver, method_name);

  ast_t* receiver_type = ast_type(receiver);

  if(!is_receiver_enclosing_type(opt, receiver_type))
    return false;

  deferred_reification_t* method_def = lookup_try(opt, receiver,
    receiver_type, ast_name(method_name), true);

  if(method_def == NULL)
    return false;

  ast_t* method_ast = method_def->ast;
  deferred_reify_free(method_def);

  return method_ast == method;
}

static bool has_independent_error(pass_opt_t* opt, ast_t* ast, ast_t* method)
{
  ast_t* child = ast_child(ast);

  if((ast_id(ast) == TK_TRY) || (ast_id(ast) == TK_TRY_NO_CHECK))
  {
    pony_assert(child != NULL);
    child = ast_sibling(child);
  }

  bool found_child_error = false;

  while(child != NULL)
  {
    if(ast_canerror(child))
    {
      found_child_error = true;

      if(has_independent_error(opt, child, method))
        return true;
    }

    child = ast_sibling(child);
  }

  if(ast_canerror(ast))
  {
    if(ast_id(ast) == TK_ERROR)
      return true;

    if((ast_id(ast) == TK_FUNREF) || (ast_id(ast) == TK_FUNCHAIN) ||
      (ast_id(ast) == TK_NEWREF))
      return !is_call_to_method(opt, ast, method);

    if(!found_child_error)
      return true;
  }

  return false;
}

static bool show_partiality(pass_opt_t* opt, ast_t* ast)
{
  ast_t* child = ast_child(ast);
  bool found = false;

  if((ast_id(ast) == TK_TRY) || (ast_id(ast) == TK_TRY_NO_CHECK))
  {
      pony_assert(child != NULL);
      // Skip error in body.
      child = ast_sibling(child);
  }

  while(child != NULL)
  {
    if(ast_canerror(child))
      found |= show_partiality(opt, child);

    child = ast_sibling(child);
  }

  if(found)
    return true;

  if(ast_canerror(ast))
  {
    ast_error_continue(opt->check.errors, ast, "an error can be raised here");
    return true;
  }

  return false;
}

bool verify_fields_are_defined_in_constructor(pass_opt_t* opt, ast_t* ast)
{
  bool result = true;

  if(ast_id(ast) != TK_NEW)
    return result;

  ast_t* members = ast_parent(ast);

  if(ast_id(members) == TK_METHODGROUP)
    members = ast_parent(members);

  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        sym_status_t status;
        ast_t* id = ast_child(member);
        ast_t* def = ast_get(ast, ast_name(id), &status);

        if((def != member) || (status != SYM_DEFINED))
        {
          ast_error(opt->check.errors, def,
            "field left undefined in constructor");
          result = false;
        }

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(!result)
    ast_error(opt->check.errors, ast,
      "constructor with undefined fields is here");

  return result;
}

bool verify_fun(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_BE) || (ast_id(ast) == TK_FUN) ||
    (ast_id(ast) == TK_NEW));
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, type, guard, can_error,
    body);

  // Run checks tailored to specific kinds of methods, if any apply.
  if(!verify_main_create(opt, ast) ||
    !verify_main_runtime_override_defaults(opt, ast) ||
    !verify_primitive_init(opt, ast) ||
    !verify_any_final(opt, ast) ||
    !verify_fields_are_defined_in_constructor(opt, ast))
    return false;

  // Check parameter types.
  for(ast_t* param = ast_child(params); param != NULL; param = ast_sibling(param))
  {
    ast_t* p_type = ast_type(param);
    if(consume_type(p_type, TK_NONE, false, opt) == NULL)
    {
      ast_error(opt->check.errors, p_type, "illegal type for parameter");
      return false;
    }
  }


  // Check partial functions.
  if(ast_id(can_error) == TK_QUESTION)
  {
    // If the function is marked as partial, it must have the potential
    // to raise an error somewhere in the body. This check is skipped for
    // traits and interfaces - they are allowed to give a default implementation
    // of the method that does or does not have the potential to raise an error.
    bool is_trait =
      (ast_id(opt->check.frame->type) == TK_TRAIT) ||
      (ast_id(opt->check.frame->type) == TK_INTERFACE) ||
      (ast_id((ast_t*)ast_data(ast)) == TK_TRAIT) ||
      (ast_id((ast_t*)ast_data(ast)) == TK_INTERFACE);

    if(!is_trait &&
      !ast_canerror(body) &&
      (ast_id(ast_type(body)) != TK_COMPILE_INTRINSIC))
    {
      ast_error(opt->check.errors, can_error, "function signature is marked as "
        "partial but the function body cannot raise an error");
      return false;
    }

    if(!is_trait &&
      ast_canerror(body) &&
      (ast_type(body) != NULL) &&
      (ast_id(ast_type(body)) != TK_COMPILE_INTRINSIC) &&
      !has_independent_error(opt, body, ast))
    {
      ast_error(opt->check.errors, can_error, "function signature is marked as "
        "partial but the function body cannot raise an error");
      ast_error_continue(opt->check.errors, can_error, "only source of "
        "partiality is a recursive call to this method");
      return false;
    }
  } else {
    // If the function is not marked as partial, it must never raise an error.
    if(ast_canerror(body))
    {
      if(ast_id(ast) == TK_BE)
      {
        ast_error(opt->check.errors, can_error, "a behaviour must handle any "
          "potential error");
      } else if((ast_id(ast) == TK_NEW) &&
        (ast_id(opt->check.frame->type) == TK_ACTOR)) {
        ast_error(opt->check.errors, can_error, "an actor constructor must "
          "handle any potential error");
      } else {
        ast_error(opt->check.errors, can_error, "function signature is not "
          "marked as partial but the function body can raise an error");
      }
      show_partiality(opt, body);
      return false;
    }
  }

  return true;
}

static bool guards_same_subtype(ast_t* a, ast_t* b)
{
  if(ast_id(a) != ast_id(b))
    return false;

  if(ast_id(a) == TK_NOMINAL)
    return ast_name(ast_childidx(a, 1)) == ast_name(ast_childidx(b, 1));

  if(ast_id(a) == TK_TYPEPARAMREF)
    return ast_name(ast_child(a)) == ast_name(ast_child(b));

  if(ast_id(a) == TK_TUPLETYPE)
  {
    if(ast_childcount(a) != ast_childcount(b))
      return false;

    ast_t* ca = ast_child(a);
    ast_t* cb = ast_child(b);
    while(ca != NULL)
    {
      if(!guards_same_subtype(ca, cb))
        return false;
      ca = ast_sibling(ca);
      cb = ast_sibling(cb);
    }
    return true;
  }

  return false;
}

static bool single_guard_shadows(ast_t* earlier, ast_t* later,
  pass_opt_t* opt)
{
  pony_assert(ast_id(earlier) == TK_IFTYPEGUARD);
  pony_assert(ast_id(later) == TK_IFTYPEGUARD);

  ast_t* e_sub = ast_child(earlier);
  ast_t* e_super = ast_sibling(e_sub);
  ast_t* l_sub = ast_child(later);
  ast_t* l_super = ast_sibling(l_sub);

  return guards_same_subtype(e_sub, l_sub) &&
    is_subtype(l_super, e_super, NULL, opt);
}

static bool guard_shadows(ast_t* earlier, ast_t* later, pass_opt_t* opt)
{
  // Simple case: both are single guards.
  if((ast_id(earlier) == TK_IFTYPEGUARD) &&
    (ast_id(later) == TK_IFTYPEGUARD))
    return single_guard_shadows(earlier, later, opt);

  // If later is an OR, it's shadowed if every branch is individually shadowed
  // by earlier.
  if(ast_id(later) == TK_IFTYPEGUARD_OR)
  {
    ast_t* l_child = ast_child(later);
    while(l_child != NULL)
    {
      if(!guard_shadows(earlier, l_child, opt))
        return false;
      l_child = ast_sibling(l_child);
    }
    return true;
  }

  // If earlier is an OR, it shadows later if any branch shadows later.
  if(ast_id(earlier) == TK_IFTYPEGUARD_OR)
  {
    ast_t* e_child = ast_child(earlier);
    while(e_child != NULL)
    {
      if(guard_shadows(e_child, later, opt))
        return true;
      e_child = ast_sibling(e_child);
    }
    return false;
  }

  // Both are AND guards. Earlier shadows later if every constraint in earlier
  // has a matching constraint in later on the same type parameter where the
  // later supertype is at least as narrow (l_super <: e_super). This means
  // earlier is at least as broad as later — anything matching later also
  // matches earlier.
  if((ast_id(earlier) == TK_IFTYPEGUARD_AND) &&
    (ast_id(later) == TK_IFTYPEGUARD_AND))
  {
    ast_t* e_child = ast_child(earlier);
    while(e_child != NULL)
    {
      bool found = false;
      ast_t* l_child = ast_child(later);
      while(l_child != NULL)
      {
        if(single_guard_shadows(e_child, l_child, opt))
        {
          found = true;
          break;
        }
        l_child = ast_sibling(l_child);
      }

      if(!found)
        return false;

      e_child = ast_sibling(e_child);
    }
    return true;
  }

  // If later is an AND, it's shadowed if earlier (a single guard) shadows
  // any constraint in later (since satisfying more constraints is narrower).
  if(ast_id(later) == TK_IFTYPEGUARD_AND)
  {
    ast_t* l_child = ast_child(later);
    while(l_child != NULL)
    {
      if(guard_shadows(earlier, l_child, opt))
        return true;
      l_child = ast_sibling(l_child);
    }
    return false;
  }

  // An AND guard is narrower than any of its individual constraints — it
  // matches the intersection. It can never shadow a broader single guard.
  return false;
}

static void narrow_type_from_guard(ast_t* type, ast_t* guard)
{
  switch(ast_id(guard))
  {
    case TK_IFTYPEGUARD:
    {
      ast_t* store = ast_childidx(guard, 2);
      if(ast_id(store) == TK_TYPEPARAMS)
        typeparam_narrow(type, store);
      break;
    }
    case TK_IFTYPEGUARD_AND:
    {
      ast_t* child = ast_child(guard);
      while(child != NULL)
      {
        narrow_type_from_guard(type, child);
        child = ast_sibling(child);
      }
      break;
    }
    case TK_IFTYPEGUARD_OR:
    {
      ast_t* first = ast_child(guard);
      narrow_type_from_guard(type, first);
      break;
    }
    default:
      break;
  }
}

static bool types_same_ignoring_cap(ast_t* a, ast_t* b)
{
  if(ast_id(a) != ast_id(b))
    return false;

  switch(ast_id(a))
  {
    case TK_NOMINAL:
    {
      // Compare package, id, typeargs; skip cap and ephemeral.
      ast_t* a_pkg = ast_child(a);
      ast_t* b_pkg = ast_child(b);
      if(ast_id(a_pkg) != ast_id(b_pkg))
        return false;

      if(ast_id(a_pkg) != TK_NONE)
      {
        ast_t* a_pkg_id = ast_child(a_pkg);
        ast_t* b_pkg_id = ast_child(b_pkg);

        if(a_pkg_id != NULL && b_pkg_id != NULL)
        {
          if(ast_name(a_pkg_id) != ast_name(b_pkg_id))
            return false;
        }
        else if(a_pkg_id != b_pkg_id)
        {
          return false;
        }
      }

      ast_t* a_id = ast_sibling(a_pkg);
      ast_t* b_id = ast_sibling(b_pkg);

      if(ast_name(a_id) != ast_name(b_id))
        return false;

      ast_t* a_targs = ast_sibling(a_id);
      ast_t* b_targs = ast_sibling(b_id);

      if(ast_childcount(a_targs) != ast_childcount(b_targs))
        return false;

      ast_t* at = ast_child(a_targs);
      ast_t* bt = ast_child(b_targs);

      while(at != NULL && bt != NULL)
      {
        if(!types_same_ignoring_cap(at, bt))
          return false;

        at = ast_sibling(at);
        bt = ast_sibling(bt);
      }

      return true;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      if(ast_childcount(a) != ast_childcount(b))
        return false;

      ast_t* ac = ast_child(a);
      ast_t* bc = ast_child(b);

      while(ac != NULL && bc != NULL)
      {
        if(!types_same_ignoring_cap(ac, bc))
          return false;

        ac = ast_sibling(ac);
        bc = ast_sibling(bc);
      }

      return true;
    }

    case TK_TYPEPARAMREF:
    {
      ast_t* a_id = ast_child(a);
      ast_t* b_id = ast_child(b);
      return ast_name(a_id) == ast_name(b_id);
    }

    case TK_ARROW:
    {
      ast_t* ac = ast_child(a);
      ast_t* bc = ast_child(b);

      while(ac != NULL && bc != NULL)
      {
        if(!types_same_ignoring_cap(ac, bc))
          return false;

        ac = ast_sibling(ac);
        bc = ast_sibling(bc);
      }

      return ac == NULL && bc == NULL;
    }

    default:
      break;
  }

  return false;
}

bool verify_methodgroup(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_METHODGROUP);

  ast_t* default_method = ast_child(ast);
  pony_assert(default_method != NULL);

  AST_GET_CHILDREN(default_method, d_cap, d_id, d_typeparams, d_params,
    d_result, d_guard, d_can_error, d_body, d_docstring);

  (void)d_guard;
  (void)d_body;
  (void)d_docstring;

  bool d_partial = (ast_id(d_can_error) == TK_QUESTION);

  ast_t* spec = ast_sibling(default_method);

  while(spec != NULL)
  {
    AST_GET_CHILDREN(spec, s_cap, s_id, s_typeparams, s_params,
      s_result, s_guard_s, s_can_error, s_body, s_docstring);

    (void)s_body;
    (void)s_docstring;

    if(ast_id(s_guard_s) == TK_NONE)
    {
      spec = ast_sibling(spec);
      continue;
    }

    if(!d_partial && (ast_id(s_can_error) == TK_QUESTION))
    {
      ast_error(opt->check.errors, s_can_error,
        "specialization is partial but the default is not");
      ast_error_continue(opt->check.errors, d_can_error,
        "default method is defined here");
      return false;
    }

    if(ast_id(d_cap) != ast_id(s_cap))
    {
      ast_error(opt->check.errors, s_cap,
        "specialization receiver capability does not match the default");
      ast_error_continue(opt->check.errors, d_cap,
        "default receiver capability is defined here");
      return false;
    }

    if(ast_id(default_method) != ast_id(spec))
    {
      ast_error(opt->check.errors, spec,
        "specialization method kind does not match the default");
      ast_error_continue(opt->check.errors, default_method,
        "default method is defined here");
      return false;
    }

    if(ast_childcount(d_typeparams) != ast_childcount(s_typeparams))
    {
      ast_error(opt->check.errors, s_typeparams,
        "specialization has a different number of type parameters "
        "than the default");
      ast_error_continue(opt->check.errors, d_typeparams,
        "default type parameters are defined here");
      return false;
    }

    ast_t* d_tp = ast_child(d_typeparams);
    ast_t* s_tp = ast_child(s_typeparams);

    while(d_tp != NULL)
    {
      ast_t* d_constraint = ast_childidx(d_tp, 1);
      ast_t* s_constraint = ast_childidx(s_tp, 1);

      ast_t* d_constraint_cmp = ast_dup(d_constraint);
      if(ast_id(s_guard_s) != TK_NONE)
        narrow_type_from_guard(d_constraint_cmp, s_guard_s);

      if(!is_eqtype(d_constraint_cmp, s_constraint, NULL, opt))
      {
        ast_free_unattached(d_constraint_cmp);
        ast_error(opt->check.errors, s_constraint,
          "specialization type parameter constraint does not match "
          "the default");
        ast_error_continue(opt->check.errors, d_constraint,
          "default type parameter constraint is defined here");
        return false;
      }

      ast_free_unattached(d_constraint_cmp);
      d_tp = ast_sibling(d_tp);
      s_tp = ast_sibling(s_tp);
    }

    // Link the specialization's method-level type parameters to the default's
    // so that typeparam_root resolves to the same identity. Each method owns
    // its own TK_TYPEPARAM nodes; without this, is_eqtype fails on pointer
    // identity even when the type parameters are structurally identical.
    // Save and restore the original ast_data after the comparisons.
    size_t tp_count = ast_childcount(d_typeparams);
    ast_t** saved_data = NULL;

    if(tp_count > 0)
    {
      saved_data = (ast_t**)ponyint_pool_alloc_size(
        tp_count * sizeof(ast_t*));

      d_tp = ast_child(d_typeparams);
      s_tp = ast_child(s_typeparams);
      size_t i = 0;

      while(d_tp != NULL)
      {
        saved_data[i] = (ast_t*)ast_data(s_tp);
        ast_setdata(s_tp, d_tp);
        d_tp = ast_sibling(d_tp);
        s_tp = ast_sibling(s_tp);
        i++;
      }
    }

    bool params_ok = true;
    bool result_ok = true;

    if(ast_childcount(d_params) != ast_childcount(s_params))
    {
      ast_error(opt->check.errors, s_params,
        "specialization has a different number of parameters than the default");
      ast_error_continue(opt->check.errors, d_params,
        "default parameters are defined here");
      params_ok = false;
    }

    if(params_ok)
    {
      // The guard narrows class-level type parameters in the specialization's
      // scope (e.g. A becomes A val under "iftype A <: Any val"). The
      // specialization's return type references the narrowed version while the
      // default's references the original. Duplicate the default's param and
      // return types and apply the guard narrowing so both sides use the same
      // type parameter representation.
      ast_t* d_params_cmp = ast_dup(d_params);
      ast_t* d_result_cmp = ast_dup(d_result);

      if(ast_id(s_guard_s) != TK_NONE)
      {
        narrow_type_from_guard(d_params_cmp, s_guard_s);
        narrow_type_from_guard(d_result_cmp, s_guard_s);
      }

      ast_t* d_param = ast_child(d_params_cmp);
      ast_t* d_param_orig = ast_child(d_params);
      ast_t* s_param = ast_child(s_params);

      while(d_param != NULL)
      {
        ast_t* d_ptype = ast_childidx(d_param, 1);
        ast_t* s_ptype = ast_childidx(s_param, 1);

        if(!is_eqtype(d_ptype, s_ptype, NULL, opt))
        {
          ast_error(opt->check.errors, s_ptype,
            "specialization parameter type does not match the default");
          ast_error_continue(opt->check.errors, ast_childidx(d_param_orig, 1),
            "default parameter type is defined here");
          params_ok = false;
          break;
        }

        d_param = ast_sibling(d_param);
        d_param_orig = ast_sibling(d_param_orig);
        s_param = ast_sibling(s_param);
      }

      if(params_ok && !is_subtype(s_result, d_result_cmp, NULL, opt))
      {
        ast_error(opt->check.errors, s_result,
          "specialization return type is not a subtype of the default return "
          "type");
        ast_error_continue(opt->check.errors, d_result,
          "default return type is defined here");
        result_ok = false;
      }

      ast_free_unattached(d_params_cmp);
      ast_free_unattached(d_result_cmp);
    }

    // Restore the specialization's type parameter data pointers.
    if(tp_count > 0)
    {
      s_tp = ast_child(s_typeparams);
      size_t i = 0;

      while(s_tp != NULL)
      {
        ast_setdata(s_tp, saved_data[i]);
        s_tp = ast_sibling(s_tp);
        i++;
      }

      ponyint_pool_free_size(tp_count * sizeof(ast_t*), saved_data);
    }

    if(!params_ok || !result_ok)
      return false;

    spec = ast_sibling(spec);
  }

  // A later specialization is unreachable when an earlier guard's constraint
  // is a supertype — everything matching later already matches earlier.
  ast_t* earlier = ast_sibling(default_method);

  while(earlier != NULL)
  {
    ast_t* e_guard = ast_childidx(earlier, 5);
    ast_t* later = ast_sibling(earlier);

    while(later != NULL)
    {
      ast_t* l_guard = ast_childidx(later, 5);

      if(guard_shadows(e_guard, l_guard, opt))
      {
        ast_error(opt->check.errors, l_guard,
          "specialization is unreachable because a previous guard "
          "matches all the same types");
        ast_error_continue(opt->check.errors, e_guard,
          "this guard shadows the unreachable specialization");
        return false;
      }

      later = ast_sibling(later);
    }

    earlier = ast_sibling(earlier);
  }

  // Type overload validation: collect unguarded definitions and check
  // consistency rules across the overload set.
  ast_t* first_overload = NULL;
  ast_t* child = ast_child(ast);
  int overload_count = 0;

  while(child != NULL)
  {
    if(ast_id(ast_childidx(child, 5)) == TK_NONE)
    {
      if(first_overload == NULL)
        first_overload = child;

      overload_count++;
    }
    child = ast_sibling(child);
  }

  if(overload_count > 1)
  {
    // Overloads must differ in nominal type, not just capability.
    ast_t* outer = ast_child(ast);

    while(outer != NULL)
    {
      if(ast_id(ast_childidx(outer, 5)) != TK_NONE)
      {
        outer = ast_sibling(outer);
        continue;
      }

      ast_t* inner = ast_sibling(outer);

      while(inner != NULL)
      {
        if(ast_id(ast_childidx(inner, 5)) != TK_NONE)
        {
          inner = ast_sibling(inner);
          continue;
        }

        ast_t* o_params = ast_childidx(outer, 3);
        ast_t* i_params = ast_childidx(inner, 3);

        if(ast_childcount(o_params) == ast_childcount(i_params))
        {
          bool all_same = true;
          ast_t* op = ast_child(o_params);
          ast_t* ip = ast_child(i_params);

          while(op != NULL && ip != NULL)
          {
            ast_t* ot = ast_childidx(op, 1);
            ast_t* it = ast_childidx(ip, 1);

            if(!types_same_ignoring_cap(ot, it))
            {
              all_same = false;
              break;
            }

            op = ast_sibling(op);
            ip = ast_sibling(ip);
          }

          if(all_same)
          {
            ast_error(opt->check.errors, inner,
              "type overloads must differ in nominal type, not just "
              "capability");
            ast_error_continue(opt->check.errors, outer,
              "other overload is defined here");
            return false;
          }
        }

        inner = ast_sibling(inner);
      }

      outer = ast_sibling(outer);
    }

    // No method-level type parameters when type overloads exist.
    child = ast_child(ast);

    while(child != NULL)
    {
      if(ast_id(ast_childidx(child, 5)) == TK_NONE)
      {
        ast_t* tps = ast_childidx(child, 2);

        if(ast_childcount(tps) > 0)
        {
          ast_error(opt->check.errors, tps,
            "type-overloaded methods cannot have method-level type "
            "parameters");
          return false;
        }
      }
      child = ast_sibling(child);
    }

    // All overloads must use the same receiver capability and method kind.
    ast_t* first_cap = ast_child(first_overload);
    token_id first_kind = ast_id(first_overload);
    child = ast_child(ast);

    while(child != NULL)
    {
      if(ast_id(ast_childidx(child, 5)) == TK_NONE && child != first_overload)
      {
        if(ast_id(child) != first_kind)
        {
          ast_error(opt->check.errors, child,
            "all type overloads must be the same method kind");
          ast_error_continue(opt->check.errors, first_overload,
            "first overload is defined here");
          return false;
        }

        ast_t* c_cap = ast_child(child);

        if(ast_id(c_cap) != ast_id(first_cap))
        {
          ast_error(opt->check.errors, c_cap,
            "all type overloads must have the same receiver capability");
          ast_error_continue(opt->check.errors, first_cap,
            "first overload's receiver capability is defined here");
          return false;
        }
      }
      child = ast_sibling(child);
    }

    // When default arguments cause two overloads to accept the same call
    // shape, and neither is strictly more specific at shared positions, reject.
    outer = ast_child(ast);

    while(outer != NULL)
    {
      if(ast_id(ast_childidx(outer, 5)) != TK_NONE)
      {
        outer = ast_sibling(outer);
        continue;
      }

      ast_t* o_params = ast_childidx(outer, 3);
      size_t o_total = ast_childcount(o_params);
      size_t o_required = o_total;
      ast_t* op = ast_child(o_params);
      while(op != NULL)
      {
        if(ast_id(ast_childidx(op, 2)) != TK_NONE)
        {
          o_required = o_total - 1;
          ast_t* rest = ast_sibling(op);
          while(rest != NULL)
          {
            if(ast_id(ast_childidx(rest, 2)) != TK_NONE)
              o_required--;
            rest = ast_sibling(rest);
          }
          break;
        }
        op = ast_sibling(op);
      }

      ast_t* inner = ast_sibling(outer);

      while(inner != NULL)
      {
        if(ast_id(ast_childidx(inner, 5)) != TK_NONE)
        {
          inner = ast_sibling(inner);
          continue;
        }

        ast_t* i_params = ast_childidx(inner, 3);
        size_t i_total = ast_childcount(i_params);
        size_t i_required = i_total;
        ast_t* ip = ast_child(i_params);
        while(ip != NULL)
        {
          if(ast_id(ast_childidx(ip, 2)) != TK_NONE)
          {
            i_required = i_total - 1;
            ast_t* rest = ast_sibling(ip);
            while(rest != NULL)
            {
              if(ast_id(ast_childidx(rest, 2)) != TK_NONE)
                i_required--;
              rest = ast_sibling(rest);
            }
            break;
          }
          ip = ast_sibling(ip);
        }

        // Check if the arity ranges overlap.
        size_t shared_min = (o_required > i_required) ? o_required : i_required;
        size_t shared_max = (o_total < i_total) ? o_total : i_total;

        if(shared_min <= shared_max && o_total != i_total)
        {
          // Ranges overlap and arities differ — check if one is strictly more
          // specific at the shared positions.
          bool outer_sub_inner = true;
          bool inner_sub_outer = true;
          bool outer_proper = false;
          bool inner_proper = false;
          op = ast_child(o_params);
          ip = ast_child(i_params);

          for(size_t pos = 0; pos < shared_min; pos++)
          {
            ast_t* ot = ast_childidx(op, 1);
            ast_t* it = ast_childidx(ip, 1);

            bool o_sub_i = is_subtype(ot, it, NULL, opt);
            bool i_sub_o = is_subtype(it, ot, NULL, opt);

            if(!o_sub_i)
              outer_sub_inner = false;
            if(!i_sub_o)
              inner_sub_outer = false;
            if(o_sub_i && !i_sub_o)
              outer_proper = true;
            if(i_sub_o && !o_sub_i)
              inner_proper = true;

            op = ast_sibling(op);
            ip = ast_sibling(ip);
          }

          // Ambiguous only when the types are comparable: if neither
          // overload's params are subtypes of the other's, no single value
          // can match both, so the call site can always resolve by type.
          if(!outer_sub_inner && !inner_sub_outer)
          {
            inner = ast_sibling(inner);
            continue;
          }

          bool outer_strictly = outer_sub_inner && outer_proper;
          bool inner_strictly = inner_sub_outer && inner_proper;

          if(!outer_strictly && !inner_strictly)
          {
            ast_error(opt->check.errors, inner,
              "type overloads with default arguments create an ambiguous "
              "call shape");
            ast_error_continue(opt->check.errors, outer,
              "other overload is defined here");
            return false;
          }
        }

        inner = ast_sibling(inner);
      }

      outer = ast_sibling(outer);
    }

    // Type parameter collapse: reject overload pairs where one parameter is
    // a class-level type parameter and the other is a concrete type that
    // satisfies the constraint, since reification could make them identical.
    ast_t* entity = ast_parent(ast_parent(ast));
    ast_t* class_typeparams = ast_childidx(entity, 1);

    if(ast_id(class_typeparams) == TK_TYPEPARAMS)
    {
      outer = ast_child(ast);

      while(outer != NULL)
      {
        if(ast_id(ast_childidx(outer, 5)) != TK_NONE)
        {
          outer = ast_sibling(outer);
          continue;
        }

        ast_t* inner = ast_sibling(outer);

        while(inner != NULL)
        {
          if(ast_id(ast_childidx(inner, 5)) != TK_NONE)
          {
            inner = ast_sibling(inner);
            continue;
          }

          ast_t* o_params = ast_childidx(outer, 3);
          ast_t* i_params = ast_childidx(inner, 3);

          if(ast_childcount(o_params) == ast_childcount(i_params))
          {
            bool all_collapse = true;
            ast_t* op = ast_child(o_params);
            ast_t* ip = ast_child(i_params);

            while(op != NULL && ip != NULL)
            {
              ast_t* ot = ast_childidx(op, 1);
              ast_t* it = ast_childidx(ip, 1);

              if(ast_id(ot) == TK_TYPEPARAMREF &&
                ast_id(it) != TK_TYPEPARAMREF)
              {
                ast_t* constraint = typeparam_constraint(ot);

                if(constraint == NULL || !is_subtype(it, constraint, NULL, opt))
                  all_collapse = false;
              }
              else if(ast_id(it) == TK_TYPEPARAMREF &&
                ast_id(ot) != TK_TYPEPARAMREF)
              {
                ast_t* constraint = typeparam_constraint(it);

                if(constraint == NULL || !is_subtype(ot, constraint, NULL, opt))
                  all_collapse = false;
              }
              else if(ast_id(ot) != TK_TYPEPARAMREF &&
                ast_id(it) != TK_TYPEPARAMREF)
              {
                if(!is_eqtype(ot, it, NULL, opt))
                  all_collapse = false;
              }

              if(!all_collapse)
                break;

              op = ast_sibling(op);
              ip = ast_sibling(ip);
            }

            if(all_collapse)
            {
              ast_error(opt->check.errors, inner,
                "type overload would collapse with another overload after "
                "reification of a type parameter — use an iftype "
                "specialization instead");
              ast_error_continue(opt->check.errors, outer,
                "other overload is defined here");
              return false;
            }
          }

          inner = ast_sibling(inner);
        }

        outer = ast_sibling(outer);
      }
    }
  }

  return true;
}
