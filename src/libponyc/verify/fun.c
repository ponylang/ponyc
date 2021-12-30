#include "fun.h"
#include "../type/cap.h"
#include "../type/compattype.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "ponyassert.h"
#include <string.h>

static bool verify_calls_runtime_override(pass_opt_t* opt, ast_t* ast)
{
  token_id tk = ast_id(ast);
  if((tk == TK_NEWREF) || (tk == TK_NEWBEREF) ||
     (tk == TK_FUNREF) || (tk == TK_BEREF))
  {
    ast_t* method = ast_sibling(ast_child(ast));
    ast_t* receiver = ast_child(ast);

    // Look up the original method definition for this method call.
    deferred_reification_t* method_def = lookup(opt, ast, ast_type(receiver),
      ast_name(method));
    ast_t* method_ast = method_def->ast;

    // The deferred reification doesn't own the underlying AST so we can free it
    // safely.
    deferred_reify_free(method_def);

    if(ast_id(ast_parent(ast_parent(method_ast))) != TK_PRIMITIVE)
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

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);
  ast_t* type = ast_parent(ast_parent(ast));

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

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error);
  ast_t* type = ast_parent(ast_parent(ast));

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

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error);

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
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

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

static bool verify_serialise_space(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  if(strcmp(ast_name(id), "_serialise_space"))
    return true;

  bool ok = true;

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast, "_serialise_space must be a function");
    ok = false;
  }

  if(ast_id(cap) != TK_BOX)
  {
    ast_error(opt->check.errors, cap, "_serialise_space must be box");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams, "_serialise_space must not be polymorphic");
    ok = false;
  }

  if(ast_childcount(params) != 0)
  {
    ast_error(opt->check.errors, params, "_serialise_space must have no parameters");
    ok = false;
  }

  if(!is_literal(result, "USize"))
  {
    ast_error(opt->check.errors, result, "_serialise_space must return USize");
    ok = false;
  }

  return ok;
}

static bool verify_serialiser(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  if(strcmp(ast_name(id), "_serialise"))
    return true;

  bool ok = true;

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast, "_serialise must be a function");
    ok = false;
  }

  if(ast_id(cap) != TK_BOX)
  {
    ast_error(opt->check.errors, cap, "_serialise must be box");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams, "_serialise must not be polymorphic");
    ok = false;
  }

  if(ast_childcount(params) != 1)
  {
    ast_error(opt->check.errors, params, "_serialise must have one parameter");
    ok = false;
  }

  if(!is_none(result))
  {
    ast_error(opt->check.errors, result, "_serialise must return None");
    ok = false;
  }

  return ok;
}

static bool verify_deserialiser(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, result, can_error, body);

  if(strcmp(ast_name(id), "_deserialise"))
    return true;

  bool ok = true;

  if(ast_id(ast) != TK_FUN)
  {
    ast_error(opt->check.errors, ast, "_deserialise must be a function");
    ok = false;
  }

  if(ast_id(cap) != TK_REF)
  {
    ast_error(opt->check.errors, cap, "_deserialise must be ref");
    ok = false;
  }

  if(ast_id(typeparams) != TK_NONE)
  {
    ast_error(opt->check.errors, typeparams, "_deserialise must not be polymorphic");
    ok = false;
  }

  if(ast_childcount(params) != 1)
  {
    ast_error(opt->check.errors, params, "_deserialise must have one parameter");
    ok = false;
  }

  if(!is_none(result))
  {
    ast_error(opt->check.errors, result, "_deserialise must return None");
    ok = false;
  }

  return ok;
}

static bool verify_any_serialise(pass_opt_t* opt, ast_t* ast)
{
  if (!verify_serialise_space(opt, ast) || !verify_serialiser(opt, ast) ||
      !verify_deserialiser(opt, ast))
    return false;

  return true;
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

// From https://github.com/ponylang/ponyc/issues/3765:
//
// When an immutable object is sent across actors, the garbage collector traces
// its associated object graph, using the associated type descriptor.
// Structs don't have a type descriptor, so the GC can't trace them.
//
// An option is to track additional information for objects without type
// descriptors, but this incurs in severe overhead for the normal case.
//
// Another option is to disallow non-opaque objects without type descriptors to
// be the root of an object graph. Essentially, this means that one has to wrap
// non-tag structs (and tuple containing non-tag structs, since the GC will try
// to trace each element) inside another object with a type descriptor.
bool verify_behaviour_root_struct_param(pass_opt_t* opt, ast_t* type, bool is_in_tuple)
{
  // For correct error location, keep track of the original AST node,
  // even if we have to use the node's data as type node.
  ast_t* type_node = type;
  ast_t* cap = NULL;
  if (ast_id(type) == TK_NOMINAL)
  {
    // We're still interested in the capability of the type as it was used,
    // so save that information.
    cap = ast_childidx(type, 3);
    type = (ast_t*)ast_data(type);
  }

  switch(ast_id(type))
  {
    case TK_STRUCT:
    {
      if(cap == NULL)
        cap = ast_childidx(type, 2);

      if(ast_id(cap) == TK_TAG)
        return true;

      if(is_in_tuple)
      {
        ast_error(opt->check.errors, type_node,
          "cannot use a tuple with a struct type member as parameter to a behaviour; "
          "you will need to wrap the struct in a class or actor, "
          "or change its capability to \"tag\"");
      } else {
        ast_error(opt->check.errors, type_node,
          "only \"tag\" structs can be used as parameters to a behaviour; "
          "you will need to wrap the struct in a class or actor, "
          "or change its capability to \"tag\"");
      }
      ast_error_continue(opt->check.errors, type, "definition is here");
      return false;
    }

    case TK_TUPLETYPE:
    {
      ast_t* member = ast_child(type);
      while(member != NULL)
      {
        if(!verify_behaviour_root_struct_param(opt, member, true))
          return false;
        member = ast_sibling(member);
      }
    }

    default: {}
  }

  return true;
}

bool verify_behaviour_parameters(pass_opt_t* opt, ast_t* ast)
{
  if(ast_id(ast) != TK_BE)
    return true;

  AST_GET_CHILDREN(ast, cap, id, typeparams, params, type, can_error, body);
  ast_t* param = ast_child(params);
  while(param != NULL)
  {
    ast_t* type = ast_type(param);
    if(!verify_behaviour_root_struct_param(opt, type, false))
      return false;

    param = ast_sibling(param);
  }

  return true;
}

bool verify_fun(pass_opt_t* opt, ast_t* ast)
{
  pony_assert((ast_id(ast) == TK_BE) || (ast_id(ast) == TK_FUN) ||
    (ast_id(ast) == TK_NEW));
  AST_GET_CHILDREN(ast, cap, id, typeparams, params, type, can_error, body);

  // Run checks tailored to specific kinds of methods, if any apply.
  if(!verify_main_create(opt, ast) ||
    !verify_main_runtime_override_defaults(opt, ast) ||
    !verify_primitive_init(opt, ast) ||
    !verify_any_final(opt, ast) ||
    !verify_any_serialise(opt, ast) ||
    !verify_behaviour_parameters(opt, ast))
    return false;

  for (ast_t* param = ast_child(params);
       param != NULL;
       param = ast_sibling(param))
  {
    ast_t* var_type = ast_childidx(param, 1);
    if (!is_compat_type(var_type, var_type))
    {
      ast_error(opt->check.errors, ast,
        "Invalid type for parameter: '%s'",
        ast_print_type(var_type));
      switch(ast_id(var_type))
      {
        case TK_NOMINAL:
        case TK_TYPEPARAMREF:
          {
            ast_t* cap = cap_fetch(var_type);
            ast_t* eph = ast_sibling(cap);

            if(ast_id(eph) == TK_EPHEMERAL)
            {
              ast_error(opt->check.errors, ast,
                "Parameters cannot have ephemeral capability. "
                "Instead remove the ephemeral modifier and consume the parameter");
            }
          }

        default: {}
      }
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
