#include "traits.h"
#include "sugar.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include "../pkg/package.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../../libponyrt/mem/pool.h"
#include <assert.h>


/** We use a 4 stage process to flatten traits for each inheritting type.
 *
 * 1.  For type T we run through the types in its provides list and process
 *     them. We check for definition loops using AST_STATE_* flags in the
 *     provided types' data pointers.
 *     As we do this we also convert the provides list from an intersect type
 *     to a simple list.
 *
 * 2.  For type T we run through all methods provided by all the traits and
 *     interfaces T provides. For each method definition M, with name N, we
 *     check whether T already contains a definition of N and then perform one
 *     three steps.
 *
 * 2A. If T does contain a local definition of N then we check that we check
 *     that M is a super type of T.N. If it isn't it is flagged as an error.
 *
 * 2B. If T does contain a local definition of N, but does already have a
 *     definition from another trait or interface, then we check that M has
 *     exactly the same signature as T.N. This includes parameter names,
 *     default values, etc. If the signatures differ T.N is flagged as an
 *     error. If the signatures do match we store default body information, if
 *     M provides one.
 *
 * 2C. If T does not yet contain a definition of N then we add one using the
 *     signature from M. We also store default body information, if M provides
 *     one.
 *
 * We now have all the required methods for T, but still need to determine
 * bodies for them.
 *
 * 3.  For each delegated field in T we run through the methods it provides and
 *     store delegation information for them.
 *     These must all already be in T since we can only delegate to types we
 *     explicitly provide.
 *
 * 4.  We run through each method M in T and resolve the body to use. The
 *     priority order is:
 *     a. Local definition.
 *        If T provides an explicit signature for M we use the body associated
 *        with that signature, even if that is no body.
 *        If not, move on to option b.
 *     b. Delegation.
 *        If M is delegated to exactly one field we generate a body for that.
 *        If M is delegated to multiple fields an error is flagged.
 *        If M is delegated to no fields move on to option c.
 *     c. Trait and interface default bodies.
 *        If exactly one default body is provided for M that is used.
 *        If multiple default bodies are provided for M then M is body
 *        ambiguous. For concrete types this is an error.
 *        For non-concrete types this is fine and M is appropriately marked. We
 *        wish to keep information about which bodies are ambiguous to allow us
 *        to generate helpful error messages. To allow this we store references
 *        to 2 possible unreified default body definitions in the data pointers
 *        of the error and body children of M. Therefore if the data pointer of
 *        the body child of M is NULL M is not ambiguous, if non-NULL it is.
 *        If no default bodies are provided for M then move on to step d.
 *     d. M has no body. For concrete types this is an error. For non-concrete
 *        types this is fine.
 *
 * Throughout the processing of type T each method in T has a method_t struct
 * which is stored in the data pointer to the method node. In stage 4 we free
 * these and set the data pointer to the body donor, ie the type in which the
 * body actually used was defined, or NULL if no body is used (only in traits
 * and interfaces). This is can be used in later passes to determine the source
 * of a method body.
 *
 * On any error processing type T we set its data pointer to AST_STATE_ERROR.
 * This allows any other type that provides T to fail while avoiding follow on
 * errors.
 */


/* Per method information needed while processing each type.
 *
 * In stages 2 and 3 we collect information about non-explicit method bodies,
 * which we then use in stage 4. This information is stored for each method in
 * a method_t struct. A lot of the information stored is only used to generate
 * helpful error messages.
 *
 * For delegation we record the field to delegate the method to, if any. If
 * multiple fields want to delegate the same method this is an error. We record
 * the first 2 fields for use in the error message and ignore any subsequent
 * fields.
 *
 * Similarly for default bodies we want to store a single body to use and a
 * second for error reporting. However, for use we need a reified body and for
 * error reporting we want the unreified method in the donating type. To handle
 * this we simple store 1 reified body and 2 unreified method references
 * separately. If the reified body is not eventually used it must be deleted,
 * but the unreified ones must not be.
 */
typedef struct method_t
{
  ast_t* body_donor;          // Type our body was defined in. Copied to data
                              // pointer when this structure is deleted
  ast_t* delegate_field_1;    // Fields to delegate to
  ast_t* delegate_field_2;
  ast_t* delegate_target_1;   // Types to delegate field to
  ast_t* delegate_target_2;
  ast_t* reified_default;     // Reified default body
  ast_t* default_body_src_1;  // Method definitions providing default body
  ast_t* default_body_src_2;
  bool local_def;             // Is there a local definition for this method
} method_t;


static bool trait_entity(ast_t* entity, pass_opt_t* options);


// Report whether the given node is a field
static bool is_field(ast_t* ast)
{
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
      return true;

    default: {}
  }

  return false;
}


// Report whether the given node is a method
static bool is_method(ast_t* ast)
{
  assert(ast != NULL);

  token_id variety = ast_id(ast);
  return (variety == TK_BE) || (variety == TK_FUN) || (variety == TK_NEW);
}


// Attach a new method_t structure to the given method
static void attach_method_t(ast_t* method, ast_t* body_donor, bool local_def)
{
  assert(method != NULL);

  method_t* p = POOL_ALLOC(method_t);
  p->body_donor = body_donor;
  p->delegate_field_1 = NULL;
  p->delegate_field_2 = NULL;
  p->delegate_target_1 = NULL;
  p->delegate_target_2 = NULL;
  p->reified_default = NULL;
  p->default_body_src_1 = NULL;
  p->default_body_src_2 = NULL;
  p->local_def = local_def;

  ast_setdata(method, p);
}


// Setup a method_t structure for each method in the given type
static void setup_local_methods(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* members = ast_childidx(ast, 4);
  assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
      attach_method_t(p, ast, true);
  }
}


// Tidy up the method_t structures in the given type
static void tidy_up(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* members = ast_childidx(ast, 4);
  assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      method_t* info = (method_t*)ast_data(p);
      assert(info != NULL);

      ast_t* body_donor = info->body_donor;
      ast_free_unattached(info->reified_default);
      POOL_FREE(method_t, info);
      ast_setdata(p, body_donor);
    }
  }
}


// Process the provides list for the given entity.
// Stage 1.
static bool provides_list(ast_t* entity, pass_opt_t* options)
{
  assert(entity != NULL);

  ast_t* provides = ast_childidx(entity, 3);
  assert(provides != NULL);

  bool r = true;

  for(ast_t* p = ast_child(provides); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) != TK_NOMINAL)
    {
      ast_error(p, "provides list may only contain traits and interfaces");
      return false;
    }

    // Check type is a trait or interface
    ast_t* def = (ast_t*)ast_data(p);
    assert(def != NULL);

    if(ast_id(def) != TK_TRAIT && ast_id(def) != TK_INTERFACE)
    {
      ast_error(p, "can only provide traits and interfaces");
      return false;
    }

    // Now process provided type
    if(!trait_entity(def, options))
      r = false;
  }

  return r;
}


// Compare the 2 given signatures to see if they are exactly the same
static bool compare_signatures(ast_t* sig_a, ast_t* sig_b)
{
  if(sig_a == NULL && sig_b == NULL)
    return true;

  if(sig_a == NULL || sig_b == NULL)
    return false;

  token_id a_id = ast_id(sig_a);

  if(a_id != ast_id(sig_b))
    return false;

  switch(a_id)
  {
    case TK_BE:
    case TK_FUN:
    case TK_NEW:
    {
      // Check everything except body and docstring, ie first 6 children
      ast_t* a_child = ast_child(sig_a);
      ast_t* b_child = ast_child(sig_b);

      for(int i = 0; i < 6; i++)
      {
        if(a_child == NULL || b_child == NULL)
          return false;

        if(!compare_signatures(a_child, b_child))
          return false;

        a_child = ast_sibling(a_child);
        b_child = ast_sibling(b_child);
      }

      return true;
    }

    case TK_STRING:
    case TK_ID:
    {
      // Can't just use strcmp, string literals may contain \0s
      size_t a_len = ast_name_len(sig_a);
      size_t b_len = ast_name_len(sig_b);

      if(a_len != b_len)
        return false;

      const char* a_text = ast_name(sig_a);
      const char* b_text = ast_name(sig_b);

      for(size_t i = 0; i < a_len; i++)
      {
        if(a_text[i] != b_text[i])
          return false;
      }

      return true;
    }

    case TK_INT:     return lexint_cmp(ast_int(sig_a), ast_int(sig_b)) == 0;
    case TK_FLOAT:   return ast_float(sig_a) == ast_float(sig_b);

    case TK_NOMINAL:
      if(ast_data(sig_a) != ast_data(sig_b))
        return false;

      break;

    default:
      break;
  }

  ast_t* a_child = ast_child(sig_a);
  ast_t* b_child = ast_child(sig_b);

  while(a_child != NULL && b_child != NULL)
  {
    if(!compare_signatures(a_child, b_child))
      return false;

    a_child = ast_sibling(a_child);
    b_child = ast_sibling(b_child);
  }

  if(a_child != NULL || b_child != NULL)
    return false;

  return true;
}


// Check that the given method is compatible with the existing definition.
// Return the method that ends up being in the entity or NULL on error.
// Stages 2A, 2B and 2C.
static ast_t* add_method(ast_t* entity, ast_t* existing_method,
  ast_t* new_method, ast_t** last_method)
{
  assert(entity != NULL);
  assert(new_method != NULL);
  assert(last_method != NULL);

  const char* name = ast_name(ast_childidx(new_method, 1));
  const char* entity_name = ast_name(ast_child(entity));

  if(existing_method == NULL)
  {
    // This method is new to the entity.
    // Stage 2C.
    ast_t* case_clash = ast_get_case(entity, name, NULL);

    if(case_clash != NULL)
    {
      ast_error(case_clash, "in %s method name %s differs only in case",
        entity_name, name);
      ast_error(new_method, "clashing method is here");
      return NULL;
    }

    attach_method_t(new_method, NULL, false);
    ast_list_append(ast_childidx(entity, 4), last_method, new_method);
    ast_set(entity, name, new_method, SYM_DEFINED);
    return *last_method;
  }

  method_t* info = (method_t*)ast_data(existing_method);
  assert(info != NULL);

  if(info->local_def)
  {
    // Existing method is a local definition, new method must be a subtype
    // Stage 2A
    if(is_subtype(existing_method, new_method, true))
      return existing_method;

    ast_error(existing_method,
      "local method %s is not compatible with provided version", name);
    ast_error(new_method, "clashing method is here");

    return NULL;
  }

  // Both method versions came from the provides list, their signatures must
  // match exactly
  // Stage 2B
  if(compare_signatures(existing_method, new_method))
    return existing_method;

  ast_error(entity, "clashing definitions of method %s provided, local "
    "disambiguation required", name);
  ast_error(existing_method, "provided here");
  ast_error(new_method, "and here");

  return NULL;
}


// Record default body information from the given method in the given method_t
// struct.
// Returns true if the given method is explicitly referenced (and so must not
// be deleted yet), false otherwise.
static bool record_default_body(ast_t* reified_method, ast_t* raw_method,
  method_t* info)
{
  assert(reified_method != NULL);
  assert(raw_method != NULL);
  assert(info != NULL);

  if(info->local_def) // Don't need a default body with a local definition
    return false;

  if(info->default_body_src_2 != NULL)  // Default body is already ambiguous
    return false;

  if(info->default_body_src_1 != NULL &&
    ast_data(info->default_body_src_1) == ast_data(raw_method))
    // That method body is already recorded
    return false;

  ast_t* body = ast_childidx(raw_method, 6);
  assert(body != NULL);

  if(ast_id(body) == TK_NONE)
  {
    if(ast_data(body) == NULL) // No body, no info to record
      return false;

    // Given method has an ambiguous body, propogate that info
    if(info->default_body_src_1 == NULL)
      info->default_body_src_1 = (ast_t*)ast_data(body);

    ast_t* err = ast_childidx(raw_method, 5);
    info->default_body_src_2 = (ast_t*)ast_data(err);

    assert(info->default_body_src_1 != NULL);
    assert(info->default_body_src_2 != NULL);
    return false;
  }

  // Given method has a body
  if(info->default_body_src_1 == NULL)
  {
    assert(info->reified_default == NULL);
    info->reified_default = reified_method;
    info->default_body_src_1 = raw_method;
    return true;
  }

  info->default_body_src_2 = raw_method;
  return false;
}


// Process the given provided method for the given entity.
// The method passed should be reified already and will be freed by this
// function.
static bool provided_method(pass_opt_t* opt, ast_t* entity,
  ast_t* reified_method, ast_t* raw_method, ast_t** last_method)
{
  assert(entity != NULL);
  assert(reified_method != NULL);
  assert(last_method != NULL);

  AST_GET_CHILDREN(reified_method, cap, id, typeparams, params, result,
    can_error, body, doc);

  if(ast_id(reified_method) == TK_BE || ast_id(reified_method) == TK_NEW)
  {
    // Modify return type to the inheriting type
    ast_t* this_type = type_for_this(opt, entity, ast_id(cap),
      TK_EPHEMERAL, true);

    ast_replace(&result, this_type);
  }

  // Ignore docstring
  if(ast_id(doc) == TK_STRING)
  {
    ast_set_name(doc, "");
    ast_setid(doc, TK_NONE);
  }

  // Check for existing method of the same name
  const char* name = ast_name(id);
  assert(name != NULL);

  ast_t* existing = ast_get(entity, name, NULL);

  if(existing != NULL && is_field(existing))
  {
    ast_error(existing, "field '%s' clashes with provided method", name);
    ast_error(raw_method, "method is defined here");
    ast_free_unattached(reified_method);
    return false;
  }

  existing = add_method(entity, existing, reified_method, last_method);

  if(existing == NULL)
  {
    ast_free_unattached(reified_method);
    return false;
  }

  method_t* info = (method_t*)ast_data(existing);
  assert(info != NULL);

  if(!record_default_body(reified_method, raw_method, info))
    ast_free_unattached(reified_method);

  return true;
}


// Process the method provided to the given entity.
// Stage 2.
static bool provided_methods(pass_opt_t* opt, ast_t* entity)
{
  assert(entity != NULL);

  ast_t* provides = ast_childidx(entity, 3);
  ast_t* entity_members = ast_childidx(entity, 4);
  ast_t* last_member = ast_childlast(entity_members);
  bool r = true;

  // Run through our provides list
  for(ast_t* p = ast_child(provides); p != NULL; p = ast_sibling(p))
  {
    ast_t* trait_def = (ast_t*)ast_data(p);
    assert(trait_def != NULL);

    ast_t* type_args = ast_childidx(p, 2);
    ast_t* type_params = ast_childidx(trait_def, 1);
    ast_t* members = ast_childidx(trait_def, 4);

    // Run through the methods of each provided type
    for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
    {
      // Check behaviour compatability
      if(ast_id(m) == TK_BE)
      {
        switch(ast_id(entity))
        {
          case TK_PRIMITIVE:
            ast_error(entity,
              "primitives can't provide traits that have behaviours");
            r = false;
            continue;

          case TK_STRUCT:
            ast_error(entity,
              "structs can't provide traits that have behaviours");
            r = false;
            continue;

          case TK_CLASS:
            ast_error(entity,
              "classes can't provide traits that have behaviours");
            r = false;
            continue;

          default: {}
        }
      }

      if(is_method(m))
      {
        // We have a provided method
        // Reify the method with the type parameters from trait definition and
        // type arguments from trait reference
        if(!reify_defaults(type_params, type_args, true))
          return false;

        ast_t* reified = reify(m, type_params, type_args);

        if(reified == NULL) // Reification error, already reported
          return false;

        if(!provided_method(opt, entity, reified, m, &last_member))
          r = false;
      }
    }
  }

  return r;
}


// Check that the given delegate target is in the entity's provides list and
// is a legal type for the field
static bool check_delegate(ast_t* entity, ast_t* field_type,
  ast_t* delegated)
{
  assert(entity != NULL);
  assert(field_type != NULL);
  assert(delegated != NULL);

  if(!is_subtype(field_type, delegated, true))
  {
    ast_error(delegated, "field not a subtype of delegate");
    ast_error(field_type, "field type: %s", ast_print_type(field_type));
    ast_error(delegated, "delegate type: %s", ast_print_type(delegated));
    return false;
  }

  ast_t* provides = ast_childidx(entity, 3);
  void* del_def = ast_data(delegated);

  for(ast_t* p = ast_child(provides); p != NULL; p = ast_sibling(p))
  {
    if(ast_data(p) == del_def)
      return true;
  }

  assert(ast_id(delegated) == TK_NOMINAL);
  ast_error(delegated,
    "cannot delegate to %s, containing entity does not provide it",
    ast_name(ast_childidx(delegated, 1)));
  return false;
}


// Process all field delegations in the given type.
// Stage 3.
static bool field_delegations(ast_t* entity)
{
  assert(entity != NULL);

  ast_t* members = ast_childidx(entity, 4);
  assert(members != NULL);

  bool r = true;

  // Check all fields
  for(ast_t* f = ast_child(members); f != NULL; f = ast_sibling(f))
  {
    if(is_field(f))
    {
      AST_GET_CHILDREN(f, id, f_type, value, delegates);

      // Check all delegates for field
      for(ast_t* d = ast_child(delegates); d != NULL; d = ast_sibling(d))
      {
        if(!check_delegate(entity, f_type, d))
        {
          r = false;
          continue;
        }

        // Mark all methods in delegate trait as targets for this field
        ast_t* trait = (ast_t*)ast_data(d);
        assert(trait != NULL);
        ast_t* t_members = ast_childidx(trait, 4);

        for(ast_t* m = ast_child(t_members); m != NULL; m = ast_sibling(m))
        {
          if(is_method(m))
          {
            // Mark method as delegate target for this field
            const char* method_name = ast_name(ast_childidx(m, 1));
            assert(method_name != NULL);

            ast_t* local_method = ast_get(entity, method_name, NULL);
            assert(local_method != NULL);

            method_t* info = (method_t*)ast_data(local_method);
            assert(info != NULL);

            if(info->delegate_field_1 == NULL)
            {
              // First delegate field for this method
              info->delegate_field_1 = f;
              info->delegate_target_1 = d;
            }
            else if(info->delegate_field_2 == NULL &&
              info->delegate_field_1 != f)
            {
              // We already have one delegate field, record second
              info->delegate_field_2 = f;
              info->delegate_target_2 = d;
            }
          }
        }
      }
    }
  }

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
    case TK_EMBED:
    case TK_PARAM:
    case TK_TYPEPARAM:
      assert(ast_child(ast) != NULL);
      ast_set(ast, ast_name(ast_child(ast)), ast, SYM_DEFINED);
      break;

    case TK_LET:
    case TK_VAR:
    {
      ast_t* scope = ast_parent(ast);
      ast_t* id = ast_child(ast);
      ast_set(scope, ast_name(id), id, SYM_DEFINED);
      break;
    }

    default: {}
  }

  return AST_OK;
}


#define BODY_ERROR  (ast_t*)1


// Resolve the field delegate body to use for the given method, if any.
// Return the body to use, NULL if none found or BODY_ERROR on error.
static ast_t* resolve_delegate_body(ast_t* entity, ast_t* method,
  method_t* info, const char* name)
{
  assert(entity != NULL);
  assert(method != NULL);
  assert(info != NULL);
  assert(name != NULL);

  if(info->delegate_field_2 != NULL)
  {
    // Ambiguous delegate
    assert(info->delegate_field_1 != NULL);
    assert(info->delegate_target_1 != NULL);
    assert(info->delegate_target_2 != NULL);

    ast_error(entity,
      "clashing delegates for method %s, local disambiguation required", name);
    ast_error(info->delegate_field_1, "field %s delegates to %s via %s",
      ast_name(ast_child(info->delegate_field_1)), name,
      ast_name(ast_child(info->delegate_target_1)));
    ast_error(info->delegate_field_2, "field %s delegates to %s via %s",
      ast_name(ast_child(info->delegate_field_2)), name,
      ast_name(ast_child(info->delegate_target_2)));

    return BODY_ERROR;
  }

  if(info->delegate_field_1 == NULL)  // No delegation needed
    return NULL;

  // We have a delegation, make a redirection body
  const char* field_name = ast_name(ast_child(info->delegate_field_1));
  ast_t* args = ast_from(info->delegate_field_1, TK_NONE);
  ast_t* last_arg = NULL;

  AST_GET_CHILDREN(method, cap, id, t_params, params, result, error, old_body);

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    const char* param_name = ast_name(ast_child(p));

    BUILD(arg, info->delegate_field_1,
      NODE(TK_SEQ,
        NODE(TK_CONSUME,
          NONE
          NODE(TK_REFERENCE, ID(param_name)))));

    ast_list_append(args, &last_arg, arg);
    ast_setid(args, TK_POSITIONALARGS);
  }

  BUILD(body, info->delegate_field_1,
    NODE(TK_SEQ,
      NODE(TK_CALL,
        TREE(args)    // Positional args
        NODE(TK_NONE) // Named args
        NODE(TK_DOT,  // Receiver
          NODE(TK_REFERENCE, ID(field_name))
          ID(name)))));

  if(is_none(result))
  {
    // Add None to end of body. Whilst the call generated above will return
    // None anyway in this case, without this extra None testing is very hard
    // since a directly written version of this body will have the None.
    BUILD(none, info->delegate_field_1,
      NODE(TK_REFERENCE, ID("None")));

    ast_append(body, none);
  }

  info->body_donor = entity;
  return body;
}


// Resolve the default body to use for the given method, if any.
// Return the body to use, NULL if none found or BODY_ERROR on error.
static ast_t* resolve_default_body(ast_t* entity, ast_t* method,
  method_t* info, const char* name, bool concrete)
{
  assert(entity != NULL);
  assert(method != NULL);
  assert(info != NULL);
  assert(name != NULL);

  if(info->default_body_src_2 != NULL)
  {
    // Ambiguous body, store info for use by any types that provide this one
    assert(info->default_body_src_1 != NULL);

    ast_t* err = ast_childidx(method, 5);
    ast_t* body = ast_childidx(method, 6);

    // Ditch whatever body we have, if any
    ast_erase(body);

    ast_setdata(body, info->default_body_src_1);
    ast_setdata(err, info->default_body_src_2);

    if(!concrete)
      return NULL;

    ast_error(entity, "multiple possible bodies for method %s, local "
      "disambiguation required", name);
    ast_error(info->default_body_src_1, "one possible body here");
    ast_error(info->default_body_src_2, "another possible body here");
    return BODY_ERROR;
  }

  if(info->default_body_src_1 == NULL)  // No default body found
    return NULL;

  // We have a default body, use it
  assert(info->reified_default != NULL);
  info->body_donor = (ast_t*)ast_data(info->default_body_src_1);
  return ast_childidx(info->reified_default, 6);
}


// Determine which body to use for the given method
static bool resolve_body(ast_t* entity, ast_t* method, pass_opt_t* options)
{
  assert(entity != NULL);
  assert(method != NULL);

  method_t* info = (method_t*)ast_data(method);
  assert(info != NULL);

  if(info->local_def) // Local defs just use their own body
    return true;

  token_id e_id = ast_id(entity);
  bool concrete =
    (e_id == TK_PRIMITIVE) || (e_id == TK_STRUCT) ||
    (e_id == TK_CLASS) || (e_id == TK_ACTOR);

  const char* name = ast_name(ast_childidx(method, 1));
  assert(name != NULL);

  // NExt try a delegate body
  ast_t* r = resolve_delegate_body(entity, method, info, name);

  if(r == BODY_ERROR)
    return false;

  if(r == NULL) // And finally a default body
    r = resolve_default_body(entity, method, info, name, concrete);

  if(r == BODY_ERROR)
    return false;

  if(r != NULL)
  {
    // We have a body, use it and patch up symbol tables
    ast_t* old_body = ast_childidx(method, 6);
    ast_replace(&old_body, r);
    ast_visit(&method, rescope, NULL, options, PASS_ALL);
    return true;
  }

  // Nowhere left to get a body from
  if(concrete)
  {
    ast_error(entity, "no body found for method %s", name);
    ast_error(method, "provided from here");
    return false;
  }

  return true;
}


// Resolve all methods in the given type.
// Stage 4.
static bool resolve_methods(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);

  ast_t* members = ast_childidx(ast, 4);
  assert(members != NULL);

  bool r = true;

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p) && !resolve_body(ast, p, options))
      r = false;
  }

  return r;
}


// Add provided methods to the given entity
static bool trait_entity(ast_t* entity, pass_opt_t* options)
{
  assert(entity != NULL);

  int state = ast_checkflag(entity,
    AST_FLAG_RECURSE_1 | AST_FLAG_DONE_1 | AST_FLAG_ERROR_1);

  // Check for recursive definitions
  switch(state)
  {
    case 0:
      ast_setflag(entity, AST_FLAG_RECURSE_1);
      break;

    case AST_FLAG_RECURSE_1:
      ast_error(entity, "traits and interfaces can't be recursive");
      ast_clearflag(entity, AST_FLAG_RECURSE_1);
      ast_setflag(entity, AST_FLAG_ERROR_1);
      return false;

    case AST_FLAG_DONE_1:
      return true;

    case AST_FLAG_ERROR_1:
      return false;

    default:
      assert(0);
      return false;
  }

  setup_local_methods(entity);

  bool r =
    provides_list(entity, options) && // Stage 1
    provided_methods(options, entity) && // Stage 2
    field_delegations(entity) && // Stage 3
    resolve_methods(entity, options); // Stage 4

  tidy_up(entity);
  ast_clearflag(entity, AST_FLAG_RECURSE_1);
  ast_setflag(entity, AST_FLAG_DONE_1);

  return r;
}


// Check that embed fields are not recursive.
static bool embed_fields(ast_t* entity, pass_opt_t* options)
{
  assert(entity != NULL);

  int state = ast_checkflag(entity,
    AST_FLAG_RECURSE_2 | AST_FLAG_DONE_2 | AST_FLAG_ERROR_2);

  // Check for recursive embeds
  switch(state)
  {
    case 0:
      ast_setflag(entity, AST_FLAG_RECURSE_2);
      break;

    case AST_FLAG_RECURSE_2:
      ast_error(entity, "embedded fields can't be recursive");
      ast_clearflag(entity, AST_FLAG_RECURSE_2);
      ast_setflag(entity, AST_FLAG_ERROR_2);
      return false;

    case AST_FLAG_DONE_2:
      return true;

    case AST_FLAG_ERROR_2:
      return false;

    default:
      assert(0);
      return false;
  }

  AST_GET_CHILDREN(entity, id, typeparams, cap, provides, members);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if(ast_id(member) == TK_EMBED)
    {
      AST_GET_CHILDREN(member, f_id, f_type);
      ast_t* def = (ast_t*)ast_data(f_type);
      assert(def != NULL);

      if(!embed_fields(def, options))
        return false;
    }

    member = ast_sibling(member);
  }

  ast_clearflag(entity, AST_FLAG_RECURSE_2);
  ast_setflag(entity, AST_FLAG_DONE_2);
  return true;
}


// Setup the type, or lack thereof, for local variable declarations.
// This is not really anything to do with traits, but must be done before the
// expr pass (to allow initialisation references to the variable type) but
// after the name pass (to get temporal capabilities).
static void local_types(ast_t* ast)
{
  assert(ast != NULL);

  // Setup type or mark as inferred now to allow calling create on a
  // non-inferred local to initialise itself
  AST_GET_CHILDREN(ast, id, type);
  assert(type != NULL);

  if(ast_id(type) == TK_NONE)
    type = ast_from(id, TK_INFERTYPE);

  ast_settype(id, type);
  ast_settype(ast, type);
}


// Add eq() and ne() functions to the given entity.
static bool add_comparable(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);

  AST_GET_CHILDREN(ast, id, typeparams, defcap, traits, members);
  ast_t* typeargs = ast_from(typeparams, TK_NONE);
  bool r = true;

  for(ast_t* p = ast_child(typeparams); p != NULL; p = ast_sibling(p))
  {
    ast_t* p_id = ast_child(p);

    BUILD_NO_DEBUG(type, p_id,
      NODE(TK_NOMINAL, NONE TREE(p_id) NONE NONE NONE));

    ast_append(typeargs, type);
    ast_setid(typeargs, TK_TYPEARGS);
  }

  if(!has_member(members, "eq"))
  {
    BUILD_NO_DEBUG(eq, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("eq")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            NODE(TK_NOMINAL, NONE TREE(id) TREE(typeargs) NONE NONE)
            NONE))
        NODE(TK_NOMINAL, NONE ID("Bool") NONE NONE NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_IS,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE
        NONE));

    // Need to set function data field to point to originating type, ie ast.
    // This won't be done when we catch up the passes since we've already
    // processed that type.
    ast_setdata(eq, ast);
    ast_append(members, eq);
    ast_set(ast, stringtab("eq"), eq, SYM_DEFINED);

    if(!ast_passes_subtree(&eq, options, PASS_TRAITS))
      r = false;
  }

  if(!has_member(members, "ne"))
  {
    BUILD_NO_DEBUG(ne, members,
      NODE(TK_FUN, AST_SCOPE
        NODE(TK_BOX)
        ID("ne")
        NONE
        NODE(TK_PARAMS,
          NODE(TK_PARAM,
            ID("that")
            NODE(TK_NOMINAL, NONE TREE(id) TREE(typeargs) NONE NONE)
            NONE))
        NODE(TK_NOMINAL, NONE ID("Bool") NONE NONE NONE)
        NONE
        NODE(TK_SEQ,
          NODE(TK_ISNT,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE
        NONE));

    // Need to set function data field to point to originating type, ie ast.
    // This won't be done when we catch up the passes since we've already
    // processed that type.
    ast_setdata(ne, ast);
    ast_append(members, ne);
    ast_set(ast, stringtab("ne"), ne, SYM_DEFINED);

    if(!ast_passes_subtree(&ne, options, PASS_TRAITS))
      r = false;
  }

  ast_free_unattached(typeargs);
  return r;
}


ast_result_t pass_traits(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      if(!trait_entity(ast, options))
        return AST_ERROR;

      if(!embed_fields(ast, options))
        return AST_ERROR;
      break;

    case TK_PRIMITIVE:
      if(!trait_entity(ast, options))
        return AST_ERROR;

      if(!add_comparable(ast, options))
        return AST_FATAL;
      break;

    case TK_INTERFACE:
    case TK_TRAIT:
      if(!trait_entity(ast, options))
        return AST_ERROR;
      break;

    case TK_LET:
    case TK_VAR:
      local_types(ast);
      break;

    default:
      break;
  }

  return AST_OK;
}
