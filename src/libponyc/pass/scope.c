#include "scope.h"
#include "../type/assemble.h"
#include "../pkg/package.h"
#include "../pkg/use.h"
#include "../ast/symtab.h"
#include "../ast/token.h"
#include "../ast/stringtab.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "ponyassert.h"
#include <string.h>

/**
 * Insert a name->AST mapping into the specified scope.
 */
static bool set_scope(pass_opt_t* opt, ast_t* scope, ast_t* name, ast_t* value,
  bool allow_shadowing)
{
  pony_assert(ast_id(name) == TK_ID);
  const char* s = ast_name(name);

  if(is_name_dontcare(s))
    return true;

  sym_status_t status = SYM_NONE;

  switch(ast_id(value))
  {
    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
    case TK_TYPEPARAM:
    case TK_PACKAGE:
    case TK_NEW:
    case TK_BE:
    case TK_FUN:
    case TK_METHODGROUP:
      break;

    case TK_VAR:
    case TK_LET:
      status = SYM_UNDEFINED;
      break;

    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
      status = SYM_DEFINED;
      break;

    default:
      pony_assert(0);
      return false;
  }

  if(!ast_set(scope, s, value, status, allow_shadowing, opt->strtab))
  {
    ast_t* prev = ast_get(scope, s, NULL);
    ast_t* prev_nocase = ast_get_case(scope, s, NULL, opt->strtab);

    ast_error(opt->check.errors, name, "can't reuse name '%s'", s);
    ast_error_continue(opt->check.errors, prev_nocase,
      "previous use of '%s'%s",
      s, (prev == NULL) ? " differs only by case" : "");

    return false;
  }

  return true;
}

bool use_package(ast_t* ast, const char* path, ast_t* name,
  pass_opt_t* options)
{
  pony_assert(ast != NULL);
  pony_assert(path != NULL);

  ast_t* package = package_load(ast, path, options);

  if(package == NULL)
  {
    ast_error(options->check.errors, ast, "can't load package '%s'", path);
    return false;
  }

  // Store the package so we can import it later without having to look it up
  // again
  ast_setdata(ast, (void*)package);

  ast_t* curr_package = ast_nearest(ast, TK_PACKAGE);
  pony_assert(curr_package != NULL);
  package_add_dependency(curr_package, package);

  if(name != NULL && ast_id(name) == TK_ID) // We have an alias
    return set_scope(options, ast, name, package, false);

  ast_setflag(ast, AST_FLAG_IMPORT);

  return true;
}

static bool has_guard(ast_t* method)
{
  return ast_id(ast_childidx(method, 5)) != TK_NONE;
}

static bool type_asts_equal(ast_t* a, ast_t* b)
{
  if(a == b)
    return true;

  if((a == NULL) || (b == NULL))
    return false;

  if(ast_id(a) != ast_id(b))
    return false;

  switch(ast_id(a))
  {
    case TK_ID:
    case TK_STRING:
      return ast_name(a) == ast_name(b);

    default:
      break;
  }

  if(ast_childcount(a) != ast_childcount(b))
    return false;

  ast_t* ca = ast_child(a);
  ast_t* cb = ast_child(b);

  while(ca != NULL)
  {
    if(!type_asts_equal(ca, cb))
      return false;

    ca = ast_sibling(ca);
    cb = ast_sibling(cb);
  }

  return true;
}

static bool params_same_type(ast_t* a, ast_t* b)
{
  ast_t* params_a = ast_childidx(a, 3);
  ast_t* params_b = ast_childidx(b, 3);

  if(ast_childcount(params_a) != ast_childcount(params_b))
    return false;

  ast_t* pa = ast_child(params_a);
  ast_t* pb = ast_child(params_b);

  while(pa != NULL)
  {
    if(ast_id(pa) != ast_id(pb))
      return false;

    if(ast_id(pa) == TK_PARAM)
    {
      if(!type_asts_equal(ast_childidx(pa, 1), ast_childidx(pb, 1)))
        return false;
    }

    pa = ast_sibling(pa);
    pb = ast_sibling(pb);
  }

  return true;
}

static bool is_type_overload_reserved(pass_opt_t* opt, const char* name)
{
  return (name == stringtab(opt->strtab, "_final")) ||
    (name == stringtab(opt->strtab, "_event_notify")) ||
    (name == stringtab(opt->strtab, "_init"));
}

static void detach_member(ast_t* member)
{
  ast_t* placeholder = ast_from(member, TK_NONE);
  ast_swap(member, placeholder);
  ast_remove(placeholder);
}

static bool scope_method(pass_opt_t* opt, ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);

  if(!set_scope(opt, ast_parent(ast), id, ast, false))
    return false;

  return true;
}

static ast_result_t scope_entity(pass_opt_t* opt, ast_t* ast)
{
  AST_GET_CHILDREN(ast, id, typeparams, cap, provides, members);

  if(!set_scope(opt, opt->check.frame->package, id, ast, false))
    return AST_ERROR;

  // Scope fields and methods immediately, so that the contents of method
  // signatures and bodies cannot shadow fields and methods.
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    ast_t* next = ast_sibling(member);

    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        if(!set_scope(opt, member, ast_child(member), member, false))
          return AST_ERROR;
        break;

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        const char* name = ast_name(ast_childidx(member, 1));
        ast_t* existing = ast_get(ast, name, NULL);

        if(existing == NULL)
        {
          if(!scope_method(opt, member))
            return AST_ERROR;
        }
        else if(ast_id(existing) == TK_METHODGROUP)
        {
          if(ast_id(member) != ast_id(ast_child(existing)))
          {
            ast_error(opt->check.errors, member,
              "can't mix method kinds in overloaded '%s'", name);
            ast_error_continue(opt->check.errors, ast_child(existing),
              "first definition is here");
            return AST_ERROR;
          }

          if(!has_guard(member))
          {
            // Check whether a default with the same parameter types
            // already exists in the group.
            ast_t* child = ast_child(existing);

            while(child != NULL)
            {
              if(!has_guard(child) && params_same_type(member, child))
              {
                ast_error(opt->check.errors, member,
                  "duplicate default for overloaded '%s'", name);
                ast_error_continue(opt->check.errors, child,
                  "previous default is here");
                return AST_ERROR;
              }

              child = ast_sibling(child);
            }

            // New type overload. Validate constraints.
            if(is_type_overload_reserved(opt, name))
            {
              ast_error(opt->check.errors, member,
                "cannot type-overload reserved method '%s'; "
                "compiler requires a single definition", name);
              return AST_ERROR;
            }

            if(ast_id(ast_child(member)) == TK_AT)
            {
              ast_error(opt->check.errors, member,
                "cannot overload bare function '%s'; bare functions "
                "use C calling convention and would produce "
                "duplicate symbols", name);
              return AST_ERROR;
            }

            ast_t* first = ast_child(existing);

            if(ast_id(ast_child(first)) == TK_AT)
            {
              ast_error(opt->check.errors, member,
                "cannot overload bare function '%s'; bare functions "
                "use C calling convention and would produce "
                "duplicate symbols", name);
              ast_error_continue(opt->check.errors, first,
                "bare function defined here");
              return AST_ERROR;
            }
          }

          detach_member(member);
          ast_append(existing, member);
        }
        else
        {
          ast_t* existing_guard = ast_childidx(existing, 5);
          ast_t* member_guard = ast_childidx(member, 5);
          bool existing_has_guard = ast_id(existing_guard) != TK_NONE;
          bool member_has_guard = ast_id(member_guard) != TK_NONE;

          if(!existing_has_guard && !member_has_guard)
          {
            if(params_same_type(existing, member))
            {
              ast_error(opt->check.errors, member,
                "can't reuse name '%s'", name);
              ast_error_continue(opt->check.errors, existing,
                "previous use of '%s'", name);
              return AST_ERROR;
            }

            // Type overload: different parameter types, no guards.
            if(ast_id(member) != ast_id(existing))
            {
              ast_error(opt->check.errors, member,
                "can't mix method kinds in overloaded '%s'", name);
              ast_error_continue(opt->check.errors, existing,
                "first definition is here");
              return AST_ERROR;
            }

            if(is_type_overload_reserved(opt, name))
            {
              ast_error(opt->check.errors, member,
                "cannot type-overload reserved method '%s'; "
                "compiler requires a single definition", name);
              return AST_ERROR;
            }

            if((ast_id(ast_child(member)) == TK_AT) ||
              (ast_id(ast_child(existing)) == TK_AT))
            {
              ast_t* bare =
                (ast_id(ast_child(member)) == TK_AT) ? member : existing;
              ast_error(opt->check.errors, member,
                "cannot overload bare function '%s'; bare functions "
                "use C calling convention and would produce "
                "duplicate symbols", name);

              if(bare != member)
                ast_error_continue(opt->check.errors, bare,
                  "bare function defined here");

              return AST_ERROR;
            }

            ast_t* group = ast_from(existing, TK_METHODGROUP);

            detach_member(member);
            ast_swap(existing, group);
            ast_append(group, existing);
            ast_append(group, member);

            symtab_t* symtab = ast_get_symtab(ast);
            symtab_replace(symtab, name, group);
          }
          else
          {
            // At least one has a guard: iftype specialization.
            if(ast_id(member) != ast_id(existing))
            {
              ast_error(opt->check.errors, member,
                "can't mix method kinds in overloaded '%s'", name);
              ast_error_continue(opt->check.errors, existing,
                "first definition is here");
              return AST_ERROR;
            }

            ast_t* group = ast_from(existing, TK_METHODGROUP);

            if(existing_has_guard)
            {
              if(member_has_guard)
              {
                ast_free_unattached(group);
                ast_error(opt->check.errors, member,
                  "overloaded '%s' has no default method "
                  "(without a guard)", name);
                ast_error_continue(opt->check.errors, existing,
                  "other definition is here");
                return AST_ERROR;
              }

              detach_member(member);
              ast_append(group, member);
              ast_swap(existing, group);
              ast_append(group, existing);
            }
            else
            {
              detach_member(member);
              ast_swap(existing, group);
              ast_append(group, existing);
              ast_append(group, member);
            }

            symtab_t* symtab = ast_get_symtab(ast);
            symtab_replace(symtab, name, group);
          }
        }
        break;
      }

      case TK_METHODGROUP:
        break;

      default:
        pony_assert(0);
        return AST_FATAL;
    }

    member = next;
  }

  return AST_OK;
}

static ast_t* make_iftype_typeparam(pass_opt_t* opt, ast_t* subtype,
  ast_t* supertype, ast_t* scope)
{
  pony_assert(ast_id(subtype) == TK_NOMINAL);

  const char* name = ast_name(ast_childidx(subtype, 1));
  ast_t* def = ast_get(scope, name, NULL);
  if(def == NULL)
  {
    ast_error(opt->check.errors, ast_child(subtype),
      "can't find definition of '%s'", name);
    return NULL;
  }

  if(ast_id(def) != TK_TYPEPARAM)
  {
    ast_error(opt->check.errors, subtype, "the subtype in an iftype condition "
      "must be a type parameter or a tuple of type parameters");
    return NULL;
  }

  ast_t* current_constraint = ast_childidx(def, 1);
  ast_t* new_constraint = ast_dup(supertype);
  if((ast_id(current_constraint) != TK_NOMINAL) ||
    (ast_name(ast_childidx(current_constraint, 1)) != name))
  {
    // If the constraint is the type parameter itself, there is no constraint.
    // We can't use type_isect to build the new constraint because we don't have
    // full type information yet.
    BUILD(isect, new_constraint,
      NODE(TK_ISECTTYPE,
        TREE(ast_dup(current_constraint))
        TREE(new_constraint)));

    new_constraint = isect;
  }

  BUILD(typeparam, def,
    NODE(TK_TYPEPARAM,
      ID(name)
      TREE(new_constraint)
      NONE));

  // keep data pointing to the original def
  ast_setdata(typeparam, ast_data(def));

  return typeparam;
}

static ast_result_t scope_iftype(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_IFTYPE);

  AST_GET_CHILDREN(ast, subtype, supertype, body, typeparam_store);
  // When the iftype occurs inside an object literal (or lambda), this pass
  // runs again during the "catch up" that processes the anonymous type. We've
  // already built the synthetic narrowed type parameters on the first run and
  // parked them in typeparam_store, so we don't rebuild them. But the catch up
  // empties this node's symbol table (capture_from_type clears every symtab in
  // the subtree via ast_clear), which drops the scope binding set_scope
  // established on the first run. Without it, references to the narrowed type
  // parameter in the supertype and body bind back to the original, un-narrowed
  // parameter, so a narrowing condition — including a valid F-bounded one such
  // as `iftype A <: T[A]` against `trait T[X: T[X]]` — fails to type-check.
  // Re-install the stored parameters so name resolution finds them again,
  // exactly as at method scope.
  if(ast_id(typeparam_store) != TK_NONE)
  {
    for(ast_t* typeparam = ast_child(typeparam_store); typeparam != NULL;
      typeparam = ast_sibling(typeparam))
    {
      // Re-install the stored node itself, never a copy. The symtab was just
      // emptied, so this is a fresh add; but the AST_OK descent below visits
      // typeparam_store and runs set_scope on these same nodes again, where
      // set_scope no-ops because the name already maps to this exact node. A
      // copy would be a different node and would error as a name clash there.
      if(!set_scope(opt, ast, ast_child(typeparam), typeparam, true))
        return AST_ERROR;
    }

    // Return AST_OK, not AST_IGNORE, so the scope pass still descends into
    // the children on the catch up, exactly as the first run does. This
    // matters for a nested iftype in the then branch: its own scope_iftype
    // must run here to re-install its narrowed parameter. AST_IGNORE would
    // skip the descent, so the inner condition would never be re-scoped and
    // its references would bind to the un-narrowed parameter — the same bug
    // this fix addresses, one level in. It also leaves the then branch's own
    // local bindings unscoped, which crashes a later pass (see #5441).
    return AST_OK;
  }

  ast_t* typeparams = ast_from(ast, TK_TYPEPARAMS);

  switch(ast_id(subtype))
  {
    case TK_NOMINAL:
    {
      ast_t* typeparam = make_iftype_typeparam(opt, subtype, supertype, ast);
      if(typeparam == NULL)
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(!set_scope(opt, ast, ast_child(typeparam), typeparam, true))
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_add(typeparams, typeparam);
      break;
    }

    case TK_TUPLETYPE:
    {
      if(ast_id(supertype) != TK_TUPLETYPE)
      {
        ast_error(opt->check.errors, subtype, "iftype subtype is a tuple but "
          "supertype isn't");
        ast_error_continue(opt->check.errors, supertype, "Supertype is %s",
          ast_print_type(supertype, opt->strtab));
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(ast_childcount(subtype) != ast_childcount(supertype))
      {
        ast_error(opt->check.errors, subtype, "the subtype and the supertype "
          "in an iftype condition must have the same cardinality");
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_t* sub_child = ast_child(subtype);
      ast_t* super_child = ast_child(supertype);
      while(sub_child != NULL)
      {
        ast_t* typeparam = make_iftype_typeparam(opt, sub_child, super_child,
          ast);
        if(typeparam == NULL)
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        if(!set_scope(opt, ast, ast_child(typeparam), typeparam, true))
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        ast_add(typeparams, typeparam);
        sub_child = ast_sibling(sub_child);
        super_child = ast_sibling(super_child);
      }

      break;
    }

    default:
      ast_error(opt->check.errors, subtype, "the subtype in an iftype "
        "condition must be a type parameter or a tuple of type parameters");
      ast_free_unattached(typeparams);
      return AST_ERROR;
  }

  // We don't want the scope pass to run on typeparams. The compiler would think
  // that type parameters are declared twice.
  ast_pass_record(typeparams, PASS_SCOPE);
  pony_assert(ast_id(typeparam_store) == TK_NONE);
  ast_replace(&typeparam_store, typeparams);
  return AST_OK;
}

static bool scope_call(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_CALL);
  AST_GET_CHILDREN(ast, lhs, positional, named, question);

  // Run the args before the receiver, so that symbol status tracking
  // will have their scope names defined in the args first.
  if(!ast_passes_subtree(&positional, opt, PASS_SCOPE) ||
    !ast_passes_subtree(&named, opt, PASS_SCOPE))
    return false;

  return true;
}

static bool scope_assign(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_ASSIGN);
  AST_GET_CHILDREN(ast, left, right);

  // Run the right side before the left side, so that symbol status tracking
  // will have their scope names defined in the right side first.
  if(!ast_passes_subtree(&right, opt, PASS_SCOPE))
    return false;

  return true;
}

static bool set_scope_guard(pass_opt_t* opt, ast_t* method, ast_t* typeparam)
{
  ast_t* id = ast_child(typeparam);
  pony_assert(ast_id(id) == TK_ID);
  const char* name = ast_name(id);

  if(ast_set(method, name, typeparam, SYM_NONE, true, opt->strtab))
    return true;

  // The name already exists in the method's local scope. This happens when
  // an AND guard constrains the same type parameter twice, or when the guard
  // constrains a method-level type parameter. In both cases
  // make_iftype_typeparam already built the correct narrowed constraint from
  // the existing entry; replace it.
  symtab_t* symtab = ast_get_symtab(method);

  if((symtab != NULL) && symtab_replace(symtab, name, typeparam))
    return true;

  ast_error(opt->check.errors, id, "can't reuse name '%s'", name);
  return false;
}

static ast_result_t scope_iftype_guard(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_IFTYPEGUARD);

  AST_GET_CHILDREN(ast, subtype, supertype, typeparam_store);
  ast_t* parent = ast_parent(ast);

  // Disjunction children are handled by scope_iftype_guard_or.
  if(ast_id(parent) == TK_IFTYPEGUARD_OR)
    return AST_OK;

  // Walk past compound guard nodes to find the method.
  ast_t* method = parent;
  if(ast_id(method) == TK_IFTYPEGUARD_AND)
    method = ast_parent(method);

  if(ast_id(typeparam_store) != TK_NONE)
  {
    for(ast_t* typeparam = ast_child(typeparam_store); typeparam != NULL;
      typeparam = ast_sibling(typeparam))
    {
      if(!set_scope_guard(opt, method, typeparam))
        return AST_ERROR;
    }

    return AST_OK;
  }

  ast_t* typeparams = ast_from(ast, TK_TYPEPARAMS);

  switch(ast_id(subtype))
  {
    case TK_NOMINAL:
    {
      ast_t* typeparam = make_iftype_typeparam(opt, subtype, supertype,
        method);
      if(typeparam == NULL)
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(!set_scope_guard(opt, method, typeparam))
      {
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_add(typeparams, typeparam);
      break;
    }

    case TK_TUPLETYPE:
    {
      if(ast_id(supertype) != TK_TUPLETYPE)
      {
        ast_error(opt->check.errors, subtype, "the subtype in an iftype guard "
          "is a tuple but the supertype is not");
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      if(ast_childcount(subtype) != ast_childcount(supertype))
      {
        ast_error(opt->check.errors, subtype, "the subtype and the supertype "
          "in an iftype guard must have the same cardinality");
        ast_free_unattached(typeparams);
        return AST_ERROR;
      }

      ast_t* sub_child = ast_child(subtype);
      ast_t* super_child = ast_child(supertype);
      while(sub_child != NULL)
      {
        ast_t* typeparam = make_iftype_typeparam(opt, sub_child, super_child,
          method);
        if(typeparam == NULL)
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        if(!set_scope_guard(opt, method, typeparam))
        {
          ast_free_unattached(typeparams);
          return AST_ERROR;
        }

        ast_add(typeparams, typeparam);
        sub_child = ast_sibling(sub_child);
        super_child = ast_sibling(super_child);
      }

      break;
    }

    default:
      ast_error(opt->check.errors, subtype, "the subtype in an iftype guard "
        "must be a type parameter or a tuple of type parameters");
      ast_free_unattached(typeparams);
      return AST_ERROR;
  }

  ast_swap(typeparam_store, typeparams);
  ast_free_unattached(typeparam_store);

  return AST_OK;
}

static ast_result_t scope_iftype_guard_or(pass_opt_t* opt, ast_t* ast)
{
  pony_assert(ast_id(ast) == TK_IFTYPEGUARD_OR);

  ast_t* method = ast_parent(ast);

  // Collect all unique type parameter names across branches, then build a
  // union constraint for each.

  // First child drives which parameters get narrowed. Each parameter's
  // supertype is unioned with matching supertypes from subsequent children.
  ast_t* first = ast_child(ast);
  pony_assert(ast_id(first) == TK_IFTYPEGUARD);

  AST_GET_CHILDREN(first, first_sub, first_super, first_store);

  if(ast_id(first_store) != TK_NONE)
  {
    // Already processed. Set scope from stored typeparams on first child only.
    for(ast_t* tp = ast_child(first_store); tp != NULL;
      tp = ast_sibling(tp))
    {
      if(!set_scope(opt, method, ast_child(tp), tp, true))
        return AST_ERROR;
    }
    return AST_OK;
  }

  if(ast_id(first_sub) != TK_NOMINAL)
  {
    ast_error(opt->check.errors, first_sub,
      "the subtype in an iftype guard must be a type parameter "
      "or a tuple of type parameters");
    return AST_ERROR;
  }

  const char* name = ast_name(ast_childidx(first_sub, 1));
  ast_t* def = ast_get(method, name, NULL);
  if(def == NULL)
  {
    ast_error(opt->check.errors, first_sub,
      "can't find definition of '%s'", name);
    return AST_ERROR;
  }

  if(ast_id(def) != TK_TYPEPARAM)
  {
    ast_error(opt->check.errors, first_sub,
      "the subtype in an iftype guard must be a type parameter "
      "or a tuple of type parameters");
    return AST_ERROR;
  }

  // Build union of supertypes from all branches. All branches must constrain
  // the same type parameter.
  ast_t* union_super = ast_dup(first_super);

  ast_t* other = ast_sibling(first);
  while(other != NULL)
  {
    pony_assert(ast_id(other) == TK_IFTYPEGUARD);
    ast_t* o_sub = ast_child(other);
    ast_t* o_super = ast_childidx(other, 1);

    if(ast_id(o_sub) != TK_NOMINAL)
    {
      ast_error(opt->check.errors, o_sub,
        "the subtype in an iftype guard must be a type parameter "
        "or a tuple of type parameters");
      ast_free_unattached(union_super);
      return AST_ERROR;
    }

    const char* o_name = ast_name(ast_childidx(o_sub, 1));
    if(o_name != name)
    {
      ast_error(opt->check.errors, o_sub,
        "all branches of an 'or' guard must constrain the same type "
        "parameter");
      ast_error_continue(opt->check.errors, first_sub,
        "first branch constrains '%s'", name);
      ast_free_unattached(union_super);
      return AST_ERROR;
    }

    BUILD(u, union_super,
      NODE(TK_UNIONTYPE,
        TREE(union_super)
        TREE(ast_dup(o_super))));
    union_super = u;

    other = ast_sibling(other);
  }

  ast_t* typeparam = make_iftype_typeparam(opt, first_sub, union_super, method);
  ast_free_unattached(union_super);

  if(typeparam == NULL)
    return AST_ERROR;

  if(!set_scope(opt, method, ast_child(typeparam), typeparam, true))
    return AST_ERROR;

  // Store the narrowed typeparam on the first child's typeparam_store.
  ast_t* typeparams = ast_from(first, TK_TYPEPARAMS);
  ast_add(typeparams, typeparam);
  ast_swap(first_store, typeparams);
  ast_free_unattached(first_store);

  return AST_OK;
}

ast_result_t pass_scope(ast_t** astp, pass_opt_t* options)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_USE:
      return use_command(ast, options);

    case TK_TYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return scope_entity(options, ast);

    case TK_VAR:
    case TK_LET:
    case TK_PARAM:
    case TK_MATCH_CAPTURE:
      if(!set_scope(options, ast, ast_child(ast), ast, false))
        return AST_ERROR;
      break;

    case TK_TYPEPARAM:
      if(!set_scope(options, ast, ast_child(ast), ast, false))
        return AST_ERROR;

      // Store the original definition of the typeparam in the data field here.
      // It will be retained later if the typeparam def is copied via ast_dup.
      if(ast_data(ast) == NULL)
        ast_setdata(ast, ast);
      break;

    case TK_IFTYPE:
      return scope_iftype(options, ast);

    case TK_IFTYPEGUARD:
      return scope_iftype_guard(options, ast);

    case TK_IFTYPEGUARD_OR:
      return scope_iftype_guard_or(options, ast);

    case TK_CALL:
      if(!scope_call(options, ast))
        return AST_ERROR;
      break;

    case TK_ASSIGN:
      if(!scope_assign(options, ast))
        return AST_ERROR;
      break;

    default: {}
  }

  return AST_OK;
}
