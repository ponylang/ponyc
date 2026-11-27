#include "sanitise.h"
#include "typeparam.h"
#include "../ast/astbuild.h"
#include "../pass/pass.h"
#include "ponyassert.h"


// Collect the given type parameter
static void collect_type_param(ast_t* orig_param, ast_t* params, ast_t* args, pass_opt_t* opt)
{
  pony_assert(orig_param != NULL);

  // Get original type parameter info
  AST_GET_CHILDREN(orig_param, id, constraint, deflt);
  const char* name = ast_name(id);

  constraint = sanitise_type(constraint, opt);
  pony_assert(constraint != NULL);

  // New type parameter has the same constraint as the old one (sanitised)
  if(params != NULL)
  {
    BUILD(new_param, orig_param,
      NODE(TK_TYPEPARAM,
        ID(name)
        TREE(constraint)
        NONE));

    // The new type param is directly bound to the original, so store the
    // original in the data field for use in subtyping checks.
    ast_setdata(new_param, orig_param);

    ast_append(params, new_param);
    ast_setid(params, TK_TYPEPARAMS);
  }

  // New type arguments binds to old type parameter
  if(args != NULL)
  {
    BUILD(new_arg, orig_param,
      NODE(TK_NOMINAL,
        NONE  // Module
        ID(name)
        NONE  // Type args
        NONE  // cap
        NONE)); // ephemeral

    ast_append(args, new_arg);
    ast_setid(args, TK_TYPEARGS);
  }
}


void remap_collected_typeparams(ast_t* ast, ast_t* params)
{
  if(ast == NULL)
    return;

  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(ast);

    if(def != NULL)
    {
      ast_t* root = typeparam_root(def);

      for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
      {
        if(typeparam_root((ast_t*)ast_data(p)) == root)
        {
          ast_setdata(ast, p);
          break;
        }
      }
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    remap_collected_typeparams(child, params);
}


void own_collected_typeparams(ast_t* params)
{
  if(ast_id(params) != TK_TYPEPARAMS)
    return;

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
    ast_setdata(p, p);
}


void collect_type_params(ast_t* ast, ast_t** out_params, ast_t** out_args, pass_opt_t* opt)
{
  pony_assert(ast != NULL);

  // Create params and args as TK_NONE, we'll change them if we find any type
  // params
  ast_t* params = out_params ? ast_from(ast, TK_NONE) : NULL;
  ast_t* args = out_args ? ast_from(ast, TK_NONE) : NULL;

  // Find enclosing entity, or NULL if not within an entity
  ast_t* entity = ast;

  while(entity != NULL && ast_id(entity) != TK_INTERFACE &&
    ast_id(entity) != TK_TRAIT && ast_id(entity) != TK_PRIMITIVE &&
    ast_id(entity) != TK_STRUCT && ast_id(entity) != TK_CLASS &&
    ast_id(entity) != TK_ACTOR && ast_id(entity) != TK_TYPE)
  {
    entity = ast_parent(entity);
  }

  // Find enclosing method, or NULL if not within a method
  ast_t* method = ast;

  while(method != NULL && ast_id(method) != TK_FUN &&
    ast_id(method) != TK_NEW && ast_id(method) != TK_BE)
  {
    method = ast_parent(method);
  }

  // Collect type parameters defined on the entity (if within an entity)
  if(entity != NULL)
  {
    ast_t* entity_t_params = ast_childidx(entity, 1);

    for(ast_t* p = ast_child(entity_t_params); p != NULL; p = ast_sibling(p))
      collect_type_param(p, params, args, opt);
  }

  // Collect type parameters defined on the method (if within a method)
  if(method != NULL)
  {
    ast_t* method_t_params = ast_childidx(method, 2);

    for(ast_t* p = ast_child(method_t_params); p != NULL; p = ast_sibling(p))
      collect_type_param(p, params, args, opt);
  }

  // A constraint can mention the type parameters being collected. Point it at
  // the new parameters.
  if((params != NULL) && (ast_id(params) == TK_TYPEPARAMS))
  {
    for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
      remap_collected_typeparams(ast_childidx(p, 1), params);
  }

  if(out_params != NULL)
    *out_params = params;

  if(out_args != NULL)
    *out_args = args;
}


// Sanitise the given type (sub)AST, which has already been copied: clear its
// pass flags so the passes run on it again where it is placed. Type parameter
// and type alias references keep the definition they refer to; the type can
// be placed in a type whose own scope gives their names another meaning, and
// the anonymous type that receives a type parameter reference points it at
// its own parameter by identity (see collect_type_params).
static void sanitise(ast_t** astp, pass_opt_t* opt)
{
  pony_assert(astp != NULL);

  ast_t* type = *astp;
  pony_assert(type != NULL);

  ast_clearflag(type, AST_FLAG_PASS_MASK);

  if(ast_id(type) == TK_TYPEPARAMREF)
  {
    // The capability and ephemerality are worked out again from the
    // parameter's constraint, as for a type parameter named in source.
    AST_GET_CHILDREN(type, id, cap, eph);
    ast_replace(&cap, ast_from(cap, TK_NONE));
    ast_replace(&eph, ast_from(eph, TK_NONE));
    ast_clearflag(ast_child(type), AST_FLAG_PASS_MASK);
    return;
  }

  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
    sanitise(&p, opt);
}


ast_t* sanitise_type(ast_t* type, pass_opt_t* opt)
{
  pony_assert(type != NULL);

  ast_t* new_type = ast_dup(type);
  sanitise(&new_type, opt);
  return new_type;
}
