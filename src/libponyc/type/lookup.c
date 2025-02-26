#include "lookup.h"
#include "assemble.h"
#include "cap.h"
#include "reify.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../ast/id.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../expr/literal.h"
#include "ponyassert.h"
#include <string.h>

static deferred_reification_t* lookup_base(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private);

// If a box method is being called with an iso/trn receiver, we mustn't replace
// this-> by iso/trn as this would be unsound, but by ref. See #1887
//
// This method (recursively) replaces occurences of iso and trn in `receiver` by
// ref. If a modification was required then a copy is returned, otherwise the
// original pointer is.
static ast_t* downcast_iso_trn_receiver_to_ref(ast_t* receiver) {
  switch (ast_id(receiver))
  {
    case TK_NOMINAL:
    case TK_TYPEPARAMREF:
      switch (cap_single(receiver))
      {
        case TK_TRN:
        case TK_ISO:
          return set_cap_and_ephemeral(receiver, TK_REF, TK_NONE);

        default:
          return receiver;
      }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(receiver, left, right);

      ast_t* downcasted_right = downcast_iso_trn_receiver_to_ref(right);
      if(right != downcasted_right)
        return viewpoint_type(left, downcasted_right);
      else
        return receiver;
    }

    default:
      pony_assert(0);
      return NULL;
  }
}

static deferred_reification_t* lookup_nominal(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private)
{
  pony_assert(ast_id(type) == TK_NOMINAL);

  ast_t* def = (ast_t*)ast_data(type);
  AST_GET_CHILDREN(def, type_id, typeparams);
  const char* type_name = ast_name(type_id);

  if(is_name_private(type_name) && (from != NULL) && (opt != NULL)
    && !allow_private)
  {
    typecheck_t* t = &opt->check;

    if(ast_nearest(def, TK_PACKAGE) != t->frame->package)
    {
      if(errors)
      {
        ast_error(opt->check.errors, from, "can't lookup fields or methods "
          "on private types from other packages");
      }

      return NULL;
    }
  }

  ast_t* find = ast_get(def, name, NULL);

  if(find != NULL)
  {
    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Typecheck default args immediately.
        if(opt != NULL)
        {
          AST_GET_CHILDREN(find, cap, id, typeparams, params);
          ast_t* param = ast_child(params);

          while(param != NULL)
          {
            AST_GET_CHILDREN(param, name, type, def_arg);

            if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL))
            {
              ast_t* child = ast_child(def_arg);

              if(ast_id(child) == TK_CALL)
                ast_settype(child, ast_from(child, TK_INFERTYPE));

              if(ast_visit_scope(&param, pass_pre_expr, pass_expr, opt,
                PASS_EXPR) != AST_OK)
                return NULL;

              def_arg = ast_childidx(param, 2);

              if(!coerce_literals(&def_arg, type, opt))
                return NULL;
            }

            param = ast_sibling(param);
          }
        }
        break;
      }

      default:
        find = NULL;
    }
  }

  if(find == NULL)
  {
    if(errors)
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'", name,
        type_name);

    return NULL;
  }

  if(is_name_private(name) && (from != NULL) && (opt != NULL) && !allow_private)
  {
    typecheck_t* t = &opt->check;

    switch(ast_id(find))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(t->frame->type != def)
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup private fields from outside the type");
          }

          return NULL;
        }
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Given that we can pick up method bodies from default methods on
        // traits, we need to allow the lookup of private methods from within
        // the same package.
        // We do this by getting the method we are calling from:
        //
        // t->frame->method
        //
        // And getting its body donor.
        // If the body_donor is a trait then check if that trait is in the same
        // package the method we are trying to lookup is defined in.
        //
        // If the method we are looking up is in the same package as the trait
        // that we got our method body from then we can allow the lookup.
        bool skip_check = false;
        ast_t* body_donor = (ast_t*)ast_data(t->frame->method);
        if ((body_donor != NULL) && (ast_id(body_donor) == TK_TRAIT) && (ast_nearest(find, TK_PACKAGE) == ast_nearest(body_donor, TK_PACKAGE)))
           skip_check = true;

        if(!skip_check && (ast_nearest(def, TK_PACKAGE) != t->frame->package))
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup private methods from outside the package");
          }

          return NULL;
        }
        break;
      }

      default:
        pony_assert(0);
        return NULL;
    }

    if(name == stringtab("_final"))
    {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          if(errors)
            ast_error(opt->check.errors, from,
              "can't lookup a _final function");

          return NULL;

        default: {}
      }
    } else if((name == stringtab("_init")) && (ast_id(def) == TK_PRIMITIVE)) {
      switch(ast_id(find))
      {
        case TK_NEW:
        case TK_BE:
        case TK_FUN:
          break;

        default:
          pony_assert(0);
      }

      if(errors)
        ast_error(opt->check.errors, from,
          "can't lookup an _init function on a primitive");

      return NULL;
    }
  }

  ast_t* typeargs = ast_childidx(type, 2);

  ast_t* orig_initial = orig;
  if(ast_id(find) == TK_FUN && ast_id(ast_child(find)) == TK_BOX)
    orig = downcast_iso_trn_receiver_to_ref(orig);

  deferred_reification_t* reified = deferred_reify_new(find, typeparams,
    typeargs, orig);

  // free if we made a copy of orig
  if(orig != orig_initial)
    ast_free(orig);

  return reified;
}

static deferred_reification_t* lookup_typeparam(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private)
{
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* constraint = ast_childidx(def, 1);
  ast_t* constraint_def = (ast_t*)ast_data(constraint);

  if(def == constraint_def)
  {
    if(errors)
    {
      ast_t* type_id = ast_child(def);
      const char* type_name = ast_name(type_id);
      ast_error(opt->check.errors, from, "couldn't find '%s' in '%s'",
        name, type_name);
    }

    return NULL;
  }

  // Lookup on the constraint instead.
  return lookup_base(opt, from, orig, constraint, name, errors, allow_private);
}

static bool param_names_match(ast_t* from, ast_t* prev_fun, ast_t* cur_fun,
  const char* name, errorframe_t* pframe, pass_opt_t* opt)
{
  ast_t* parent = from != NULL ? ast_parent(from) : NULL;
  if(parent != NULL && ast_id(parent) == TK_CALL)
  {
    AST_GET_CHILDREN(parent, receiver, positional, namedargs);
    if(namedargs != NULL && ast_id(namedargs) == TK_NAMEDARGS)
    {
      AST_GET_CHILDREN(prev_fun, prev_cap, prev_name, prev_tparams,
        prev_params);
      AST_GET_CHILDREN(cur_fun, cur_cap, cur_name, cur_tparams, cur_params);

      ast_t* prev_param = ast_child(prev_params);
      ast_t* cur_param = ast_child(cur_params);

      while(prev_param != NULL && cur_param != NULL)
      {
        AST_GET_CHILDREN(prev_param, prev_id);
        AST_GET_CHILDREN(cur_param, cur_id);

        if(ast_name(prev_id) != ast_name(cur_id))
        {
          if(pframe != NULL)
          {
            errorframe_t frame = NULL;
            ast_error_frame(&frame, from, "the '%s' methods of this union"
              " type have different parameter names; this prevents their"
              " use as named arguments", name);
            errorframe_append(&frame, pframe);
            errorframe_report(&frame, opt->check.errors);
          }
          return false;
        }

        prev_param = ast_sibling(prev_param);
        cur_param = ast_sibling(cur_param);
      }
    }
  }

  return true;
}

static deferred_reification_t* lookup_union(pass_opt_t* opt, ast_t* from,
  ast_t* type, const char* name, bool errors, bool allow_private)
{
  ast_t* child = ast_child(type);
  deferred_reification_t* result = NULL;
  ast_t* reified_result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    deferred_reification_t* r = lookup_base(opt, from, child, child, name,
      errors, allow_private);

    if(r == NULL)
    {
      // All possible types in the union must have this.
      if(errors)
      {
        ast_error(opt->check.errors, from, "couldn't find %s in %s",
          name, ast_print_type(child));
      }

      ok = false;
    } else {
      switch(ast_id(r->ast))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup field %s in %s in a union type",
              name, ast_print_type(child));
          }

          deferred_reify_free(r);
          ok = false;
          break;

        default:
        {
          errorframe_t frame = NULL;
          errorframe_t* pframe = errors ? &frame : NULL;

          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else {
            if(reified_result == NULL)
              reified_result = deferred_reify_method_def(result, result->ast,
                opt);

            ast_t* reified_r = deferred_reify_method_def(r, r->ast, opt);

            if(!is_subtype_fun(reified_r, reified_result, pframe, opt))
            {
              if(is_subtype_fun(reified_result, reified_r, pframe, opt))
              {
                // Use the supertype function. Require the most specific
                // arguments and return the least specific result.
                if(!param_names_match(from, reified_result, reified_r, name,
                  pframe, opt))
                {
                  ast_free_unattached(reified_r);
                  deferred_reify_free(r);
                  ok = false;
                } else {
                  if(errors)
                    errorframe_discard(pframe);

                  ast_free_unattached(reified_result);
                  deferred_reify_free(result);
                  reified_result = reified_r;
                  result = r;
                }
              } else {
                if(errors)
                {
                  errorframe_t frame = NULL;
                  ast_error_frame(&frame, from,
                    "a member of the union type has an incompatible method "
                    "signature");
                  errorframe_append(&frame, pframe);
                  errorframe_report(&frame, opt->check.errors);
                }

                ast_free_unattached(reified_r);
                deferred_reify_free(r);
                ok = false;
              }
            } else {
              if(!param_names_match(from, reified_result, reified_r, name,
                pframe, opt))
                ok = false;

              ast_free_unattached(reified_r);
              deferred_reify_free(r);
            }
          }
          break;
        }
      }
    }

    child = ast_sibling(child);
  }

  if(reified_result != NULL)
    ast_free_unattached(reified_result);

  if(!ok)
  {
    deferred_reify_free(result);
    result = NULL;
  }

  return result;
}

static deferred_reification_t* lookup_isect(pass_opt_t* opt, ast_t* from,
  ast_t* type, const char* name, bool errors, bool allow_private)
{
  ast_t* child = ast_child(type);
  deferred_reification_t* result = NULL;
  ast_t* reified_result = NULL;
  bool ok = true;

  while(child != NULL)
  {
    deferred_reification_t* r = lookup_base(opt, from, child, child, name,
      false, allow_private);

    if(r != NULL)
    {
      switch(ast_id(r->ast))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          // Ignore fields.
          deferred_reify_free(r);
          break;

        default:
          if(result == NULL)
          {
            // If we don't have a result yet, use this one.
            result = r;
          } else {
            if(reified_result == NULL)
              reified_result = deferred_reify_method_def(result, result->ast,
                opt);

            ast_t* reified_r = deferred_reify_method_def(r, r->ast, opt);
            bool changed = false;

            if(!is_subtype_fun(reified_result, reified_r, NULL, opt))
            {
              if(is_subtype_fun(reified_r, reified_result, NULL, opt))
              {
                // Use the subtype function. Require the least specific
                // arguments and return the most specific result.
                ast_free_unattached(reified_result);
                deferred_reify_free(result);
                reified_result = reified_r;
                result = r;
                changed = true;
              }

              // TODO: isect the signatures, to handle arg names and
              // default arguments. This is done even when the functions have
              // no subtype relationship.
            }

            if(!changed)
            {
              ast_free_unattached(reified_r);
              deferred_reify_free(r);
            }
          }
          break;
      }
    }

    child = ast_sibling(child);
  }

  if(errors && (result == NULL))
    ast_error(opt->check.errors, from, "couldn't find '%s'", name);

  if(reified_result != NULL)
    ast_free_unattached(reified_result);

  if(!ok)
  {
    deferred_reify_free(result);
    result = NULL;
  }

  return result;
}

static deferred_reification_t* lookup_base(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
      return lookup_union(opt, from, type, name, errors, allow_private);

    case TK_ISECTTYPE:
      return lookup_isect(opt, from, type, name, errors, allow_private);

    case TK_TUPLETYPE:
      if(errors)
        ast_error(opt->check.errors, from, "can't lookup by name on a tuple");

      return NULL;

    case TK_DONTCARETYPE:
      if(errors)
        ast_error(opt->check.errors, from, "can't lookup by name on '_'");

      return NULL;

    case TK_NOMINAL:
      return lookup_nominal(opt, from, orig, type, name, errors, allow_private);

    case TK_ARROW:
      return lookup_base(opt, from, orig, ast_childidx(type, 1), name, errors,
        allow_private);

    case TK_TYPEPARAMREF:
      return lookup_typeparam(opt, from, orig, type, name, errors,
        allow_private);

    case TK_FUNTYPE:
      if(errors)
        ast_error(opt->check.errors, from,
          "can't lookup by name on a function type");

      return NULL;

    case TK_INFERTYPE:
    case TK_ERRORTYPE:
      // Can only happen due to a local inference fail earlier
      return NULL;

    default: {}
  }

  pony_assert(0);
  return NULL;
}

deferred_reification_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name)
{
  return lookup_base(opt, from, type, type, name, true, false);
}

deferred_reification_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool allow_private)
{
  return lookup_base(opt, from, type, type, name, false, allow_private);
}
