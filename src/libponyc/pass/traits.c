#include "traits.h"
#include "../ast/token.h"
#include "../pkg/package.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/assemble.h"
#include <assert.h>


/** We use a 4 stage process to flatten traits for each inheritting type.
 *
 * 1.  For type T we find all the methods provided by all the traits T
 *     provides. These methods are sorted into a list per method name.
 *
 * 2.  For each method name M we found in stage 1 we perform one of two
 *     actions, depending on whether T contains a local definition of M.
 *
 * 2A. If T does contain a local definition of M then we check that all the
 *     methods from stage 1 with name M are a super types of T.M. Any that are
 *     not are flagged as errors.
 *
 * 2B. If T does not contain a local definition of M then we find the most
 *     general method G from the list found in stage 1. The most general method
 *     is any that is a sub type of all other methods in the list. It is an
 *     error if no such method exists. Method G is added to type T.
 *
 * 3.  For each method name M we found in stage 1 we now find a suitable body.
 *     If T contains a local body definition that is used. Otherwise we find
 *     all methods in the list from stage 1 that have exactly the same type as
 *     T.M and which provide a body. Each body is tagged with the trait it
 *     originated from, so diamond inheritance only counts as a single body.
 *     If there are no such bodies then T gets no body, this is fine for traits
 *     but not for concrete types.
 *     If there are multiple such bodies then T is marked as having a body
 *     ambiguity. Again this is fine for traits but not for concrete types.
 *
 * 4.  If T is a concrete type we check that all its methods have exactly one
 *     body each.
 */

#define BODY_AMBIGUOUS ((void*)2)

typedef struct methods_t
{
  symtab_t* symtab;
  ast_t* name_lists;
  ast_t* last_list;
} methods_t;

static bool build_entity_def(ast_t* entity);


// Add the given method to the relevant name list in the given symbol table
static bool add_method_to_list(ast_t* method, methods_t* method_info,
  const char *entity_name)
{
  assert(method != NULL);
  assert(method_info != NULL);
  assert(entity_name != NULL);

  const char* name = ast_name(ast_childidx(method, 1));
  assert(name != NULL);

  symtab_t* symtab = method_info->symtab;
  assert(symtab != NULL);

  // Entity doesn't yet have method, add it to our list for later
  ast_t* list = (ast_t*)symtab_find(symtab, name, NULL);

  if(list == NULL)
  {
    ast_t* case_clash = (ast_t*)symtab_find_case(symtab, name, NULL);

    if(case_clash != NULL)
    {
      ast_error(case_clash, "in %s method name differs only in case",
        entity_name);
      ast_error(method, "previous definition is here");
      return false;
    }

    // First instance of this name
    list = ast_blank(TK_ID);
    ast_set_name(list, name);
    symtab_add(symtab, name, (void*)list, SYM_NONE);

    if(method_info->last_list == NULL)
      ast_add(method_info->name_lists, list);
    else
      ast_add_sibling(method_info->last_list, list);

    method_info->last_list = list;
  }

  ast_add(list, method);
  return true;
}


// Add all methods from the provides list of the given entity into lists in the
// given symbol table. 
static bool collate_provided(ast_t* entity, methods_t* method_info)
{
  assert(entity != NULL);

  bool r = true;
  ast_t* traits = ast_childidx(entity, 3);

  for(ast_t* t = ast_child(traits); t != NULL; t = ast_sibling(t))
  {
    assert(ast_id(t) == TK_NOMINAL);
    ast_t* trait_def = (ast_t*)ast_data(t);
    assert(trait_def != NULL);

    // TODO: Check whether we need an error here
    if((ast_id(trait_def) != TK_TRAIT) && (ast_id(trait_def) != TK_INTERFACE))
      return false;

    if(!build_entity_def(trait_def))
      return false;

    ast_t* type_params = ast_childidx(trait_def, 1);
    ast_t* type_args = ast_childidx(t, 2);
    ast_t* trait_methods = ast_childidx(trait_def, 4);

    for(ast_t* m = ast_child(trait_methods); m != NULL; m = ast_sibling(m))
    {
      // Reify the method with the type parameters from trait definition and
      // the reified type arguments from trait reference
      ast_t* r_method = reify(m, type_params, type_args);
      const char* entity_name = ast_name(ast_child(entity));

      if(ast_id(r_method) == TK_BE || ast_id(r_method) == TK_NEW)
      {
        // Modify return type to the inheritting type
        ast_t* ret_type = ast_childidx(r_method, 4);
        assert(ast_id(ret_type) == TK_NOMINAL);

        const char* pkg_name = package_name(ast_nearest(entity, TK_PACKAGE));
        ast_set_name(ast_childidx(ret_type, 0), pkg_name);
        ast_set_name(ast_childidx(ret_type, 1), entity_name);
        ast_setdata(r_method, (void*)entity);
      }

      if(!add_method_to_list(r_method, method_info, entity_name))
        r = false;
    }
  }

  return r;
}


// Find the most general method from the given list (if any)
// Return NULL if no most general found
static ast_t* most_general_method(ast_t* list, ast_t* entity, const char* name)
{
  assert(list != NULL);
  assert(entity != NULL);
  assert(name != NULL);

  for(ast_t* p = ast_child(list); p != NULL; p = ast_sibling(p))
  {
    bool is_p_best = true;

    for(ast_t* q = ast_child(list); q != NULL; q = ast_sibling(q))
    {
      if(ast_id(p) != ast_id(q))
      {
        ast_error(entity, "Clashing types for method %s provided by traits",
          name);
        return NULL;
      }

      if(!is_subtype(p, q))
      {
        // p is less general than q
        is_p_best = false;
        break;
      }
    }

    if(is_p_best)
      return p;
  }

  ast_error(entity, "Clashing types for method %s provided by traits", name);
  return NULL;
}


// Check that all the methods in the given list are compatible with the given
// method
static bool methods_compatible(ast_t* list, ast_t* method, const char* name,
  ast_t* entity)
{
  assert(list != NULL);
  assert(method != NULL);
  assert(name != NULL);
  assert(entity != NULL);

  bool r = true;

  for(ast_t* p = ast_child(list); p != NULL; p = ast_sibling(p))
  {
    if(!is_subtype(method, p))
    {
      ast_error(method,
        "Clashing type for method %s provided by trait to %s %s",
        name, ast_get_print(entity), ast_name(ast_child(entity)));
      ast_error(p, "clashing method here");
      r = false;
    }
  }

  return r;
}


// Attach the appropriate body (if any) from the given method list to the given
// entity method 
static void attach_body_from_list(ast_t* list, ast_t* entity_method)
{
  assert(list != NULL);
  assert(entity_method != NULL);

  void* data = ast_data(entity_method);

  for(ast_t* p = ast_child(list); p != NULL; p = ast_sibling(p))
  {
    void* p_data = ast_data(p);

    if(p_data != NULL && p_data != data && is_eqtype(entity_method, p))
    {
      // p has a valid (and different) body

      if(data != NULL || p_data == BODY_AMBIGUOUS)
      {
        // Multiple possible bodies
        ast_setdata(entity_method, BODY_AMBIGUOUS);
        return;
      }

      // This is the first valid body, use it
      ast_t* old_body = ast_childidx(entity_method, 6);
      assert(ast_id(old_body) == TK_NONE);
      ast_replace(&old_body, ast_childidx(p, 6));
      ast_setdata(entity_method, p_data);
      data = p_data;
    }
  }
}


// Process the given list of provided methods (all of the same name) for the
// given entity
static bool process_method_name(ast_t* list, ast_t* entity)
{
  assert(list != NULL);
  assert(entity != NULL);

  const char* name = ast_name(list);
  assert(name != NULL);

  ast_t* existing = ast_get(entity, name, NULL);

  if(existing != NULL)
  {
    // Method is explicitly defined in entity
    if(!methods_compatible(list, existing, name, entity))
      return false;
  }
  else
  {
    // Method is not defined in entity
    existing = most_general_method(list, entity, name);
    if(existing == NULL)
      return false;

    // Add the most general method from the list to the entity
    existing = ast_dup(existing);
    ast_append(ast_childidx(entity, 4), existing);
    ast_set(entity, name, existing, SYM_NONE);
  }

  if(ast_data(existing) != (void*)entity)
    // Current body (if any) is not provided by this entity, get one from the
    // trait methods
    attach_body_from_list(list, existing);

  return true;
}


// Process the provides list of the given entity
static bool process_provides(ast_t* entity, methods_t* method_info)
{
  assert(entity != NULL);
  assert(method_info != NULL);

  // First initialise any method bodies we have
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
  {
    token_id variety = ast_id(m);

    if((variety == TK_BE || variety == TK_FUN || variety == TK_NEW) &&
      ast_id(ast_childidx(m, 6)) != TK_NONE)
      ast_setdata(m, entity);
  }

  // Sort all inheritted methods into our name lists and symbol table
  if(!collate_provided(entity, method_info))
    return false;

  bool r = true;

  // Process inheritted methods
  ast_t* name_lists = method_info->name_lists;
  assert(name_lists != NULL);

  for(ast_t* p = ast_child(name_lists); p != NULL; p = ast_sibling(p))
  {
    if(!process_method_name(p, entity))
      r = false;
  }

  return r;
}


// Add methods from provided traits into the given entity
static bool build_entity_def(ast_t* entity)
{
  assert(entity != NULL);

  ast_state_t state = (ast_state_t)(uint64_t)ast_data(entity);

  // Check for recursive definitions
  switch(state)
  {
    case AST_STATE_INITIAL:
      ast_setdata(entity, (void*)AST_STATE_INPROGRESS);
      break;

    case AST_STATE_INPROGRESS:
      ast_error(entity, "traits can't be recursive");
      return false;

    case AST_STATE_DONE:
      return true;

    default:
      assert(0);
      return false;
  }

  symtab_t* symtab = symtab_new();
  ast_t* name_lists = ast_blank(TK_MEMBERS);
  methods_t method_info = { symtab, name_lists, NULL };

  bool r = process_provides(entity, &method_info);

  symtab_free(symtab);
  ast_free(name_lists);

  ast_setdata(entity, (void*)AST_STATE_DONE);
  return r;
}


// Sort out symbol table for copied method body
static ast_result_t rescope(ast_t** astp, pass_opt_t* options)
{
  (void)options;
  ast_t* ast = *astp;

  if(ast_has_scope(ast))
  ast_clear_local(ast);

  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_PARAM:
    case TK_TYPEPARAM:
      assert(ast_child(ast) != NULL);
      ast_set(ast, ast_name(ast_child(ast)), ast, SYM_DEFINED);
      break;

    case TK_IDSEQ:
    {
      ast_t* scope = ast_parent(ast);

      for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
        ast_set(scope, ast_name(p), p, SYM_DEFINED);

      break;
    }

    default: {}
  }

  return AST_OK;
}


// Check resulting methods are compatible with the containing entity and patch
// up symbol tables
static bool post_process_methods(ast_t* entity, pass_opt_t* options,
  bool is_concrete)
{
  assert(entity != NULL);

  bool r = true;
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
  {
    token_id variety = ast_id(m);

    // Check behaviour compatability
    if(variety == TK_BE)
    {
      switch(ast_id(entity))
      {
        case TK_PRIMITIVE:
          ast_error(entity,
            "primitives can't provide traits that have behaviours");
          return false;

        case TK_CLASS:
          ast_error(entity, "classes can't have provide that have behaviours");
          return false;

        default:
          break;
      }
    }

    if(variety == TK_BE || variety == TK_FUN || variety == TK_NEW)
    {
      // Check concrete method bodies
      if(ast_data(m) == BODY_AMBIGUOUS)
      {
        if(is_concrete)
        {
          ast_error(m, "multiple possible method bodies from traits");
          r = false;
        }
      }
      else if(ast_data(m) == NULL)
      {
        if(is_concrete)
        {
          assert(ast_id(ast_childidx(m, 6)) == TK_NONE);
          ast_error(m, "no body found for method %d %s %s", is_concrete,
            ast_get_print(entity), ast_name(ast_child(entity)));
          r = false;
        }
      }
      else
      {
        assert(ast_id(ast_childidx(m, 6)) != TK_NONE);

        if(ast_data(m) != entity)
        {
          // Sort out copied symbol tables
          ast_visit(&m, rescope, NULL, options);
        }
      }
    }
  }

  return r;
}


ast_result_t pass_traits(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      if(!build_entity_def(ast) ||
        !post_process_methods(ast, options, false))
        return AST_ERROR;

      break;

    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      if(!build_entity_def(ast) ||
        !post_process_methods(ast, options, true))
        return AST_ERROR;

      break;

    default:
      break;
  }

  return AST_OK;
}
