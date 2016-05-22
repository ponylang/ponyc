#include "sanitise.h"
#include "../ast/astbuild.h"
#include <assert.h>


// Collect the given type parameter
static void collect_type_param(ast_t* orig_param, ast_t* params, ast_t* args)
{
  assert(orig_param != NULL);

  // Get original type parameter info
  AST_GET_CHILDREN(orig_param, id, constraint, deflt);
  const char* name = ast_name(id);

  constraint = sanitise_type(constraint);
  assert(constraint != NULL);

  // New type parameter has the same constraint as the old one (sanitised)
  if(params != NULL)
  {
    BUILD(new_param, orig_param,
      NODE(TK_TYPEPARAM,
        ID(name)
        TREE(constraint)
        NONE));

    ast_append(params, new_param);
    ast_setid(params, TK_TYPEPARAMS);
  }

  // New type arguments binds to old type parameter
  if(args != NULL)
  {
    BUILD(new_arg, orig_param,
      NODE(TK_NOMINAL,
        NONE  // Package
        ID(name)
        NONE  // Type args
        NONE  // cap
        NONE)); // ephemeral

    ast_append(args, new_arg);
    ast_setid(args, TK_TYPEARGS);
  }
}


void collect_type_params(ast_t* ast, ast_t** out_params, ast_t** out_args)
{
  assert(ast != NULL);

  // Create params and args as TK_NONE, we'll change them if we find any type
  // params
  ast_t* params = out_params ? ast_from(ast, TK_NONE) : NULL;
  ast_t* args = out_args ? ast_from(ast, TK_NONE) : NULL;

  // Find enclosing entity, or NULL if not within an entity
  ast_t* entity = ast;

  while(entity != NULL && ast_id(entity) != TK_INTERFACE &&
    ast_id(entity) != TK_TRAIT && ast_id(entity) != TK_PRIMITIVE &&
    ast_id(entity) != TK_STRUCT && ast_id(entity) != TK_CLASS &&
    ast_id(entity) != TK_ACTOR)
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
      collect_type_param(p, params, args);
  }

  // Collect type parameters defined on the method (if within a method)
  if(method != NULL)
  {
    ast_t* method_t_params = ast_childidx(method, 2);

    for(ast_t* p = ast_child(method_t_params); p != NULL; p = ast_sibling(p))
      collect_type_param(p, params, args);
  }

  if(out_params != NULL)
    *out_params = params;

  if(out_args != NULL)
    *out_args = args;
}


// Sanitise the given type (sub)AST, which has already been copied
static void sanitise(ast_t** astp)
{
  assert(astp != NULL);

  ast_t* type = *astp;
  assert(type != NULL);

  ast_clearflag(*astp, AST_FLAG_PASS_MASK);

  if(ast_id(type) == TK_TYPEPARAMREF)
  {
    // We have a type param reference, convert to a nominal
    ast_t* def = (ast_t*)ast_data(type);
    assert(def != NULL);

    const char* name = ast_name(ast_child(def));
    assert(name != NULL);

    REPLACE(astp,
      NODE(TK_NOMINAL,
        NONE      // Package name
        ID(name)
        NONE      // Type args
        NONE      // Capability
        NONE));   // Ephemeral

    return;
  }

  // Process all our children
  for(ast_t* p = ast_child(type); p != NULL; p = ast_sibling(p))
    sanitise(&p);
}


ast_t* sanitise_type(ast_t* type)
{
  assert(type != NULL);

  ast_t* new_type = ast_dup(type);
  sanitise(&new_type);
  return new_type;
}
