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


/** The following defines how we determine the signature and body to use for
 * some method M in type T.
 * Note that the flatten pass will already have ensured that provides and
 * delegates types contain only traits and interfaces, and converted those to
 * lists.
 *
 * 1. Explicit.
 *    If T provides an explicit definition of M we use that, including the body
 *    defined. If T provides a signature and no body, then T.M has no body,
 *    even if a body is available from some other source.
 * 2. Delegation.
 *    If T does not define M explicitly then we look at delegation.
 *    If exactly one field delegates to M then T gets an M with the signature
 *    as used in the delegation type.
 *    More than one field delegating to M is an error.
 *    Note that we don't delegate constructors.
 * 3. Compatible inheritance.
 *    If T gets an M from either (1) or (2) then we use "compatible"
 *    inheritance.
 *    Each M from T's provides list must be a supertype of T.M.
 *    Any default bodies of M from the provides list are ignored, even if T.M
 *    has no body.
 * 4. Identical inheritance.
 *    If T does NOT get an M from either (1) or (2) then we use "identical"
 *    inheritance.
 *    All Ms from T's provides list must have identical signatures.
 *    If exactly one default body is provided for M then T.M uses that.
 *    If multiple default bodies are provided for M then T.M is body ambiguous.
 *    If any types in T's provides list are body ambiguous for M then T.M is
 *    body ambiguous.
 * 5. Required body.
 *    If T is a concrete type then M must have a body. It is an error if it has
 *    no body or is body ambiguous.
 *    If T is a trait or interface then it is fine for M to have no body or be
 *    body ambiguous.
 *
 * Throughout the processing of type T each method in T has a method_t struct
 * which is stored in the data field of the method node. Some of the
 * information stored is only used to generate helpful error messages.
 *
 * Once processing of T is finished these are freed and the data field is set
 * to the body donor, ie the type in which the body actually used was defined.
 * This can be used in later passes to determine the source of a method body.
 * If no body is used (only in traits and interfaces) the donor will be set to
 * the containing type.
 */

typedef struct method_t
{
  ast_t* body_donor;  // Type body was defined in. NULL if we have no body.
  ast_t* delegated_field;
  ast_t* trait_ref;
  bool local_define;
  bool failed;
} method_t;


static bool trait_entity(ast_t* entity, pass_opt_t* options);


// Report whether the given node is a field.
static bool is_field(ast_t* ast)
{
  assert(ast != NULL);

  token_id variety = ast_id(ast);
  return (variety == TK_FVAR) || (variety == TK_FLET) || (variety == TK_EMBED);
}


// Report whether the given node is a method.
static bool is_method(ast_t* ast)
{
  assert(ast != NULL);

  token_id variety = ast_id(ast);
  return (variety == TK_BE) || (variety == TK_FUN) || (variety == TK_NEW);
}


// Attach a new method_t structure to the given method.
static method_t* attach_method_t(ast_t* method)
{
  assert(method != NULL);

  method_t* p = POOL_ALLOC(method_t);
  p->body_donor = NULL;
  p->delegated_field = NULL;
  p->trait_ref = NULL;
  p->local_define = false;
  p->failed = false;

  ast_setdata(method, p);
  return p;
}


// Setup a method_t structure for each method in the given type.
static void setup_local_methods(ast_t* ast)
{
  assert(ast != NULL);

  ast_t* members = ast_childidx(ast, 4);
  assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      method_t* info = attach_method_t(p);
      info->local_define = true;

      if(ast_id(ast_childidx(p, 6)) != TK_NONE)
        info->body_donor = ast;
    }
  }
}


// Tidy up the method_t structures in the given entity.
static void tidy_up(ast_t* entity)
{
  assert(entity != NULL);

  ast_t* members = ast_childidx(entity, 4);
  assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      method_t* info = (method_t*)ast_data(p);
      assert(info != NULL);
      ast_t* body_donor = info->body_donor;
      POOL_FREE(method_t, info);

      if(body_donor == NULL)
        // No body, donor should indicate containing type.
        body_donor = entity;

      ast_setdata(p, body_donor);
    }
  }
}


// Compare the 2 given signatures to see if they are exactly the same.
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
      // Check everything except body and docstring, ie first 6 children.
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
      // Can't just use strcmp, string literals may contain \0s.
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


// Reify the method with the type parameters from trait definition
// and type arguments from trait reference.
// Also handles modifying return types from behaviours, etc.
// Returns the reified type, which must be freed later, or NULL on error.
static ast_t* reify_provides_type(ast_t* entity, ast_t* method,
  ast_t* trait_ref, pass_opt_t* opt)
{
  assert(method != NULL);
  assert(trait_ref != NULL);

  // Apply the type args (if any) from the trait reference to the type
  // parameters from the trait definition.
  ast_t* trait_def = (ast_t*)ast_data(trait_ref);
  assert(trait_def != NULL);
  ast_t* type_args = ast_childidx(trait_ref, 2);
  ast_t* type_params = ast_childidx(trait_def, 1);

  if(!reify_defaults(type_params, type_args, true, opt))
    return NULL;

  ast_t* reified = reify(method, type_params, type_args, opt);

  if(reified == NULL)
    return NULL;

  AST_GET_CHILDREN(reified, cap, id, typeparams, params, result,
    can_error, body, doc);

  if(ast_id(reified) == TK_BE || ast_id(reified) == TK_NEW)
  {
    // Modify return type to the inheriting type.
    ast_t* this_type = type_for_this(opt, entity, ast_id(cap),
      TK_EPHEMERAL, true);

    ast_replace(&result, this_type);
  }

  return reified;
}


// Find the method with the specified name in the given entity.
// Find only methods, ignore fields.
// Return the method with the specified name or NULL if not found.
static ast_t* find_method(ast_t* entity, const char* name)
{
  assert(entity != NULL);
  assert(name != NULL);

  ast_t* method = ast_get(entity, name, NULL);

  if(method == NULL)
    return NULL;

  if(is_method(method))
    return method;

  return NULL;
}


// Add a new method to the given entity, based on the specified method from
// the specified type.
// The trait_ref is the entry in the provides / delegates list that causes this
// method inclusion. Needed for error reporting.
// The basis_method is the reified method in the trait to add.
// The adjective parameter is used for error reporting and should be "provided"
// or similar.
// Return the newly added method or NULL on error.
static ast_t* add_method(ast_t* entity, ast_t* trait_ref, ast_t* basis_method,
  const char* adjective, pass_opt_t* opt)
{
  assert(entity != NULL);
  assert(trait_ref != NULL);
  assert(basis_method != NULL);
  assert(adjective != NULL);

  const char* name = ast_name(ast_childidx(basis_method, 1));

  // Check behaviour compatability.
  if(ast_id(basis_method) == TK_BE)
  {
    switch(ast_id(entity))
    {
      case TK_PRIMITIVE:
        ast_error(opt->check.errors, trait_ref,
          "cannot add a behaviour (%s) to a primitive", name);
        return NULL;

      case TK_STRUCT:
        ast_error(opt->check.errors, trait_ref,
          "cannot add a behaviour (%s) to a struct", name);
        return NULL;

      case TK_CLASS:
        ast_error(opt->check.errors, trait_ref,
          "cannot add a behaviour (%s) to a class", name);
        return NULL;

      default:
        break;
    }
  }

  // Check for existing method of the same name.
  ast_t* existing = ast_get(entity, name, NULL);

  if(existing != NULL)
  {
    assert(is_field(existing)); // Should already have checked for methods.

    ast_error(opt->check.errors, trait_ref,
      "%s method '%s' clashes with field", adjective, name);
    ast_error_continue(opt->check.errors, basis_method,
      "method is defined here");
    return NULL;
  }

  // Check for clash with existing method.
  ast_t* case_clash = ast_get_case(entity, name, NULL);

  if(case_clash != NULL)
  {
    const char* clash_name = "";

    switch(ast_id(case_clash))
    {
      case TK_FUN:
      case TK_BE:
      case TK_NEW:
        clash_name = ast_name(ast_childidx(case_clash, 1));
        break;

      case TK_LET:
      case TK_VAR:
      case TK_EMBED:
        clash_name = ast_name(ast_child(case_clash));
        break;

      default:
        assert(0);
        break;
    }

    ast_error(opt->check.errors, trait_ref,
      "%s method '%s' differs only in case from '%s'",
      adjective, name, clash_name);
    ast_error_continue(opt->check.errors, basis_method,
      "clashing method is defined here");
    return NULL;
  }

  AST_GET_CHILDREN(basis_method, cap, id, typeparams, params, result,
    can_error, body, doc);

  // Ignore docstring.
  if(ast_id(doc) == TK_STRING)
  {
    ast_set_name(doc, "");
    ast_setid(doc, TK_NONE);
  }

  ast_t* local = ast_append(ast_childidx(entity, 4), basis_method);
  ast_set(entity, name, local, SYM_DEFINED);
  ast_t* body_donor = (ast_t*)ast_data(basis_method);
  method_t* info = attach_method_t(local);
  info->trait_ref = trait_ref;

  if(ast_id(body) != TK_NONE)
    info->body_donor = body_donor;

  return local;
}


// Sort out symbol table for copied method body.
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
    {
      assert(ast_child(ast) != NULL);
      ast_set(ast, ast_name(ast_child(ast)), ast, SYM_DEFINED);
      break;
    }

    case TK_LET:
    case TK_VAR:
    {
      ast_t* scope = ast_parent(ast);
      ast_t* id = ast_child(ast);
      ast_set(scope, ast_name(id), id, SYM_UNDEFINED);
      break;
    }

    case TK_MATCH_CAPTURE:
    {
      ast_t* scope = ast_parent(ast);
      ast_t* id = ast_child(ast);
      ast_set(scope, ast_name(id), id, SYM_DEFINED);
      break;
    }

    case TK_TYPEPARAMREF:
    {
      assert(ast_child(ast) != NULL);
      ast_t* def = ast_get(ast, ast_name(ast_child(ast)), NULL);
      ast_setdata(ast, def);
      break;
    }

    default: {}
  }

  return AST_OK;
}


// Combine the given inherited method with the existing one, if any, in the
// given entity.
// The provided method must already be reified.
// The trait_ref is the entry in the provides list that causes this method
// inclusion. Needed for error reporting.
// Returns true on success, false on failure in which case an error will have
// been reported.
static bool add_method_from_trait(ast_t* entity, ast_t* method,
  ast_t* trait_ref, pass_opt_t* opt)
{
  assert(entity != NULL);
  assert(method != NULL);
  assert(trait_ref != NULL);

  AST_GET_CHILDREN(method, cap, id, t_params, params, result, error,
    method_body);

  const char* method_name = ast_name(id);
  ast_t* existing_method = find_method(entity, method_name);

  if(existing_method == NULL)
  {
    // We don't have a method yet with this name, add the one from this trait.
    ast_t* m = add_method(entity, trait_ref, method, "provided", opt);

    if(m == NULL)
      return false;

    if(ast_id(ast_childidx(m, 6)) != TK_NONE)
      ast_visit(&m, rescope, NULL, opt, PASS_ALL);

    return true;
  }

  // A method with this name already exists.
  method_t* info = (method_t*)ast_data(existing_method);
  assert(info != NULL);

  if(info->failed)  // Method has already caused an error, do nothing.
    return false;

  if(info->local_define || info->delegated_field != NULL)
  {
    // Method is local or provided by delegation. Provided method must be a
    // suitable supertype.
    errorframe_t err = NULL;
    if(is_subtype(existing_method, method, &err, opt))
      return true;

    if(info->local_define)
    {
      ast_error(opt->check.errors, trait_ref,
        "method '%s' provided here is not compatible with local version",
        method_name);
      ast_error_continue(opt->check.errors, method, "provided method");
      ast_error_continue(opt->check.errors, existing_method, "local method");
    }
    else
    {
      assert(info->delegated_field != NULL);
      assert(info->trait_ref != NULL);

      ast_error(opt->check.errors, trait_ref, "provided method '%s' is not "
        "compatible with delegated version, provided type: %s", method_name,
        ast_print_type(method));
      ast_error_continue(opt->check.errors, info->trait_ref,
        "method delegated from field '%s' has type: %s",
        ast_name(ast_child(info->delegated_field)),
        ast_print_type(existing_method));
    }

    info->failed = true;
    return false;
  }

  // Existing method is also provided, signatures must match exactly.
  if(!compare_signatures(existing_method, method))
  {
    assert(info->trait_ref != NULL);

    ast_error(opt->check.errors, trait_ref, "clashing definitions for "
      "method '%s' provided, local disambiguation required", method_name);
    ast_error_continue(opt->check.errors, trait_ref, "provided here, type: %s",
      ast_print_type(method));
    ast_error_continue(opt->check.errors, info->trait_ref, "and here, type: %s",
      ast_print_type(existing_method));

    info->failed = true;
    return false;
  }

  // Resolve bodies, if any.
  ast_t* existing_body = ast_childidx(existing_method, 6);

  bool multiple_bodies =
    (info->body_donor != NULL) &&
    (ast_id(method_body) != TK_NONE) &&
    (info->body_donor != (ast_t*)ast_data(method));

  if(multiple_bodies ||
    ast_checkflag(existing_method, AST_FLAG_AMBIGUOUS) ||
    ast_checkflag(method, AST_FLAG_AMBIGUOUS))
  {
    // This method body ambiguous, which is not necessarily an error.
    ast_setflag(existing_method, AST_FLAG_AMBIGUOUS);

    if(ast_id(existing_body) != TK_NONE) // Ditch existing body.
      ast_replace(&existing_body, ast_from(existing_method, TK_NONE));

    info->body_donor = NULL;
    return true;
  }

  if(ast_id(method_body) == TK_NONE ||
    info->body_donor == (ast_t*)ast_data(method))
    // No new body to resolve.
    return true;

  // Trait provides default body. Use it and patch up symbol tables.
  assert(ast_id(existing_body) == TK_NONE);
  ast_replace(&existing_body, method_body);
  ast_visit(&method_body, rescope, NULL, opt, PASS_ALL);

  info->body_donor = (ast_t*)ast_data(method);
  info->trait_ref = trait_ref;
  return true;
}


// Process the methods provided to the given entity from all traits in its
// provides list.
static bool provided_methods(ast_t* entity, pass_opt_t* opt)
{
  assert(entity != NULL);

  ast_t* provides = ast_childidx(entity, 3);
  bool r = true;

  // Run through our provides list
  for(ast_t* trait_ref = ast_child(provides); trait_ref != NULL;
    trait_ref = ast_sibling(trait_ref))
  {
    ast_t* trait = (ast_t*)ast_data(trait_ref);
    assert(trait != NULL);

    if(!trait_entity(trait, opt))
      return false;

    ast_t* members = ast_childidx(trait, 4);

    // Run through the methods of each provided type.
    for(ast_t* method = ast_child(members); method != NULL;
      method = ast_sibling(method))
    {
      assert(is_method(method));

      ast_t* reified = reify_provides_type(entity, method, trait_ref, opt);

      if(reified == NULL)
      {
        // Reification error, already reported.
        r = false;
      }
      else
      {
        if(!add_method_from_trait(entity, reified, trait_ref, opt))
          r = false;

        ast_free_unattached(reified);
      }
    }
  }

  return r;
}


// Convert the given method into a delegation indirection to the specified
// field.
static void make_delegation(ast_t* method, ast_t* field, ast_t* delegate_ref,
  ast_t* body_donor)
{
  assert(method != NULL);
  assert(field != NULL);
  assert(delegate_ref != NULL);

  // Make a redirection method body.
  ast_t* args = ast_from(delegate_ref, TK_NONE);
  ast_t* last_arg = NULL;

  AST_GET_CHILDREN(method, cap, id, t_params, params, result, error, old_body);

  for(ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    const char* param_name = ast_name(ast_child(p));

    BUILD(arg, delegate_ref,
      NODE(TK_SEQ,
        NODE(TK_CONSUME,
          NONE
          NODE(TK_REFERENCE, ID(param_name)))));

    ast_list_append(args, &last_arg, arg);
    ast_setid(args, TK_POSITIONALARGS);
  }

  BUILD(body, delegate_ref,
    NODE(TK_SEQ,
      NODE(TK_CALL,
        TREE(args)    // Positional args.
        NODE(TK_NONE) // Named args.
        NODE(TK_DOT,  // Receiver.
          NODE(TK_REFERENCE, ID(ast_name(ast_child(field))))
          ID(ast_name(ast_childidx(method, 1)))))));

  if(is_none(result))
  {
    // Add None to end of body. Whilst the call generated above will return
    // None anyway in this case, without this extra None testing is very hard
    // since a directly written version of this body will have the None.
    BUILD(none, delegate_ref, NODE(TK_REFERENCE, ID("None")));
    ast_append(body, none);
  }

  ast_replace(&old_body, body);

  // Setup method info.
  method_t* info = (method_t*)ast_data(method);
  assert(info != NULL);
  info->body_donor = body_donor;
  info->delegated_field = field;
}


// Combine the given delegated method with the existing one, if any, in the
// given entity.
// The trait_ref is the entry in the delegates list that causes this method
// inclusion. Needed for error reporting.
// Returns true on success, false on failure in which case an error will have
// been reported.
static bool delegated_method(ast_t* entity, ast_t* method, ast_t* field,
  ast_t* trait_ref, pass_opt_t* opt)
{
  assert(entity != NULL);
  assert(method != NULL);
  assert(is_method(method));
  assert(field != NULL);
  assert(trait_ref != NULL);

  if(ast_id(method) == TK_NEW)  // Don't delegate constructors.
    return true;

  // Check for existing method of the same name.
  const char* name = ast_name(ast_childidx(method, 1));
  assert(name != NULL);

  ast_t* existing = find_method(entity, name);

  if(existing != NULL)
  {
    // We already have a method with this name.
    method_t* info = (method_t*)ast_data(existing);
    assert(info != NULL);

    if(info->local_define || info->delegated_field == field)
      // Nothing new to add.
      return true;

    assert(info->trait_ref != NULL);

    ast_error(opt->check.errors, trait_ref,
      "clashing delegates for method %s, local disambiguation required", name);
    ast_error_continue(opt->check.errors, trait_ref,
      "field %s delegates to %s here", ast_name(ast_child(field)), name);
    ast_error_continue(opt->check.errors, info->trait_ref,
      "field %s delegates to %s here",
      ast_name(ast_child(info->delegated_field)), name);
    info->failed = true;
    info->delegated_field = NULL;
    return false;
  }

  // This is a new method, reify and add it.
  ast_t* reified = reify_provides_type(entity, method, trait_ref, opt);

  if(reified == NULL) // Reification error, already reported
    return false;

  ast_t* new_method = add_method(entity, trait_ref, reified, "delegate", opt);

  if(new_method == NULL)
  {
    ast_free_unattached(reified);
    return false;
  }

  // Convert the newly added method into a delegation redirection.
  make_delegation(new_method, field, trait_ref, entity);
  return true;
}


// Process the methods required for delegation to all the fields in the given
// entity.
static bool delegated_methods(ast_t* entity, pass_opt_t* opt)
{
  assert(entity != NULL);

  bool r = true;

  // Check all fields.
  for(ast_t* field = ast_child(ast_childidx(entity, 4)); field != NULL;
    field = ast_sibling(field))
  {
    if(is_field(field))
    {
      AST_GET_CHILDREN(field, id, f_type, value, delegates);

      // Check all delegates for field.
      for(ast_t* trait_ref = ast_child(delegates); trait_ref != NULL;
        trait_ref = ast_sibling(trait_ref))
      {
        ast_t* trait = (ast_t*)ast_data(trait_ref);
        assert(trait != NULL);

        if(!trait_entity(trait, opt))
          return false;

        errorframe_t err = NULL;
        if(!is_subtype(f_type, trait_ref, &err, opt))
        {
          ast_error(opt->check.errors, trait_ref,
            "field not a subtype of delegate");
          ast_error_continue(opt->check.errors, f_type,
            "field type: %s", ast_print_type(f_type));
          ast_error_continue(opt->check.errors, trait_ref,
            "delegate type: %s", ast_print_type(trait_ref));
          r = false;
        }
        else
        {
          // Run through the methods of each delegated type.
          for(ast_t* method = ast_child(ast_childidx(trait, 4));
            method != NULL; method = ast_sibling(method))
          {
            if(!delegated_method(entity, method, field, trait_ref, opt))
              r = false;
          }
        }
      }
    }
  }

  return r;
}


// Check that the given entity, if concrete, has bodies for all methods.
static bool check_concrete_bodies(ast_t* entity, pass_opt_t* opt)
{
  assert(entity != NULL);

  token_id variety = ast_id(entity);
  if((variety != TK_PRIMITIVE) && (variety != TK_STRUCT) &&
    (variety != TK_CLASS) && (variety != TK_ACTOR))
    return true;

  bool r = true;
  ast_t* members = ast_childidx(entity, 4);
  assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      method_t* info = (method_t*)ast_data(p);
      assert(info != NULL);

      if(!info->failed)
      {
        const char* name = ast_name(ast_childidx(p, 1));

        if(ast_checkflag(p, AST_FLAG_AMBIGUOUS))
        {
          // Concrete types must not have ambiguous bodies.
          ast_error(opt->check.errors, entity, "multiple possible bodies for "
            "method %s, local disambiguation required", name);
          r = false;
        }
        else if(info->body_donor == NULL)
        {
          // Concrete types must have method bodies.
          assert(info->trait_ref != NULL);
          ast_error(opt->check.errors, info->trait_ref,
            "no body found for method %s", name);
          r = false;
        }
      }
    }
  }

  return r;
}


// Add provided and delegated methods to the given entity.
static bool trait_entity(ast_t* entity, pass_opt_t* opt)
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
      ast_error(opt->check.errors, entity,
        "traits and interfaces can't be recursive");
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
    delegated_methods(entity, opt) &&
    provided_methods(entity, opt) &&
    check_concrete_bodies(entity, opt);

  tidy_up(entity);
  ast_clearflag(entity, AST_FLAG_RECURSE_1);
  ast_setflag(entity, AST_FLAG_DONE_1);

  return r;
}


// Check that embed fields are not recursive.
static bool embed_fields(ast_t* entity, pass_opt_t* opt)
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
      ast_error(opt->check.errors, entity,
        "embedded fields can't be recursive");
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

      if(!embed_fields(def, opt))
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

    BUILD(type, p_id, NODE(TK_NOMINAL, NONE TREE(p_id) NONE NONE NONE));
    ast_append(typeargs, type);
    ast_setid(typeargs, TK_TYPEARGS);
  }

  if(!has_member(members, "eq"))
  {
    BUILD(eq, members,
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
    BUILD(ne, members,
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
