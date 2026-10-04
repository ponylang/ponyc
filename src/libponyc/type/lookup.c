#include "lookup.h"
#include "assemble.h"
#include "cap.h"
#include "reify.h"
#include "typealias.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../expr/literal.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdlib.h>
#include <string.h>

static deferred_reification_t* lookup_base(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private);

// If a box method is being called with an iso/trn receiver, we mustn't replace
// this-> by iso/trn as this would be unsound, but by ref. See #1887
//
// This method (recursively) replaces occurences of iso and trn in `receiver` by
// ref. If a modification was required then a copy is returned, otherwise the
// original pointer is.
static ast_t* downcast_iso_trn_receiver_to_ref(ast_t* receiver, pass_opt_t* opt) {
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

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(receiver);

      if(unfolded == NULL)
        return receiver;

      ast_t* result = downcast_iso_trn_receiver_to_ref(unfolded, opt);

      if(result != unfolded)
        ast_free_unattached(unfolded);

      return result;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(receiver, left, right);

      ast_t* downcasted_right = downcast_iso_trn_receiver_to_ref(right, opt);
      if(right != downcasted_right)
        return viewpoint_type(left, downcasted_right, opt);
      else
        return receiver;
    }

    default:
      pony_assert(0);
      return NULL;
  }
}

static bool lookup_evaluate_guard(deferred_reification_t* fun, ast_t* guard,
  pass_opt_t* opt)
{
  switch(ast_id(guard))
  {
    case TK_IFTYPEGUARD:
    {
      AST_GET_CHILDREN(guard, subtype, supertype);
      ast_t* r_sub = deferred_reify(fun, subtype, opt);
      ast_t* r_super = deferred_reify(fun, supertype, opt);
      bool matches = is_subtype_constraint(r_sub, r_super, NULL, opt);
      ast_free_unattached(r_sub);
      ast_free_unattached(r_super);
      return matches;
    }

    case TK_IFTYPEGUARD_AND:
    {
      ast_t* child = ast_child(guard);
      while(child != NULL)
      {
        if(!lookup_evaluate_guard(fun, child, opt))
          return false;
        child = ast_sibling(child);
      }
      return true;
    }

    case TK_IFTYPEGUARD_OR:
    {
      ast_t* child = ast_child(guard);
      while(child != NULL)
      {
        if(lookup_evaluate_guard(fun, child, opt))
          return true;
        child = ast_sibling(child);
      }
      return false;
    }

    default:
      pony_assert(0);
      return false;
  }
}

static bool methodgroup_has_type_overloads(ast_t* group)
{
  pony_assert(ast_id(group) == TK_METHODGROUP);

  int unguarded_count = 0;
  ast_t* child = ast_child(group);

  while(child != NULL)
  {
    ast_t* guard = ast_childidx(child, 5);

    if(ast_id(guard) == TK_NONE)
      unguarded_count++;

    if(unguarded_count > 1)
      return true;

    child = ast_sibling(child);
  }

  return false;
}

static ast_t* lookup_select_specialization(ast_t* default_method,
  ast_t* typeparams, ast_t* typeargs, ast_t* thistype, pass_opt_t* opt)
{
  ast_t* parent = ast_parent(default_method);

  if((parent == NULL) || (ast_id(parent) != TK_METHODGROUP))
    return NULL;

  // Only evaluate guards when all type arguments are concrete.
  // When any type argument is still a type parameter reference, the guard
  // cannot be resolved — defer to codegen (reach.c) where concrete types
  // are known.
  ast_t* targ = ast_child(typeargs);
  while(targ != NULL)
  {
    if(ast_id(targ) == TK_TYPEPARAMREF)
      return NULL;
    targ = ast_sibling(targ);
  }

  deferred_reification_t* temp = deferred_reify_new(default_method,
    typeparams, typeargs, thistype);
  ast_t* spec = ast_sibling(default_method);
  ast_t* result = NULL;

  while(spec != NULL)
  {
    ast_t* guard = ast_childidx(spec, 5);

    if(lookup_evaluate_guard(temp, guard, opt))
    {
      result = spec;
      break;
    }

    spec = ast_sibling(spec);
  }

  deferred_reify_free(temp);
  return result;
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

      case TK_METHODGROUP:
      {
        if(methodgroup_has_type_overloads(find))
        {
          // Type overloads: return the group for call-site resolution.
          break;
        }

        ast_t* default_method = ast_child(find);
        ast_t* typeargs = ast_childidx(type, 2);

        if(opt != NULL)
        {
          ast_t* spec = lookup_select_specialization(default_method,
            typeparams, typeargs, orig, opt);
          find = (spec != NULL) ? spec : default_method;
        }
        else
        {
          find = default_method;
        }

        // fallthrough to typecheck default args
      }
      // fallthrough

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
    {
      if(type_name[0] == '$')
      {
        ast_error(opt->check.errors, from,
          "couldn't find '%s' in anonymous type", name);

        ast_t* members = ast_childidx(def, 4);
        ast_t* member = ast_child(members);

        while(member != NULL)
        {
          if((ast_id(member) == TK_FUN) || (ast_id(member) == TK_BE) ||
            (ast_id(member) == TK_METHODGROUP))
          {
            ast_t* m = (ast_id(member) == TK_METHODGROUP)
              ? ast_child(member) : member;
            ast_t* member_id = ast_childidx(m, 1);
            const char* member_name = ast_name(member_id);

            if(member_name[0] != '$' && member_name[0] != '_')
              ast_error_continue(opt->check.errors, member,
                "it has a method named '%s'", member_name);
          }

          member = ast_sibling(member);
        }
      } else {
        ast_error(opt->check.errors, from,
          "couldn't find '%s' in '%s'", name, type_name);
      }
    }

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

      case TK_METHODGROUP:
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

    if(name == stringtab(opt->strtab, "_final"))
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
    } else if((name == stringtab(opt->strtab, "_init")) && (ast_id(def) == TK_PRIMITIVE)) {
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
    orig = downcast_iso_trn_receiver_to_ref(orig, opt);

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
  deferred_reification_t* result =
    lookup_base(opt, from, orig, constraint, name, errors, allow_private);

  if(result != NULL && result->thistype != NULL)
  {
    // The constraint lookup may have set thistype to one member of an
    // intersection or union constraint. The actual receiver is the type
    // parameter itself, so replace thistype with orig (applying the same
    // iso/trn-to-ref downcast that lookup_nominal would).
    ast_t* thistype = ast_dup(orig);

    if(ast_id(result->ast) == TK_FUN &&
      ast_id(ast_child(result->ast)) == TK_BOX)
    {
      ast_t* downcast = downcast_iso_trn_receiver_to_ref(thistype, opt);
      if(downcast != thistype)
        ast_free(thistype);
      thistype = downcast;
    }

    ast_free(result->thistype);
    result->thistype = thistype;
  }

  return result;
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

static void methodgroup_collapse_to_unguarded(deferred_reification_t* r)
{
  pony_assert(ast_id(r->ast) == TK_METHODGROUP);

  ast_t* ug = ast_child(r->ast);
  while(ug != NULL)
  {
    if(ast_id(ast_childidx(ug, 5)) == TK_NONE)
      break;
    ug = ast_sibling(ug);
  }

  if(ug != NULL)
    r->ast = ug;
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
          name, ast_print_type(child, opt->strtab));
      }

      ok = false;
    } else {
      if(ast_id(r->ast) == TK_METHODGROUP)
        methodgroup_collapse_to_unguarded(r);

      switch(ast_id(r->ast))
      {
        case TK_FVAR:
        case TK_FLET:
        case TK_EMBED:
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup field %s in %s in a union type",
              name, ast_print_type(child, opt->strtab));
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
      if(ast_id(r->ast) == TK_METHODGROUP)
        methodgroup_collapse_to_unguarded(r);

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

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);

      if(unfolded == NULL)
        return NULL;

      deferred_reification_t* result = lookup_base(opt, from, orig, unfolded,
        name, errors, allow_private);
      ast_free_unattached(unfolded);
      return result;
    }

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

  // No other kind of type has members.
  pony_assert(!errors);
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

static void overload_encode_type(printbuf_t* buf, ast_t* type);

static void overload_encode_cap(printbuf_t* buf, ast_t* node)
{
  switch(ast_id(node))
  {
    case TK_ISO: printbuf(buf, "ci"); break;
    case TK_TRN: printbuf(buf, "ct"); break;
    case TK_REF: printbuf(buf, "cr"); break;
    case TK_VAL: printbuf(buf, "cv"); break;
    case TK_BOX: printbuf(buf, "cb"); break;
    case TK_TAG: printbuf(buf, "cg"); break;
    case TK_THISTYPE: printbuf(buf, "cs"); break;

    default:
      overload_encode_type(buf, node);
      break;
  }
}

static void overload_encode_type(printbuf_t* buf, ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, package, id, typeargs);
      const char* pkg_name = ast_name(package);
      const char* name = ast_name(id);

      if(pkg_name[0] != '\0')
      {
        size_t pkg_len = strlen(pkg_name);
        printbuf(buf, "%zu_%s_", pkg_len, pkg_name);
      }

      size_t len = strlen(name);
      printbuf(buf, "%zu_%s", len, name);

      ast_t* ta = ast_child(typeargs);
      if(ta != NULL)
      {
        printbuf(buf, "_A");
        while(ta != NULL)
        {
          printbuf(buf, "_");
          overload_encode_type(buf, ta);
          ta = ast_sibling(ta);
        }
        printbuf(buf, "_E");
      }
      break;
    }

    case TK_UNIONTYPE:
    {
      printbuf(buf, "U%d", ast_childcount(type));
      ast_t* child = ast_child(type);
      while(child != NULL)
      {
        printbuf(buf, "_");
        overload_encode_type(buf, child);
        child = ast_sibling(child);
      }
      break;
    }

    case TK_ISECTTYPE:
    {
      printbuf(buf, "I%d", ast_childcount(type));
      ast_t* child = ast_child(type);
      while(child != NULL)
      {
        printbuf(buf, "_");
        overload_encode_type(buf, child);
        child = ast_sibling(child);
      }
      break;
    }

    case TK_TUPLETYPE:
    {
      printbuf(buf, "T%d", ast_childcount(type));
      ast_t* child = ast_child(type);
      while(child != NULL)
      {
        printbuf(buf, "_");
        overload_encode_type(buf, child);
        child = ast_sibling(child);
      }
      break;
    }

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id);
      const char* name = ast_name(id);
      size_t len = strlen(name);
      printbuf(buf, "%zu_%s", len, name);
      break;
    }

    case TK_TYPEALIASREF:
    {
      ast_t* unfolded = typealias_unfold(type);
      pony_assert(unfolded != NULL);
      overload_encode_type(buf, unfolded);
      ast_free_unattached(unfolded);
      break;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      printbuf(buf, "V");
      overload_encode_cap(buf, left);
      printbuf(buf, "_");
      overload_encode_type(buf, right);
      break;
    }

    default:
      pony_assert(0);
      break;
  }
}

const char* overload_type_suffix(ast_t* method,
  deferred_reification_t* reify, pass_opt_t* opt)
{
  ast_t* params = ast_childidx(method, 3);
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "$");

  ast_t* p = ast_child(params);
  bool first = true;

  while(p != NULL)
  {
    ast_t* p_type = ast_childidx(p, 1);
    ast_t* r_type = (reify != NULL)
      ? deferred_reify(reify, p_type, opt) : p_type;

    if(!first)
      printbuf(buf, "$");
    first = false;

    overload_encode_type(buf, r_type);

    if(reify != NULL)
      ast_free_unattached(r_type);

    p = ast_sibling(p);
  }

  const char* result = stringtab(opt->strtab, buf->m);
  printbuf_free(buf);
  return result;
}

const char* overload_name_parse(const char* name, const char** out_suffix,
  pass_opt_t* opt)
{
  const char* dollar = strchr(name, '$');

  if(dollar == NULL)
  {
    *out_suffix = NULL;
    return name;
  }

  size_t base_len = (size_t)(dollar - name);
  char* base = (char*)ponyint_pool_alloc_size(base_len + 1);
  memcpy(base, name, base_len);
  base[base_len] = '\0';
  const char* result = stringtab(opt->strtab, base);
  *out_suffix = stringtab(opt->strtab, dollar);
  ponyint_pool_free_size(base_len + 1, base);
  return result;
}

ast_t* methodgroup_select_by_suffix(ast_t* group, const char* suffix,
  deferred_reification_t* reify, pass_opt_t* opt)
{
  pony_assert(ast_id(group) == TK_METHODGROUP);

  ast_t* child = ast_child(group);

  while(child != NULL)
  {
    if(ast_id(ast_childidx(child, 5)) == TK_NONE)
    {
      const char* child_suffix = overload_type_suffix(child, reify, opt);

      if(child_suffix == suffix)
        return child;
    }
    child = ast_sibling(child);
  }

  return NULL;
}
