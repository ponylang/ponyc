#include "traits.h"
#include "mutation.h"
#include "sugar.h"
#include "names.h"
#include "scope.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include "../ast/id.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../type/assemble.h"
#include "../type/subtype.h"
#include "../type/reify.h"
#include "../type/typeparam.h"
#include "../type/typealias.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <string.h>


/** The following defines how we determine the signature and body to use for
 * some method M in type T.
 * Note that the flatten pass will already have ensured that provides types
 * contain only traits and interfaces, and converted those to lists.
 *
 * 1. Explicit.
 *    If T provides an explicit definition of M we use that, including the body
 *    defined. If T provides a signature and no body, then T.M has no body,
 *    even if a body is available from some other source.
 * 2. Compatible inheritance.
 *    If T gets an M from either (1) or (2) then we use "compatible"
 *    inheritance.
 *    Each M from T's provides list must be a supertype of T.M.
 *    Any default bodies of M from the provides list are ignored, even if T.M
 *    has no body.
 * 3. Identical inheritance.
 *    If T does NOT get an M from either (1) or (2) then we use "identical"
 *    inheritance.
 *    All Ms from T's provides list must have identical signatures when
 *    compared using the declared type parameter capabilities.
 *    If exactly one default body is provided for M then T.M uses that.
 *    If multiple default bodies are provided for M then T.M is body ambiguous.
 *    If any types in T's provides list are body ambiguous for M then T.M is
 *    body ambiguous.
 * 4. Required body.
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
  ast_t* trait_ref;
  bool local_define;
  bool failed;
} method_t;


static bool trait_entity(ast_t* entity, pass_opt_t* options);
static void local_types(ast_t* ast);
static ast_t* inherited_source(ast_t* entity, ast_t* member,
  pass_opt_t* opt);
static bool is_trait_view_entity(ast_t* entity, pass_opt_t* opt);


// Report whether the given node is a method.
static bool is_method(ast_t* ast)
{
  pony_assert(ast != NULL);

  token_id variety = ast_id(ast);
  return (variety == TK_BE) || (variety == TK_FUN) || (variety == TK_NEW);
}


// Attach a new method_t structure to the given method.
static method_t* attach_method_t(ast_t* method)
{
  pony_assert(method != NULL);

  method_t* p = POOL_ALLOC(method_t);
  p->body_donor = NULL;
  p->trait_ref = NULL;
  p->local_define = false;
  p->failed = false;

  ast_setdata(method, p);
  return p;
}


// Setup a method_t structure for each method in the given type.
static void setup_one_method(ast_t* entity, ast_t* method)
{
  method_t* info = attach_method_t(method);
  info->local_define = true;

  if(ast_id(ast_childidx(method, 7)) != TK_NONE)
    info->body_donor = entity;
}

static void setup_local_methods(ast_t* ast)
{
  pony_assert(ast != NULL);

  ast_t* members = ast_childidx(ast, 4);
  pony_assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      setup_one_method(ast, p);
    }
    else if(ast_id(p) == TK_METHODGROUP)
    {
      for(ast_t* m = ast_child(p); m != NULL; m = ast_sibling(m))
        setup_one_method(ast, m);
    }
  }
}


static bool finish_method(ast_t* entity, ast_t* method, pass_opt_t* opt,
  const mutation_type_policy_t* policy)
{
  method_t* info = (method_t*)ast_data(method);
  pony_assert(info != NULL);
  ast_t* body_donor = info->body_donor;
  POOL_FREE(method_t, info);

  if(body_donor == NULL)
    body_donor = entity;

  ast_setdata(method, body_donor);
  return (policy == NULL) || mutation_resolve_method(opt, policy, method);
}

static bool finish_methods(ast_t* entity, pass_opt_t* opt, bool composed)
{
  pony_assert(entity != NULL);

  ast_t* members = ast_childidx(entity, 4);
  pony_assert(members != NULL);

  mutation_type_policy_t type_policy = mutation_type_policy(entity);
  const mutation_type_policy_t* policy = composed ? &type_policy : NULL;
  bool ok = composed;
  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      if(!finish_method(entity, p, opt, policy))
        ok = false;
    }
    else if(ast_id(p) == TK_METHODGROUP)
    {
      for(ast_t* m = ast_child(p); m != NULL; m = ast_sibling(m))
      {
        if(!finish_method(entity, m, opt, policy))
          ok = false;
      }
    }
  }

  return ok;
}


typedef enum
{
  COMPARE_EXACT,
  COMPARE_GUARDS,
  COMPARE_INHERITED_SIGNATURE
} signature_comparison_t;

// A pair of type aliases being compared, so that comparing recursive aliases
// ends: a pair met again while it is being compared is taken as equal.
typedef struct alias_pair_t
{
  ast_t* a;
  ast_t* b;
  struct alias_pair_t* outer;
} alias_pair_t;

static bool compare_nodes(alias_pair_t* pairs, ast_t* sig_a, ast_t* sig_b,
  signature_comparison_t comparison);

// Whether every child of `a` matches a distinct child of `b`, in any order.
// Taking the first unused matching child is correct only because the
// comparison is an equivalence relation.
static bool compare_unordered_children(alias_pair_t* pairs, ast_t* a,
  ast_t* b,
  signature_comparison_t comparison)
{
  size_t count = ast_childcount(a);
  if(count != ast_childcount(b))
    return false;

  if(count == 0)
    return true;

  bool* used = (bool*)ponyint_pool_alloc_size(count * sizeof(bool));
  memset(used, 0, count * sizeof(bool));
  bool ok = true;

  for(ast_t* a_child = ast_child(a); ok && (a_child != NULL);
    a_child = ast_sibling(a_child))
  {
    bool found = false;
    size_t i = 0;
    for(ast_t* b_child = ast_child(b); b_child != NULL;
      b_child = ast_sibling(b_child), i++)
    {
      if(!used[i] && compare_nodes(pairs, a_child, b_child, comparison))
      {
        used[i] = true;
        found = true;
        break;
      }
    }

    ok = found;
  }

  ponyint_pool_free_size(count * sizeof(bool), used);
  return ok;
}

// Append to `to` copies of the members of the union or intersection `type`,
// taking the members of a member of the same kind instead of it, including
// one an alias names. A member equal to one already appended is left out.
static void append_guard_members(alias_pair_t* pairs, ast_t* to, ast_t* type)
{
  for(ast_t* m = ast_child(type); m != NULL; m = ast_sibling(m))
  {
    if(ast_id(m) == ast_id(to))
    {
      append_guard_members(pairs, to, m);
      continue;
    }

    if(ast_id(m) == TK_TYPEALIASREF)
    {
      ast_t* unfolded = typealias_unfold(m);

      if((unfolded != NULL) && (ast_id(unfolded) == ast_id(to)))
      {
        append_guard_members(pairs, to, unfolded);
        ast_free_unattached(unfolded);
        continue;
      }

      if(unfolded != NULL)
        ast_free_unattached(unfolded);
    }

    bool repeated = false;

    for(ast_t* prev = ast_child(to); !repeated && (prev != NULL);
      prev = ast_sibling(prev))
      repeated = compare_nodes(pairs, prev, m, COMPARE_GUARDS);

    if(!repeated)
      ast_append(to, ast_dup(m));
  }
}

static bool compare_nodes(alias_pair_t* pairs, ast_t* sig_a, ast_t* sig_b,
  signature_comparison_t comparison)
{
  if(sig_a == NULL && sig_b == NULL)
    return true;

  if(sig_a == NULL || sig_b == NULL)
    return false;

  // A guard tests types, not how they are spelled: a type alias in a guard
  // matches the type it names. Two references to the same alias compare as
  // written.
  bool a_alias = ast_id(sig_a) == TK_TYPEALIASREF;
  bool b_alias = ast_id(sig_b) == TK_TYPEALIASREF;

  if((comparison == COMPARE_GUARDS) && (a_alias || b_alias) &&
    !(a_alias && b_alias && (ast_data(sig_a) == ast_data(sig_b))))
  {
    ast_t* def_a = a_alias ? (ast_t*)ast_data(sig_a) : sig_a;
    ast_t* def_b = b_alias ? (ast_t*)ast_data(sig_b) : sig_b;

    for(alias_pair_t* p = pairs; p != NULL; p = p->outer)
    {
      if((p->a == def_a) && (p->b == def_b))
        return true;
    }

    alias_pair_t pair = {def_a, def_b, pairs};
    ast_t* a = a_alias ? typealias_unfold(sig_a) : sig_a;
    ast_t* b = b_alias ? typealias_unfold(sig_b) : sig_b;
    bool r = (a != NULL) && (b != NULL) &&
      compare_nodes(&pair, a, b, comparison);

    if((a != sig_a) && (a != NULL))
      ast_free_unattached(a);
    if((b != sig_b) && (b != NULL))
      ast_free_unattached(b);

    return r;
  }

  token_id a_id = ast_id(sig_a);

  if(a_id != ast_id(sig_b))
    return false;

  switch(a_id)
  {
    case TK_BE:
    case TK_FUN:
    case TK_NEW:
    {
      // Check children 0-4 (cap, id, typeparams, params, result) and child 6
      // (can_error). Skip child 5 (guard) — it distinguishes specializations,
      // not conforming signatures.
      ast_t* a_child = ast_child(sig_a);
      ast_t* b_child = ast_child(sig_b);

      for(int i = 0; i < 7; i++)
      {
        if(a_child == NULL || b_child == NULL)
          return false;

        if(i != 5 && !compare_nodes(pairs, a_child, b_child, comparison))
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

    case TK_IFTYPEGUARD:
      // The narrowed type parameter bindings are derived from the operands.
      return compare_nodes(pairs, ast_child(sig_a), ast_child(sig_b),
        COMPARE_GUARDS) && compare_nodes(pairs, ast_childidx(sig_a, 1),
        ast_childidx(sig_b, 1), COMPARE_GUARDS);

    case TK_IFTYPEGUARD_AND:
    case TK_IFTYPEGUARD_OR:
      // The clauses of an `and` or an `or` can be written in any order.
      return compare_unordered_children(pairs, sig_a, sig_b, COMPARE_GUARDS);

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      // In a guard, the members of a union or an intersection can be written
      // in any order, and nested in one of the same kind, directly or through
      // an alias.
      if(comparison == COMPARE_GUARDS)
      {
        ast_t* flat_a = ast_from(sig_a, a_id);
        ast_t* flat_b = ast_from(sig_b, a_id);
        append_guard_members(pairs, flat_a, sig_a);
        append_guard_members(pairs, flat_b, sig_b);
        bool r = compare_unordered_children(pairs, flat_a, flat_b, comparison);
        ast_free_unattached(flat_a);
        ast_free_unattached(flat_b);
        return r;
      }
      break;

    case TK_TYPEPARAM:
      // Two methods' own type parameters match by position, whatever their
      // names: compare the constraint and the default only.
      return compare_nodes(pairs, ast_childidx(sig_a, 1),
          ast_childidx(sig_b, 1), comparison) &&
        compare_nodes(pairs, ast_childidx(sig_a, 2),
          ast_childidx(sig_b, 2), comparison);

    case TK_TYPEPARAMREF:
    {
      ast_t* root_a = typeparam_root((ast_t*)ast_data(sig_a));
      ast_t* root_b = typeparam_root((ast_t*)ast_data(sig_b));
      if(comparison == COMPARE_EXACT && root_a != root_b)
        return false;

      if(comparison == COMPARE_INHERITED_SIGNATURE &&
        root_a != NULL && root_a == root_b)
      {
        // An inherited guard can refine a reference's capability without
        // changing its declaration or ephemerality.
        return ast_id(ast_childidx(sig_a, 2)) == ast_id(ast_childidx(sig_b, 2));
      }

      if(root_a != root_b)
      {
        // References to the two methods' own type parameters match by
        // position.
        typeparam_match_t match = typeparam_method_refs_compare(sig_a, sig_b);
        if(match != TYPEPARAM_MATCH_NEITHER_METHOD)
        {
          return (match == TYPEPARAM_MATCH_SAME) &&
            compare_nodes(pairs, ast_childidx(sig_a, 1),
              ast_childidx(sig_b, 1), comparison) &&
            compare_nodes(pairs, ast_childidx(sig_a, 2),
              ast_childidx(sig_b, 2), comparison);
        }
      }
      break;
    }

    case TK_NOMINAL:
    case TK_TYPEALIASREF:
      if(ast_data(sig_a) != ast_data(sig_b))
        return false;

      if(comparison == COMPARE_GUARDS && a_id == TK_NOMINAL)
      {
        // The resolved definition identifies the nominal type. The written
        // module qualifier is only used in diagnostics.
        for(size_t i = 2; i < 5; i++)
        {
          if(!compare_nodes(pairs, ast_childidx(sig_a, i),
            ast_childidx(sig_b, i), COMPARE_GUARDS))
            return false;
        }

        return true;
      }

      break;

    default:
      break;
  }

  ast_t* a_child = ast_child(sig_a);
  ast_t* b_child = ast_child(sig_b);

  while(a_child != NULL && b_child != NULL)
  {
    if(!compare_nodes(pairs, a_child, b_child, comparison))
      return false;

    a_child = ast_sibling(a_child);
    b_child = ast_sibling(b_child);
  }

  if(a_child != NULL || b_child != NULL)
    return false;

  return true;
}

static bool compare_signature_nodes(ast_t* sig_a, ast_t* sig_b,
  signature_comparison_t comparison)
{
  return compare_nodes(NULL, sig_a, sig_b, comparison);
}


static ast_t* narrowing_owner(ast_t* typeparam)
{
  ast_t* store = ast_parent(typeparam);
  ast_t* owner = store != NULL ? ast_parent(store) : NULL;
  if(owner != NULL &&
    (ast_id(owner) == TK_IFTYPE || ast_id(owner) == TK_IFTYPEGUARD))
    return owner;
  return NULL;
}

static bool is_receiver_param(ast_t* typeparam, ast_t* scope)
{
  ast_t* members = ast_nearest(scope, TK_MEMBERS);
  ast_t* root_params = ast_parent(typeparam_root(typeparam));
  return members != NULL && root_params != NULL &&
    ast_parent(root_params) == ast_parent(members);
}

static bool narrows_receiver_param(ast_t* typeparam, ast_t* scope)
{
  return narrowing_owner(typeparam) != NULL &&
    is_receiver_param(typeparam, scope);
}

static ast_t* narrowed_param_store(ast_t* ast)
{
  if(ast_id(ast) == TK_IFTYPE)
    return ast_childidx(ast, 3);
  if(ast_id(ast) == TK_IFTYPEGUARD)
    return ast_childidx(ast, 2);
  return NULL;
}

static bool has_structural_constraints(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_ISECTTYPE:
    case TK_UNIONTYPE:
      return true;
    default:
      return false;
  }
}

static void preserve_narrowed_constraint(ast_t** astp, ast_t* donor)
{
  ast_t* ast = *astp;
  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(ast);
    if(def == NULL || !narrows_receiver_param(def, donor) ||
      typeparam_constraint(ast) == NULL)
      return;

    ast_t* constraint = ast_dup(ast_childidx(def, 1));
    preserve_narrowed_constraint(&constraint, donor);
    BUILD(intersection, ast,
      NODE(TK_ISECTTYPE,
        TREE(ast_dup(ast))
        TREE(constraint)));
    ast_replace(astp, intersection);
  } else if(has_structural_constraints(ast)) {
    for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
      preserve_narrowed_constraint(&child, donor);
  }
}

static bool preserve_narrowed_constraints(ast_t* ast, ast_t* donor)
{
  ast_t* store = narrowed_param_store(ast);
  ast_t* first = store == NULL ? NULL : ast_child(store);
  bool has_narrowings = first != NULL;
  for(ast_t* narrowed = first;
    narrowed != NULL; narrowed = ast_sibling(narrowed))
  {
    if(!is_receiver_param(narrowed, donor))
      continue;

    // Inheritance can substitute one receiver parameter for several trait
    // parameters. Copy the referenced narrowings before they can become
    // references back to the same parameter.
    ast_t* constraint = ast_childidx(narrowed, 1);
    preserve_narrowed_constraint(&constraint, donor);
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(preserve_narrowed_constraints(child, donor))
      has_narrowings = true;
  }
  return has_narrowings;
}

// A checked body's expression types refer to the narrowings its iftypes
// declare. Before the trait's parameters are substituted, make each such
// reference the intersection of the parameter and its narrowed constraint, so
// the substituted type keeps what the narrowing provided.
// Only expression types change: the type nodes written in the code, such as
// an iftype's subject, keep naming the parameter. Within an expression type,
// a reference at the top or in a union or intersection changes; one in a type
// argument does not: changing it would make instantiations grow.
static void preserve_narrowed_types(ast_t* ast, ast_t* donor)
{
  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    preserve_narrowed_types(child, donor);

  ast_t* type = ast_type(ast);
  if(type != NULL)
    preserve_narrowed_constraint(&type, donor);
}

static void reify_narrowed_params(ast_t* ast, ast_t* typeparams,
  ast_t* typeargs, pass_opt_t* opt)
{
  ast_t* store = narrowed_param_store(ast);

  for(ast_t* narrowed = store == NULL ? NULL : ast_child(store);
    narrowed != NULL; narrowed = ast_sibling(narrowed))
  {
    ast_t* root = typeparam_root(narrowed);
    ast_t* param = ast_child(typeparams);
    ast_t* arg = ast_child(typeargs);
    for(; param != NULL; param = ast_sibling(param), arg = ast_sibling(arg))
    {
      if(typeparam_root(param) != root || ast_id(arg) != TK_TYPEPARAMREF)
        continue;

      ast_t* target = typeparam_root((ast_t*)ast_data(arg));
      ast_set_name(ast_child(narrowed), ast_name(ast_child(target)),
        opt->strtab);
      ast_setdata(narrowed, target);
      break;
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    reify_narrowed_params(child, typeparams, typeargs, opt);
}

// Whether no two type arguments are the same type parameter.
static bool distinct_param_args(ast_t* typeargs)
{
  for(ast_t* a = ast_child(typeargs); a != NULL; a = ast_sibling(a))
  {
    if(ast_id(a) != TK_TYPEPARAMREF)
      continue;

    ast_t* root = typeparam_root((ast_t*)ast_data(a));

    for(ast_t* b = ast_sibling(a); b != NULL; b = ast_sibling(b))
    {
      if((ast_id(b) == TK_TYPEPARAMREF) &&
        (typeparam_root((ast_t*)ast_data(b)) == root))
        return false;
    }
  }

  return true;
}

static ast_t* reify_provides_type(ast_t* entity, ast_t* method,
  ast_t* trait_ref, pass_opt_t* opt)
{
  pony_assert(entity != NULL);
  pony_assert(method != NULL);
  pony_assert(trait_ref != NULL);

  // Apply the type args (if any) from the trait reference to the type
  // parameters from the trait definition.
  ast_t* trait_def = (ast_t*)ast_data(trait_ref);
  pony_assert(trait_def != NULL);
  ast_t* type_args = ast_childidx(trait_ref, 2);
  ast_t* type_params = ast_childidx(trait_def, 1);

  if(!reify_defaults(type_params, type_args, true, opt))
    return NULL;

  ast_t* copy = ast_dup_closed(method);

  // A narrowing of a trait parameter keeps its constraint when the argument
  // is (or, in a trait-view copy, mentions) a type parameter: the branch is
  // decided per instantiation, and its checked code relies on the narrowing.
  bool trait_view = is_trait_view_entity(entity, opt);

  for(ast_t* arg = ast_child(type_args); arg != NULL; arg = ast_sibling(arg))
  {
    if((ast_id(arg) == TK_TYPEPARAMREF) ||
      (trait_view && type_mentions_typeparam(arg)))
    {
      if(trait_view)
      {
        // In a trait-view body, a narrowing of a parameter whose argument is
        // a type parameter Y becomes Y's narrowing, so its checked types say
        // "Y narrowed" rather than an intersection with Y, which reach would
        // turn into an intersection with a machine word. When one Y stands
        // for several parameters, their narrowings can't all become Y's, and
        // the intersections stay.
        if(distinct_param_args(type_args))
          reify_narrowed_params(copy, type_params, type_args, opt);

        preserve_narrowed_types(copy, method);
      }

      bool has_narrowings = preserve_narrowed_constraints(copy, method);
      ast_t* reified = reify(copy, type_params, type_args, opt, false);
      if(has_narrowings)
        reify_narrowed_params(reified, type_params, type_args, opt);
      return reified;
    }
  }

  return reify(copy, type_params, type_args, opt, false);
}


// Find the method with the specified name in the given entity.
// Find only methods, ignore fields.
// Return the method with the specified name or NULL if not found.
static ast_t* find_method(ast_t* entity, const char* name)
{
  pony_assert(entity != NULL);
  pony_assert(name != NULL);

  ast_t* method = ast_get(entity, name, NULL);

  if(method == NULL)
    return NULL;

  if(is_method(method) || (ast_id(method) == TK_METHODGROUP))
    return method;

  return NULL;
}


// Add a new method to the given entity, based on the specified method from
// the specified type.
// The trait_ref is the entry in the provides list that causes this
// method inclusion. Needed for error reporting.
// The basis_method is the reified method in the trait to add.
// The adjective parameter is used for error reporting and should be "provided"
// or similar.
// Return the newly added method or NULL on error.
static ast_t* add_method(ast_t* entity, ast_t* trait_ref, ast_t* basis_method,
  const char* adjective, pass_opt_t* opt)
{
  pony_assert(entity != NULL);
  pony_assert(trait_ref != NULL);
  pony_assert(basis_method != NULL);
  pony_assert(adjective != NULL);

  const char* name = ast_name(ast_childidx(basis_method, 1));

  // Check behaviour compatibility.
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
    // Should already have checked for methods.
    token_id variety = ast_id(existing);
    (void)variety;
    pony_assert((variety == TK_FVAR) || (variety == TK_FLET) || (variety == TK_EMBED));

    ast_error(opt->check.errors, trait_ref,
      "%s method '%s' clashes with field", adjective, name);
    ast_error_continue(opt->check.errors, basis_method,
      "method is defined here");
    return NULL;
  }

  // Check for clash with existing method.
  ast_t* case_clash = ast_get_case(entity, name, NULL, opt->strtab);

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

      case TK_METHODGROUP:
        clash_name = ast_name(ast_childidx(ast_child(case_clash), 1));
        break;

      case TK_LET:
      case TK_VAR:
      case TK_EMBED:
        clash_name = ast_name(ast_child(case_clash));
        break;

      default:
        pony_assert(0);
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
    guard, can_error, body, doc);

  // Ignore docstring.
  if(ast_id(doc) == TK_STRING)
  {
    ast_set_name(doc, "", opt->strtab);
    ast_setid(doc, TK_NONE);
    ast_settype(doc, NULL);
  }

  ast_t* local = ast_append(ast_childidx(entity, 4), basis_method);
  ast_set(entity, name, local, SYM_DEFINED, false, opt->strtab);
  ast_t* body_donor = (ast_t*)ast_data(basis_method);
  method_t* info = attach_method_t(local);
  info->trait_ref = trait_ref;

  if(ast_id(body) != TK_NONE)
    info->body_donor = body_donor;

  return local;
}


static void append_inherited_constraint(ast_t* intersection, ast_t* constraint,
  ast_t* root)
{
  if(ast_id(constraint) == TK_NONE || (ast_id(constraint) == TK_TYPEPARAMREF &&
    typeparam_root((ast_t*)ast_data(constraint)) == root))
    return;

  if(ast_id(constraint) == TK_ISECTTYPE)
  {
    for(ast_t* child = ast_child(constraint); child != NULL;
      child = ast_sibling(child))
      append_inherited_constraint(intersection, child, root);
    return;
  }

  for(ast_t* child = ast_child(intersection); child != NULL;
    child = ast_sibling(child))
  {
    if(compare_signature_nodes(child, constraint, COMPARE_EXACT))
      return;
  }

  ast_append(intersection, ast_dup(constraint));
}

static void combine_narrowed_constraints(ast_t* narrowed, ast_t* other)
{
  ast_t* constraint = ast_childidx(narrowed, 1);
  ast_t* other_constraint = ast_childidx(other, 1);
  ast_t* root = typeparam_root(narrowed);

  // Structural subtyping needs the completed trait method tables.
  ast_t* combined = ast_from(constraint, TK_ISECTTYPE);
  append_inherited_constraint(combined, constraint, root);
  append_inherited_constraint(combined, other_constraint, root);
  ast_t* first = ast_child(combined);
  if(first == NULL)
  {
    ast_free_unattached(combined);
    combined = ast_dup(ast_childidx(root, 1));
  } else if(ast_sibling(first) == NULL)
  {
    ast_t* only = ast_pop(combined);
    ast_free_unattached(combined);
    combined = only;
  }

  ast_replace(&constraint, combined);
}

static void rescope_narrowed_params(ast_t* scope, ast_t* store,
  pass_opt_t* opt)
{
  symtab_t* symtab = ast_get_symtab(scope);
  ast_t* narrowed = ast_child(store);
  while(narrowed != NULL)
  {
    ast_t* next = ast_sibling(narrowed);
    if(!narrows_receiver_param(narrowed, scope))
    {
      narrowed = next;
      continue;
    }

    const char* name = ast_name(ast_child(narrowed));
    ast_t* root = typeparam_root(narrowed);
    ast_t* visible = ast_get_case(scope, name, NULL, opt->strtab);
    if(visible != NULL && (ast_id(visible) != TK_TYPEPARAM ||
      typeparam_root(visible) != root))
      name = typeparam_receiver_key(opt->strtab, name);

    ast_t* local = symtab_find(symtab, name, NULL);
    if(local != NULL)
    {
      pony_assert(typeparam_root(local) == root);
      combine_narrowed_constraints(local, narrowed);
      ast_remove(narrowed);
      narrowed = next;
      continue;
    }

    visible = ast_get(ast_parent(scope), name, NULL);
    if(visible == NULL || ast_id(visible) != TK_TYPEPARAM ||
      typeparam_root(visible) != root)
      visible = root;
    combine_narrowed_constraints(narrowed, visible);
    ast_set(scope, name, narrowed, SYM_DEFINED, true, opt->strtab);
    narrowed = next;
  }

  if(ast_child(store) == NULL)
    ast_setid(store, TK_NONE);
}

// In a closed copy, the scopes of an iftype or a guarded method still map the
// names of the trait's narrowed receiver type parameters to the copied
// narrowings themselves. Drop those entries, so that rescope_narrowed_params
// binds each narrowing afresh, under the name of the inheriting type's
// parameter that replaced the trait's and combined with its constraint.
static void drop_receiver_narrowings(ast_t* scope, ast_t* store)
{
  symtab_t* symtab = ast_get_symtab(scope);
  if(symtab == NULL)
    return;

  for(ast_t* narrowed = ast_child(store); narrowed != NULL;
    narrowed = ast_sibling(narrowed))
  {
    if(narrows_receiver_param(narrowed, scope))
      symtab_remove_def(symtab, narrowed);
  }
}

static void rescope_narrowings_in(ast_t* scope, ast_t* store,
  pass_opt_t* opt)
{
  drop_receiver_narrowings(scope, store);
  rescope_narrowed_params(scope, store, opt);
}

static void rescope_guard(ast_t* method, ast_t* guard, pass_opt_t* opt)
{
  if(ast_id(guard) == TK_IFTYPEGUARD)
  {
    rescope_narrowings_in(method, ast_childidx(guard, 2), opt);
    return;
  }

  for(ast_t* clause = ast_child(guard); clause != NULL;
    clause = ast_sibling(clause))
    rescope_narrowings_in(method, ast_childidx(clause, 2), opt);
}

// Bind the narrowings of the trait's type parameters in a closed copy of an
// inherited method to the inheriting type's parameters that replaced them, and
// give its local declarations their types. Everything else in the copy already
// refers to the copy's own nodes or to definitions bound where the method was
// written.
static void rescope_copy_narrowings(ast_t* ast, ast_t* entity,
  pass_opt_t* opt)
{
  switch(ast_id(ast))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      rescope_guard(ast, ast_childidx(ast, 5), opt);
      break;

    case TK_IFTYPE:
    {
      // The supertype, the body and the narrowed constraints are in the
      // narrowing, as they are where the iftype is written; the subject is
      // not.
      ast_t* store = ast_childidx(ast, 3);
      rescope_narrowings_in(ast, store, opt);
      rescope_copy_narrowings(ast_childidx(ast, 1), entity, opt);
      rescope_copy_narrowings(ast_childidx(ast, 2), entity, opt);

      for(ast_t* tp = ast_child(store); tp != NULL; tp = ast_sibling(tp))
        rescope_copy_narrowings(ast_childidx(tp, 1), entity, opt);
      return;
    }

    case TK_ISECTTYPE:
    case TK_UNIONTYPE:
    {
      // A reference directly in an intersection or union of a narrowed
      // constraint keeps its binding: rebinding it to the narrowing it
      // defines makes a cycle. One inside a type argument is rebound, as an
      // F-bounded constraint needs.
      ast_t* parent = ast_parent(ast);
      while((parent != NULL) && ((ast_id(parent) == TK_ISECTTYPE) ||
        (ast_id(parent) == TK_UNIONTYPE)))
        parent = ast_parent(parent);

      if((parent != NULL) && (ast_id(parent) == TK_TYPEPARAM))
      {
        for(ast_t* child = ast_child(ast); child != NULL;
          child = ast_sibling(child))
        {
          if(ast_id(child) != TK_TYPEPARAMREF)
            rescope_copy_narrowings(child, entity, opt);
        }
        return;
      }
      break;
    }

    case TK_TYPEPARAMS:
    case TK_TYPEPARAM:
      return;

    case TK_LET:
    case TK_VAR:
      // The copy can come from a trait the traits pass has not visited yet,
      // and the traits pass does not visit methods added to the entity it is
      // processing.
      local_types(ast);
      break;

    case TK_TYPEPARAMREF:
    {
      // A trait parameter's narrowing became a narrowing of the inheriting
      // type's parameter that replaced it. A reference to that parameter
      // inside the narrowing refers to the narrowing.
      ast_t* def = (ast_t*)ast_data(ast);
      if((def != NULL) && (ast_parent(def) == ast_childidx(entity, 1)))
      {
        ast_t* narrowed = typeparam_resolve(ast, ast, opt->strtab);
        if((narrowed != NULL) && (narrowed != def))
        {
          ast_setdata(ast, narrowed);
          typeparam_set_cap(ast);
        }
      }
      return;
    }

    default: {}
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    rescope_copy_narrowings(child, entity, opt);
}

static void rescope_copied_method(ast_t** method, ast_t* entity,
  pass_opt_t* opt)
{
  rescope_copy_narrowings(*method, entity, opt);
}

static bool is_within(ast_t* ast, ast_t* outer)
{
  for(; ast != NULL; ast = ast_parent(ast))
  {
    if(ast == outer)
      return true;
  }

  return false;
}

// A body taken from another trait's method declares its locals in that
// method's scope. Declare them in the scope of the method that receives the
// body.
static void move_body_symbols(ast_t* from_method, ast_t* body,
  ast_t* to_method, pass_opt_t* opt)
{
  symtab_t* symtab = ast_get_symtab(from_method);
  if(symtab == NULL)
    return;

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  // Adding a name also adds its case-insensitive entry, which marks the
  // names it clashes with. Copying that entry itself would make the name look
  // taken, so it is left to be made again.
  while((sym = symtab_next(symtab, &i)) != NULL)
  {
    if((sym->def != NULL) && (sym->status != SYM_NOCASE) &&
      is_within(sym->def, body))
      ast_set(to_method, sym->name, sym->def, sym->status, true, opt->strtab);
  }
}

// Remove a method's symbols for the locals a body declares, when the body is
// dropped from the method.
static void remove_body_symbols(ast_t* method, ast_t* body)
{
  symtab_t* symtab = ast_get_symtab(method);
  if(symtab == NULL)
    return;

  size_t i = HASHMAP_BEGIN;
  symbol_t* sym;

  while((sym = symtab_next(symtab, &i)) != NULL)
  {
    if((sym->def != NULL) && is_within(sym->def, body))
    {
      // Removing an entry shifts a later one into its slot, so look at the
      // same slot again.
      symtab_removeindex(symtab, i);
      i--;
      POOL_FREE(symbol_t, sym);
    }
  }
}

// A body taken from another trait's method refers to that method's own type
// parameters. Point those references, and the narrowings of those parameters
// that its iftypes declare, at the parameters in the same position of the
// method that receives the body.
static void graft_body_typeparams(ast_t* ast, ast_t* body_typeparams,
  ast_t* sig_typeparams, pass_opt_t* opt)
{
  if((ast == NULL) || (ast_id(body_typeparams) != TK_TYPEPARAMS) ||
    (ast_id(sig_typeparams) != TK_TYPEPARAMS))
    return;

  // A reference to the body method's parameter itself, as a type or as a
  // value, or a narrowing of it. A reference to a narrowing keeps pointing at
  // the narrowing, which this repoints.
  token_id id = ast_id(ast);
  ast_t* def = (ast_t*)ast_data(ast);
  if((def != NULL) && (def != ast) &&
    ((id == TK_TYPEPARAM) ||
      (((id == TK_TYPEPARAMREF) || (id == TK_REFERENCE)) &&
        (ast_parent(def) == body_typeparams))))
  {
    ast_t* root = typeparam_root(def);
    int index = typeparam_root_index(body_typeparams, root);

    if(index >= 0)
    {
      ast_t* sig_tp = ast_childidx(sig_typeparams, (size_t)index);

      if(sig_tp != NULL)
      {
        ast_setdata(ast, sig_tp);

        if(id == TK_TYPEPARAMREF)
          ast_set_name(ast_child(ast), ast_name(ast_child(sig_tp)),
            opt->strtab);
      }
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    graft_body_typeparams(child, body_typeparams, sig_typeparams, opt);

  graft_body_typeparams(ast_type(ast), body_typeparams, sig_typeparams, opt);
}

// A body taken from another method refers to that method's parameters, which
// are freed with it. Point those references at the parameters in the same
// position of the method that receives the body.
static void graft_body_params(ast_t* ast, ast_t* body_params,
  ast_t* sig_params)
{
  if((ast == NULL) || (ast_id(body_params) != TK_PARAMS) ||
    (ast_id(sig_params) != TK_PARAMS))
    return;

  token_id id = ast_id(ast);
  if((id == TK_PARAMREF) || (id == TK_REFERENCE))
  {
    ast_t* def = (ast_t*)ast_data(ast);

    if((def != NULL) && (ast_parent(def) == body_params))
    {
      ast_t* sig_param = ast_childidx(sig_params, ast_index(def));

      if(sig_param != NULL)
        ast_setdata(ast, sig_param);
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    graft_body_params(child, body_params, sig_params);
}

// The provided method must already be reified.
static bool merge_inherited_method(ast_t* entity, ast_t* existing_method,
  ast_t* method, ast_t* trait_ref, pass_opt_t* opt)
{
  pony_assert(entity != NULL);
  pony_assert(existing_method != NULL);
  pony_assert(method != NULL);
  pony_assert(trait_ref != NULL);

  AST_GET_CHILDREN(method, cap, id, t_params, params, result, guard, error,
    method_body);

  const char* method_name = ast_name(id);
  method_t* info = (method_t*)ast_data(existing_method);
  pony_assert(info != NULL);

  // Method has already caused an error, do nothing.
  if(info->failed)
    return false;

  if(info->local_define)
    return true;

  if(!compare_signature_nodes(existing_method, method,
    COMPARE_INHERITED_SIGNATURE))
  {
    pony_assert(info->trait_ref != NULL);

    ast_error(opt->check.errors, trait_ref,
      "clashing definitions for method '%s' provided, local disambiguation "
      "required",
      method_name);
    ast_error_continue(opt->check.errors, trait_ref,
      "provided here, type: %s",
      ast_print_type(method, opt->strtab));
    ast_error_continue(opt->check.errors, info->trait_ref,
      "and here, type: %s",
      ast_print_type(existing_method, opt->strtab));

    info->failed = true;
    return false;
  }

  // Resolve bodies, if any.
  ast_t* existing_body = ast_childidx(existing_method, 7);

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
    {
      remove_body_symbols(existing_method, existing_body);
      ast_replace(&existing_body, ast_from(existing_method, TK_NONE));
    }

    info->body_donor = NULL;
    return true;
  }

  // No new body to resolve.
  if((ast_id(method_body) == TK_NONE) ||
    (info->body_donor == (ast_t*)ast_data(method)))
    return true;

  // Trait provides default body. Use it and patch up symbol tables.
  pony_assert(ast_id(existing_body) == TK_NONE);
  // Take the body out of the provided method instead of copying it, so the
  // symbols that refer to its nodes stay valid.
  move_body_symbols(method, method_body, existing_method, opt);
  ast_swap(method_body, ast_from(method_body, TK_NONE));
  ast_replace(&existing_body, method_body);
  graft_body_typeparams(existing_body, ast_childidx(method, 2),
    ast_childidx(existing_method, 2), opt);
  graft_body_params(existing_body, ast_childidx(method, 3),
    ast_childidx(existing_method, 3));
  mutation_inherit_body_selector(method, existing_body);

  rescope_copied_method(&existing_method, entity, opt);

  info->body_donor = (ast_t*)ast_data(method);
  info->trait_ref = trait_ref;
  return true;
}


static bool add_method_from_trait(ast_t* entity, ast_t* method,
  ast_t* trait_ref, pass_opt_t* opt)
{
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = find_method(entity, name);

  if(existing != NULL)
  {
    if(ast_id(existing) == TK_METHODGROUP)
      existing = ast_child(existing);

    return merge_inherited_method(entity, existing, method, trait_ref, opt);
  }

  ast_t* local = add_method(entity, trait_ref, method, "provided", opt);

  if(local == NULL)
    return false;

  rescope_copied_method(&local, entity, opt);
  return true;
}


static bool add_methodgroup_from_trait(ast_t* entity, ast_t* group,
  ast_t* trait_ref, pass_opt_t* opt)
{
  pony_assert(ast_id(group) == TK_METHODGROUP);

  ast_t* default_method = ast_child(group);
  pony_assert(default_method != NULL);

  const char* method_name = ast_name(ast_childidx(default_method, 1));
  ast_t* existing = find_method(entity, method_name);

  if((existing != NULL) && (ast_id(existing) == TK_METHODGROUP))
  {
    // Semantic guard equality can require structural subtyping, which
    // depends on the completed traits pass.
    for(ast_t* m = default_method; m != NULL; m = ast_sibling(m))
    {
      if(!merge_inherited_method(entity, ast_child(existing), m, trait_ref,
        opt))
        return false;
    }

    return true;
  }


  if(existing == NULL)
  {
    ast_append(ast_childidx(entity, 4), group);
    ast_set(entity, method_name, group, SYM_DEFINED, false, opt->strtab);
  }
  else
  {
    ast_t* match = default_method;
    ast_t* guard = ast_childidx(existing, 5);

    while((match != NULL) && !compare_signature_nodes(guard,
      ast_childidx(match, 5), COMPARE_GUARDS))
      match = ast_sibling(match);

    if((match != NULL) &&
      !merge_inherited_method(entity, existing, match, trait_ref, opt))
      return false;

    ast_swap(existing, group);
    symtab_replace(ast_get_symtab(entity), method_name, group);

    if(match != NULL)
    {
      ast_swap(match, existing);
      ast_free_unattached(match);
    }
    else
    {
      ast_add_sibling(default_method, existing);
    }
  }

  for(ast_t* m = ast_child(group); m != NULL; m = ast_sibling(m))
  {
    if(m == existing)
      continue;

    ast_t* donor = (ast_t*)ast_data(m);
    method_t* info = attach_method_t(m);
    info->trait_ref = trait_ref;

    if(ast_id(ast_childidx(m, 7)) != TK_NONE)
      info->body_donor = donor;

    ast_t* doc = ast_childidx(m, 8);
    if(ast_id(doc) == TK_STRING)
    {
      ast_set_name(doc, "", opt->strtab);
      ast_setid(doc, TK_NONE);
      ast_settype(doc, NULL);
    }

    rescope_copied_method(&m, entity, opt);
  }

  return true;
}

// Whether a definition found for a name in a trait's own text means the same
// definition in every copy of the method: a type, a type alias, a package or a
// type parameter. Other names (locals, parameters, fields and methods) mean
// whatever the copy's own scope gives them.
static bool is_written_binding(ast_t* def)
{
  switch(ast_id(def))
  {
    case TK_MODULE:
    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_TYPE:
    case TK_TYPEPARAM:
    case TK_PRIMITIVE:
    case TK_STRUCT:
    case TK_CLASS:
    case TK_ACTOR:
      return true;

    default:
      return false;
  }
}

typedef struct literal_names_t
{
  const char** names;
  size_t count;
  size_t size;
  struct literal_names_t* outer;
} literal_names_t;

static void add_literal_name(literal_names_t* scope, const char* name)
{
  if(scope->count == scope->size)
  {
    size_t size = (scope->size == 0) ? 8 : scope->size * 2;
    const char** names = (const char**)ponyint_pool_alloc_size(
      size * sizeof(const char*));

    if(scope->count > 0)
    {
      memcpy(names, scope->names, scope->count * sizeof(const char*));
      ponyint_pool_free_size(scope->size * sizeof(const char*), scope->names);
    }

    scope->names = names;
    scope->size = size;
  }

  scope->names[scope->count++] = name;
}

// The names declared anywhere in the text of an object literal or lambda. The
// scope pass has not run on that text, so a name declared there is not in any
// symbol table yet.
static void collect_literal_names(ast_t* ast, literal_names_t* scope)
{
  switch(ast_id(ast))
  {
    case TK_LET:
    case TK_VAR:
    case TK_PARAM:
    case TK_FVAR:
    case TK_FLET:
    case TK_EMBED:
    case TK_TYPEPARAM:
    case TK_MATCH_CAPTURE:
    case TK_LAMBDACAPTURE:
      if(ast_id(ast_child(ast)) == TK_ID)
        add_literal_name(scope, ast_name(ast_child(ast)));
      break;

    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      if(ast_id(ast_childidx(ast, 1)) == TK_ID)
        add_literal_name(scope, ast_name(ast_childidx(ast, 1)));
      break;

    default: {}
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
    collect_literal_names(child, scope);
}

static bool is_literal_name(literal_names_t* scope, const char* name)
{
  for(; scope != NULL; scope = scope->outer)
  {
    for(size_t i = 0; i < scope->count; i++)
    {
      if(scope->names[i] == name)
        return true;
    }
  }

  return false;
}

static void bind_written_names_in(ast_t** astp, literal_names_t* literal,
  pass_opt_t* opt);

// Whether every type named in an iftype's subject is a nominal declared
// outside the literal.
static bool written_iftype_subject(ast_t* subject, literal_names_t* literal)
{
  if(ast_id(subject) == TK_TUPLETYPE)
  {
    for(ast_t* p = ast_child(subject); p != NULL; p = ast_sibling(p))
    {
      if(!written_iftype_subject(p, literal))
        return false;
    }

    return true;
  }

  return (ast_id(subject) == TK_NOMINAL) &&
    (ast_id(ast_child(subject)) == TK_NONE) &&
    !is_literal_name(literal, ast_name(ast_childidx(subject, 1)));
}

static void bind_written_children(ast_t* ast, literal_names_t* literal,
  pass_opt_t* opt)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    ast_t* next = ast_sibling(child);
    bind_written_names_in(&child, literal, opt);
    child = next;
  }
}

static void bind_written_names_in(ast_t** astp, literal_names_t* literal,
  pass_opt_t* opt)
{
  ast_t* ast = *astp;

  switch(ast_id(ast))
  {
    case TK_OBJECT:
    case TK_LAMBDA:
    case TK_BARELAMBDA:
    {
      literal_names_t inner = {NULL, 0, 0, literal};
      collect_literal_names(ast, &inner);
      bind_written_children(ast, &inner, opt);

      if(inner.size > 0)
        ponyint_pool_free_size(inner.size * sizeof(const char*), inner.names);
      return;
    }

    case TK_REFERENCE:
    {
      const char* name = ast_name(ast_child(ast));

      if((ast_data(ast) == NULL) && !is_name_dontcare(name) &&
        !is_literal_name(literal, name))
      {
        ast_t* def = ast_get(ast, name, NULL);

        if((def != NULL) && is_written_binding(def))
          ast_setdata(ast, def);
      }
      return;
    }

    case TK_IFTYPE:
    {
      // An iftype in literal text narrows a type parameter of the enclosing
      // method or type. Narrow it here, where it is written, so the names in
      // the iftype resolve to the narrowing before the method is copied.
      if((literal != NULL) &&
        (ast_id(ast_childidx(ast, 3)) == TK_NONE) &&
        written_iftype_subject(ast_child(ast), literal))
      {
        errors_t* errors = opt->check.errors;
        opt->check.errors = errors_alloc();
        scope_written_iftype(opt, ast);
        errors_free(opt->check.errors);
        opt->check.errors = errors;
      }
      break;
    }

    case TK_NOMINAL:
    {
      // The names pass resolved the types written outside object literals and
      // lambdas. Resolve those inside them here, where they are written; the
      // names pass runs on that text only when the literal is expanded, in
      // whatever type the method was copied into. A name declared in the
      // literal itself is left for that pass.
      //
      // A name that doesn't resolve, or names a type its writer can't use,
      // is left as written, so the names pass reports it where it reports
      // the same error outside a literal.
      if((literal != NULL) && (ast_data(ast) == NULL) &&
        !is_literal_name(literal, ast_name(ast_childidx(ast, 1))))
      {
        ast_t* written = ast_dup(ast);
        errors_t* errors = opt->check.errors;
        opt->check.errors = errors_alloc();
        bool ok = names_nominal(opt, ast, astp, false);
        errors_free(opt->check.errors);
        opt->check.errors = errors;

        if(ok)
          ast_free(written);
        else
          ast_replace(astp, written);

        ast = *astp;
      }

      // The names pass resolves a nominal's type arguments with it.
      if(ast_id(ast) != TK_NOMINAL || ast_data(ast) != NULL)
        return;
      break;
    }

    default: {}
  }

  bind_written_children(ast, literal, opt);
}

// Bind the names in a trait's or interface's own methods that mean a type, a
// type alias, a package or a type parameter to the definition they have where
// the method is written. Copies of the method keep these bindings, so a name
// in an inherited body means the same thing in every type that inherits it,
// whatever that type's own file and package declare.
static void bind_written_names(ast_t* entity, pass_opt_t* opt)
{
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p) || (ast_id(p) == TK_METHODGROUP))
      bind_written_children(p, NULL, opt);
  }
}

// Process the methods provided to the given entity from all traits in its
// provides list.
static bool provided_methods(ast_t* entity, pass_opt_t* opt)
{
  pony_assert(entity != NULL);

  ast_t* provides = ast_childidx(entity, 3);
  bool r = true;

  // Run through our provides list
  for(ast_t* trait_ref = ast_child(provides); trait_ref != NULL;
    trait_ref = ast_sibling(trait_ref))
  {
    ast_t* trait = (ast_t*)ast_data(trait_ref);
    pony_assert(trait != NULL);

    if(!trait_entity(trait, opt))
      return false;

    ast_t* members = ast_childidx(trait, 4);

    // Run through the methods of each provided type.
    for(ast_t* member = ast_child(members); member != NULL;
      member = ast_sibling(member))
    {
      if(ast_id(member) == TK_METHODGROUP)
      {
        // Build a reified copy of the entire group.
        ast_t* reified_group = ast_from(member, TK_METHODGROUP);
        bool group_ok = true;

        for(ast_t* method = ast_child(member); method != NULL;
          method = ast_sibling(method))
        {
          pony_assert(is_method(method));
          ast_t* reified = reify_provides_type(entity,
            inherited_source(entity, method, opt), trait_ref, opt);

          if(reified == NULL)
          {
            group_ok = false;
            break;
          }

          ast_append(reified_group, reified);
        }

        if(group_ok)
        {
          if(!add_methodgroup_from_trait(entity, reified_group, trait_ref, opt))
            r = false;
        }
        else
        {
          r = false;
        }

        ast_free_unattached(reified_group);
      }
      else
      {
        pony_assert(is_method(member));

        ast_t* reified = reify_provides_type(entity,
          inherited_source(entity, member, opt), trait_ref, opt);

        if(reified == NULL)
        {
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
  }

  return r;
}


static bool is_local_decl(token_id id)
{
  return id == TK_VAR || id == TK_LET || id == TK_PARAM ||
    id == TK_MATCH_CAPTURE;
}

static bool is_entity_method(token_id id)
{
  return id == TK_FUN || id == TK_BE || id == TK_NEW ||
    id == TK_METHODGROUP;
}

// Walk an AST subtree checking each scope's local declarations against the
// entity's member table. Stops at nested entities (object literals, lambdas).
static bool check_shadows_in_subtree(ast_t* ast, ast_t* entity,
  pass_opt_t* opt)
{
  if(ast == NULL)
    return true;

  token_id id = ast_id(ast);
  if(id == TK_OBJECT || id == TK_LAMBDA || id == TK_BARELAMBDA)
    return true;

  bool ok = true;

  symtab_t* symtab = ast_get_symtab(ast);
  if(symtab != NULL)
  {
    symtab_t* entity_symtab = ast_get_symtab(entity);
    pony_assert(entity_symtab != NULL);

    size_t i = HASHMAP_BEGIN;
    symbol_t* sym;

    while((sym = symtab_next(symtab, &i)) != NULL)
    {
      if(sym->def == NULL || sym->status == SYM_NOCASE)
        continue;

      if(!is_local_decl(ast_id(sym->def)))
        continue;

      sym_status_t member_status;
      ast_t* member = symtab_find(entity_symtab, sym->name, &member_status);

      if(member != NULL && is_entity_method(ast_id(member)))
      {
        ast_error(opt->check.errors, sym->def,
          "can't reuse name '%s'", sym->name);
        ast_error_continue(opt->check.errors, member,
          "previous use of '%s'", sym->name);
        ok = false;
      }

      if(member == NULL)
      {
        ast_t* member_nocase = symtab_find_case(entity_symtab, sym->name,
          NULL, opt->strtab);

        if(member_nocase != NULL && is_entity_method(ast_id(member_nocase)))
        {
          const char* member_name;
          if(is_method(member_nocase))
            member_name = ast_name(ast_childidx(member_nocase, 1));
          else if(ast_id(member_nocase) == TK_METHODGROUP)
            member_name = ast_name(ast_childidx(ast_child(member_nocase), 1));
          else
            member_name = ast_name(ast_child(member_nocase));

          ast_error(opt->check.errors, sym->def,
            "can't reuse name '%s'", sym->name);
          ast_error_continue(opt->check.errors, member_nocase,
            "previous use of '%s' differs only by case", member_name);
          ok = false;
        }
      }
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(!check_shadows_in_subtree(child, entity, opt))
      ok = false;
  }

  return ok;
}

// Check all method bodies for locals that shadow entity members. Called after
// provided_methods has populated the entity's symbol table.
//
// set_scope (scope.c) suppresses method-vs-local conflicts during the scope
// pass and defers them here. Removing this check causes those conflicts to be
// silently accepted.
static bool check_member_shadowing(ast_t* entity, pass_opt_t* opt)
{
  ast_t* members = ast_childidx(entity, 4);
  bool ok = true;

  for(ast_t* member = ast_child(members); member != NULL;
    member = ast_sibling(member))
  {
    if(is_method(member))
    {
      if(!check_shadows_in_subtree(member, entity, opt))
        ok = false;
    }
    else if(ast_id(member) == TK_METHODGROUP)
    {
      for(ast_t* m = ast_child(member); m != NULL; m = ast_sibling(m))
      {
        if(!check_shadows_in_subtree(m, entity, opt))
          ok = false;
      }
    }
  }

  return ok;
}


// Check that the given entity, if concrete, has bodies for all methods.
static bool check_concrete_bodies(ast_t* entity, pass_opt_t* opt)
{
  pony_assert(entity != NULL);

  token_id variety = ast_id(entity);
  if((variety != TK_PRIMITIVE) && (variety != TK_STRUCT) &&
    (variety != TK_CLASS) && (variety != TK_ACTOR))
    return true;

  bool r = true;
  ast_t* members = ast_childidx(entity, 4);
  pony_assert(members != NULL);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      method_t* info = (method_t*)ast_data(p);
      pony_assert(info != NULL);

      if(!info->failed)
      {
        const char* name = ast_name(ast_childidx(p, 1));

        if(ast_checkflag(p, AST_FLAG_AMBIGUOUS))
        {
          ast_error(opt->check.errors, entity, "multiple possible bodies for "
            "method %s, local disambiguation required", name);
          r = false;
        }
        else if(info->body_donor == NULL)
        {
          pony_assert(info->trait_ref != NULL);
          ast_error(opt->check.errors, info->trait_ref,
            "no body found for method '%s'", name);
          r = false;
        }
      }
    }
    else if(ast_id(p) == TK_METHODGROUP)
    {
      for(ast_t* m = ast_child(p); m != NULL; m = ast_sibling(m))
      {
        method_t* info = (method_t*)ast_data(m);
        pony_assert(info != NULL);

        if(!info->failed)
        {
          const char* name = ast_name(ast_childidx(m, 1));

          if(ast_checkflag(m, AST_FLAG_AMBIGUOUS))
          {
            ast_error(opt->check.errors, entity,
              "multiple possible bodies for "
              "method %s, local disambiguation required", name);
            r = false;
          }
          else if(info->body_donor == NULL)
          {
            pony_assert(info->trait_ref != NULL);
            ast_error(opt->check.errors, info->trait_ref,
              "no body found for method '%s'", name);
            r = false;
          }
        }
      }
    }
  }

  return r;
}


bool is_trait_view_method(ast_t* method, pass_opt_t* opt)
{
  switch(ast_id(method))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      return ast_has_annotation(method, TRAIT_VIEW_ANNOTATION, opt->strtab);

    default:
      return false;
  }
}

bool is_copy_method(ast_t* entity, ast_t* method, pass_opt_t* opt)
{
  if(entity == NULL)
    return false;

  if(ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab))
    return true;

  if(method == NULL)
    return false;

  ast_t* donor = (ast_t*)ast_data(method);
  return (donor != NULL) && (donor != entity) &&
    ((ast_id(donor) == TK_TRAIT) || (ast_id(donor) == TK_INTERFACE));
}

bool is_copy_context(pass_opt_t* opt)
{
  return is_copy_method(opt->check.frame->type, opt->check.frame->method,
    opt);
}

// The provides entry of `entity` through which it inherited a method whose
// body was written in `donor`.
static ast_t* inherited_through(ast_t* entity, ast_t* method, ast_t* donor)
{
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* provides = ast_childidx(entity, 3);

  for(ast_t* ref = ast_child(provides); ref != NULL; ref = ast_sibling(ref))
  {
    ast_t* trait = (ast_t*)ast_data(ref);

    if(trait == donor)
      return ref;

    if(trait == NULL)
      continue;

    ast_t* member = find_method(trait, name);
    if((member != NULL) && (ast_id(member) == TK_METHODGROUP))
      member = ast_child(member);

    if((member != NULL) && (ast_data(member) == donor))
      return ref;
  }

  return ast_child(provides);
}

// In `ast`, the node with token `id` written at the same place as `at`.
static ast_t* node_at(ast_t* ast, token_id id, ast_t* at)
{
  if((ast_id(ast) == id) && (ast_source(ast) == ast_source(at)) &&
    (ast_line(ast) == ast_line(at)) && (ast_pos(ast) == ast_pos(at)))
    return ast;

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
  {
    ast_t* found = node_at(c, id, at);
    if(found != NULL)
      return found;
  }

  return NULL;
}

ast_t* provider_view_type(pass_opt_t* opt, ast_t* node)
{
  ast_t* entity = opt->check.frame->type;
  ast_t* method = opt->check.frame->method;

  if((entity == NULL) || (method == NULL) ||
    ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab))
    return NULL;

  ast_t* donor = (ast_t*)ast_data(method);
  if((donor == NULL) || (donor == entity) ||
    ((ast_id(donor) != TK_TRAIT) && (ast_id(donor) != TK_INTERFACE)))
    return NULL;

  // The trait's own method, as its check typed it.
  ast_t* trait_method = node_at(ast_childidx(donor, 4), ast_id(method),
    method);
  ast_t* found = (trait_method != NULL) ?
    node_at(ast_childidx(trait_method, 7), ast_id(node), node) : NULL;
  ast_t* type = (found != NULL) ? ast_type(found) : NULL;

  if((type == NULL) || (ast_id(type) == TK_ERRORTYPE))
    return NULL;

  ast_t* entry = inherited_through(entity, method, donor);
  if((entry == NULL) || (ast_data(entry) != donor))
    return NULL;

  return reify(type, ast_childidx(donor, 1), ast_childidx(entry, 2), opt,
    true);
}

ast_t* inherited_provides_entry(ast_t* entity, ast_t* method, pass_opt_t* opt)
{
  ast_t* host_entity;
  ast_t* host_method;

  // A class made from a literal in an inherited method inherits through its
  // host's entry.
  while(ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab) &&
    program_anon_host(entity, &host_entity, &host_method))
  {
    entity = host_entity;
    method = host_method;
  }

  ast_t* donor = (ast_t*)ast_data(method);
  if((donor == NULL) || (donor == entity) ||
    ((ast_id(donor) != TK_TRAIT) && (ast_id(donor) != TK_INTERFACE)))
    return NULL;

  return inherited_through(entity, method, donor);
}

ast_t* method_entity(ast_t* method)
{
  ast_t* members = ast_parent(method);

  if((members != NULL) && (ast_id(members) == TK_METHODGROUP))
    members = ast_parent(members);

  return (members != NULL) ? ast_parent(members) : NULL;
}

// For a method of an ANON_COPY class, the body in the host method that holds
// the literal the class was made from.
static ast_t* anon_site(ast_t* method, pass_opt_t* opt)
{
  ast_t* entity = method_entity(method);

  if((entity == NULL) ||
    !ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab))
    return NULL;

  return program_anon_site(entity);
}

bool is_in_unrunnable_code(ast_t* ast, pass_opt_t* opt)
{
  for(ast_t* p = ast; (p != NULL) && (ast_id(p) != TK_MEMBERS);
    p = ast_parent(p))
  {
    if(is_decided_false_branch(p, opt))
      return true;
  }

  return false;
}

bool is_unverified_branch(ast_t* ast, pass_opt_t* opt)
{
  if(!is_decided_false_branch(ast, opt))
    return false;

  if(is_copy_context(opt))
    return true;

  for(ast_t* p = ast_parent(ast); p != NULL; p = ast_parent(p))
  {
    token_id id = ast_id(p);
    if((id == TK_FUN) || (id == TK_BE) || (id == TK_NEW))
      break;

    if((id == TK_SEQ) &&
      ast_has_annotation(p, COPIED_DEFAULT_ANNOTATION, opt->strtab))
      return true;
  }

  return false;
}

bool is_unreachable_anon_class(ast_t* entity, pass_opt_t* opt)
{
  if(!ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab))
    return false;

  for(ast_t* p = program_anon_site(entity); p != NULL; p = ast_parent(p))
  {
    if(is_decided_false_branch(p, opt))
      return true;

    token_id id = ast_id(p);
    if((id == TK_FUN) || (id == TK_BE) || (id == TK_NEW))
      break;
  }

  return false;
}

bool iftype_holds(ast_t* subject, ast_t* super, pass_opt_t* opt)
{
  return is_subtype_constraint(subject, super, NULL, opt);
}

// Whether a method guard is false for its concrete subjects.
static bool guard_decided_false(ast_t* guard, pass_opt_t* opt)
{
  switch(ast_id(guard))
  {
    case TK_IFTYPEGUARD:
    {
      AST_GET_CHILDREN(guard, subject, super);
      return !type_mentions_typeparam(subject) &&
        !iftype_holds(subject, super, opt);
    }

    case TK_IFTYPEGUARD_AND:
      for(ast_t* g = ast_child(guard); g != NULL; g = ast_sibling(g))
      {
        if(guard_decided_false(g, opt))
          return true;
      }
      return false;

    case TK_IFTYPEGUARD_OR:
      for(ast_t* g = ast_child(guard); g != NULL; g = ast_sibling(g))
      {
        if(!guard_decided_false(g, opt))
          return false;
      }
      return true;

    default:
      return false;
  }
}

bool is_decided_false_branch(ast_t* body, pass_opt_t* opt)
{
  ast_t* parent = ast_parent(body);

  if(parent == NULL)
    return false;

  token_id id = ast_id(parent);

  if(((id == TK_FUN) || (id == TK_BE) || (id == TK_NEW)) &&
    (ast_childidx(parent, 7) == body))
  {
    if(guard_decided_false(ast_childidx(parent, 5), opt))
      return true;

    ast_t* entity = method_entity(parent);
    return (entity != NULL) && is_unreachable_anon_class(entity, opt);
  }

  if((ast_id(parent) == TK_IFTYPE) && (ast_childidx(parent, 2) == body))
  {
    AST_GET_CHILDREN(parent, subject, super);
    return !type_mentions_typeparam(subject) &&
      !iftype_holds(subject, super, opt);
  }

  if((ast_id(parent) == TK_IFTYPE_SET) && (ast_childidx(parent, 1) == body))
  {
    ast_t* iftype = ast_child(parent);
    AST_GET_CHILDREN(iftype, subject, super);
    return iftype_holds(subject, super, opt);
  }

  return false;
}

// Call `fn` for one narrowing: an iftype's or a guard's, unless its subject is
// a tuple or it holds for the subject already.
static bool visit_narrowing(pass_opt_t* opt, ast_t* subject, ast_t* super,
  ast_t* typeparams, narrowing_fn fn, void* ctx)
{
  if((ast_id(subject) == TK_TUPLETYPE) ||
    iftype_holds(subject, super, opt))
    return false;

  return fn(subject, super, typeparams, ctx);
}

bool visit_copy_narrowings(pass_opt_t* opt, ast_t* at, narrowing_fn fn,
  void* ctx)
{
  if(!is_copy_context(opt))
    return false;

  ast_t* body = at;

  while(body != NULL)
  {
    ast_t* parent = ast_parent(body);

    if(parent == NULL)
      break;

    token_id id = ast_id(parent);

    if((id == TK_FUN) || (id == TK_BE) || (id == TK_NEW))
    {
      if(ast_childidx(parent, 7) != body)
        break;

      // The method's guard narrows its body, an `and` guard by each part.
      ast_t* guard = ast_childidx(parent, 5);

      if(ast_id(guard) == TK_IFTYPEGUARD)
      {
        AST_GET_CHILDREN(guard, subject, super, typeparams);
        if(visit_narrowing(opt, subject, super, typeparams, fn, ctx))
          return true;
      } else if(ast_id(guard) == TK_IFTYPEGUARD_AND) {
        for(ast_t* g = ast_child(guard); g != NULL; g = ast_sibling(g))
        {
          AST_GET_CHILDREN(g, subject, super, typeparams);
          if(visit_narrowing(opt, subject, super, typeparams, fn, ctx))
            return true;
        }
      } else if(ast_id(guard) == TK_IFTYPEGUARD_OR) {
        // An `or` guard narrows its subject to the union of the parts'
        // supertypes, held by the first part.
        ast_t* first = ast_child(guard);
        AST_GET_CHILDREN(first, subject, first_super, typeparams);
        ast_t* supers = ast_from(guard, TK_UNIONTYPE);

        for(ast_t* g = first; g != NULL; g = ast_sibling(g))
          ast_append(supers, ast_dup(ast_childidx(g, 1)));

        bool stop = visit_narrowing(opt, subject, supers, typeparams, fn,
          ctx);
        ast_free_unattached(supers);

        if(stop)
          return true;
      }

      // A class made from a literal is narrowed as the code that held the
      // literal is.
      body = anon_site(parent, opt);
      continue;
    }

    if((id == TK_IFTYPE) && (ast_childidx(parent, 2) == body))
    {
      AST_GET_CHILDREN(parent, subject, super, then_body, typeparams);

      if(visit_narrowing(opt, subject, super, typeparams, fn, ctx))
        return true;
    }

    body = parent;
  }

  return false;
}

typedef struct
{
  pass_opt_t* opt;
  ast_t* sub;
  ast_t* super;
} narrowed_subtype_t;

static bool narrowed_is_subtype(ast_t* subject, ast_t* narrowing,
  ast_t* typeparams, void* ctx)
{
  (void)subject;
  (void)typeparams;
  narrowed_subtype_t* c = (narrowed_subtype_t*)ctx;
  ast_t* narrowed = type_isect(c->opt, c->sub, narrowing);
  bool ok = is_subtype(narrowed, c->super, NULL, c->opt);
  ast_free_unattached(narrowed);
  return ok;
}

bool is_subtype_via_narrowing(pass_opt_t* opt, ast_t* at, ast_t* sub,
  ast_t* super)
{
  narrowed_subtype_t c = {opt, sub, super};
  return visit_copy_narrowings(opt, at, narrowed_is_subtype, &c);
}

const char* inherited_provider_name(ast_t* entity, ast_t* method)
{
  ast_t* donor = (ast_t*)ast_data(method);

  if((donor == NULL) || (donor == entity) ||
    ((ast_id(donor) != TK_TRAIT) && (ast_id(donor) != TK_INTERFACE)))
    return NULL;

  ast_t* ref = inherited_through(entity, method, donor);
  ast_t* trait = (ref != NULL) ? (ast_t*)ast_data(ref) : NULL;

  if(trait == NULL)
    return NULL;

  return ast_name(ast_child(trait));
}

// Errors a trait's own check reported, such as one started on demand while an
// inherited method was checked, are the trait's; errors a default argument's
// check started on demand reported are that default's method's.
static bool error_without_copy_note(size_t index, void* ctx)
{
  return program_error_without_copy_note((ast_t*)ctx, index);
}

void copy_check_enter(ast_t* method, pass_opt_t* opt)
{
  ast_t* entity = opt->check.frame->type;
  ast_t* module = opt->check.frame->module;

  if((module != NULL) && is_copy_method(entity, method, opt))
    program_push_copy_check(module, entity, method,
      errors_get_count(opt->check.errors));
}

void copy_check_leave(ast_t* method, pass_opt_t* opt)
{
  ast_t* entity;
  size_t count;

  ast_t* module = opt->check.frame->module;

  if((module == NULL) ||
    !program_pop_copy_check(module, method, &entity, &count) ||
    (errors_get_count(opt->check.errors) == count))
    return;

  // A class made from a literal in an inherited method reports as that
  // method.
  ast_t* host_entity;
  ast_t* host_method;
  while(ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab) &&
    program_anon_host(entity, &host_entity, &host_method))
  {
    entity = host_entity;
    method = host_method;
  }

  ast_t* donor = (ast_t*)ast_data(method);
  if((donor == NULL) || (donor == entity) ||
    ((ast_id(donor) != TK_TRAIT) && (ast_id(donor) != TK_INTERFACE)))
    return;

  ast_t* at = inherited_through(entity, method, donor);
  if(at == NULL)
    return;

  const char* inheritor = ast_name(ast_child(entity));
  if(is_name_internal_test(inheritor) || (inheritor[0] == '$'))
    inheritor = "object literal";

  errors_add_copy_note(opt->check.errors, count, error_without_copy_note,
    module, ast_source(at),
    ast_line(at), ast_pos(at),
    COPY_NOTE_START "%s" COPY_NOTE_INHERITED "%s from %s",
    ast_name(ast_childidx(method, 1)), inheritor,
    ast_name(ast_child(donor)));
}

static bool is_trait_view_entity(ast_t* entity, pass_opt_t* opt)
{
  return ast_has_annotation(entity, ANON_TRAIT_ANNOTATION, opt->strtab) ||
    ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab);
}

// A class made from a literal in trait text inherits each default as the
// providing trait's checked method, with the trait's type parameters
// substituted. Mark those methods so they are not checked again.
static void mark_trait_view_methods(ast_t* entity, pass_opt_t* opt)
{
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    if(is_method(p))
    {
      if(ast_data(p) != entity)
        ast_add_internal_annotation(p, TRAIT_VIEW_ANNOTATION, opt->strtab);
    } else if(ast_id(p) == TK_METHODGROUP) {
      for(ast_t* m = ast_child(p); m != NULL; m = ast_sibling(m))
      {
        if(ast_data(m) != entity)
          ast_add_internal_annotation(m, TRAIT_VIEW_ANNOTATION, opt->strtab);
      }
    }
  }
}

// A deferred class takes its inherited methods' signatures now and their
// bodies once its providers' checks have finished. Drop the bodies copied from
// providers whose checks are still running.
static void strip_inherited_bodies(ast_t* entity)
{
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    ast_t* m = (ast_id(p) == TK_METHODGROUP) ? ast_child(p) : p;

    for(; m != NULL; m = (ast_id(p) == TK_METHODGROUP) ? ast_sibling(m) : NULL)
    {
      if(!is_method(m) || (ast_data(m) == entity))
        continue;

      ast_t* body = ast_childidx(m, 7);
      if(ast_id(body) != TK_NONE)
      {
        remove_body_symbols(m, body);
        ast_replace(&body, ast_from(body, TK_NONE));
      }
    }
  }
}

// The method of `trait` that supplies the body of `method`, an inherited
// method of a deferred class: the member with the same name (and, in a method
// group, the same guard) whose body was written in the same type.
static ast_t* body_provider(ast_t* trait, ast_t* method)
{
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* member = find_method(trait, name);

  if(member == NULL)
    return NULL;

  if(ast_id(member) == TK_METHODGROUP)
  {
    for(ast_t* m = ast_child(member); m != NULL; m = ast_sibling(m))
    {
      if(compare_signature_nodes(ast_childidx(m, 5), ast_childidx(method, 5),
        COMPARE_GUARDS))
      {
        member = m;
        break;
      }
    }

    if(ast_id(member) == TK_METHODGROUP)
      return NULL;
  }

  if((ast_data(member) != ast_data(method)) ||
    (ast_id(ast_childidx(member, 7)) == TK_NONE))
    return NULL;

  return member;
}

static bool attach_body(ast_t* entity, ast_t* method, pass_opt_t* opt)
{
  ast_t* provides = ast_childidx(entity, 3);

  // The first provider in provides order that supplies the body.
  for(ast_t* trait_ref = ast_child(provides); trait_ref != NULL;
    trait_ref = ast_sibling(trait_ref))
  {
    ast_t* trait = (ast_t*)ast_data(trait_ref);
    ast_t* member = body_provider(trait, method);

    if(member == NULL)
      continue;

    ast_t* reified = reify_provides_type(entity, member, trait_ref, opt);
    if(reified == NULL)
      return false;

    // The signature taken when the class was made came from a method that
    // had not been checked. Take the checked method's type parameters,
    // parameters, result, guard and body, which refer to one another, into
    // the method node the class's own code already refers to.
    static const size_t taken[] = {2, 3, 4, 5, 7};
    for(size_t i = 0; i < sizeof(taken) / sizeof(taken[0]); i++)
    {
      ast_t* from = ast_childidx(reified, taken[i]);
      ast_t* to = ast_childidx(method, taken[i]);
      ast_swap(from, ast_from(from, TK_NONE));
      ast_replace(&to, from);
    }

    ast_clear_local(method);
    symtab_t* symtab = ast_get_symtab(reified);
    size_t i = HASHMAP_BEGIN;
    symbol_t* sym;

    // A case-insensitive entry is made again by adding its name.
    while((sym = symtab_next(symtab, &i)) != NULL)
    {
      if(sym->status != SYM_NOCASE)
        ast_set(method, sym->name, sym->def, sym->status, true, opt->strtab);
    }

    mutation_inherit_body_selector(reified, ast_childidx(method, 7));
    ast_free_unattached(reified);
    return true;
  }

  return true;
}

bool attach_trait_view_bodies(ast_t* entity, pass_opt_t* opt)
{
  ast_t* members = ast_childidx(entity, 4);
  mutation_type_policy_t policy = mutation_type_policy(entity);
  bool ok = true;

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    ast_t* m = (ast_id(p) == TK_METHODGROUP) ? ast_child(p) : p;

    for(; m != NULL; m = (ast_id(p) == TK_METHODGROUP) ? ast_sibling(m) : NULL)
    {
      if(!is_method(m) || (ast_data(m) == entity) ||
        (ast_id(ast_childidx(m, 7)) != TK_NONE))
        continue;

      if(!attach_body(entity, m, opt))
        ok = false;
      else if(!mutation_resolve_method(opt, &policy, m))
        ok = false;
    }
  }

  return ok;
}

// Keep a closed copy of each method of a trait or interface, taken once its
// methods are complete and their names bound where written, before any check
// of them. A type made after the trait's check (an object literal in ordinary
// code) inherits copies of these, checked for that type.
static void make_templates(ast_t* entity)
{
  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    ast_t* m = (ast_id(p) == TK_METHODGROUP) ? ast_child(p) : p;

    for(; m != NULL; m = (ast_id(p) == TK_METHODGROUP) ? ast_sibling(m) : NULL)
    {
      if(is_method(m))
        program_set_template(entity, m, ast_dup_closed(m));
    }
  }
}

// The method a type inherits a copy of: the template of the trait's method,
// unless the type takes the trait's checked methods (a class made from a
// literal in trait text), or the trait has no templates.
static ast_t* inherited_source(ast_t* entity, ast_t* member, pass_opt_t* opt)
{
  if(is_trait_view_entity(entity, opt))
    return member;

  // After a trait's check failed, a type made later takes no bodies from it:
  // they would report the trait's errors again.
  if(program_trait_check_failed((ast_t*)ast_data(member)))
    return member;

  ast_t* template_method = program_template(member, member);
  return (template_method != NULL) ? template_method : member;
}

// Add provided methods to the given entity.
static bool trait_entity(ast_t* entity, pass_opt_t* opt)
{
  pony_assert(entity != NULL);

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
    case AST_FLAG_ERROR_1 | AST_FLAG_DONE_1:
      return false;

    default:
      pony_assert(0);
      return false;
  }

  setup_local_methods(entity);

  if((ast_id(entity) == TK_TRAIT) || (ast_id(entity) == TK_INTERFACE))
    bind_written_names(entity, opt);

  bool r =
    provided_methods(entity, opt) &&
    check_member_shadowing(entity, opt) &&
    check_concrete_bodies(entity, opt);

  r = finish_methods(entity, opt, r);

  if(r && ((ast_id(entity) == TK_TRAIT) || (ast_id(entity) == TK_INTERFACE)))
    make_templates(entity);

  if(is_trait_view_entity(entity, opt))
  {
    mark_trait_view_methods(entity, opt);

    if(program_is_deferred(entity))
      strip_inherited_bodies(entity);
  }

  ast_clearflag(entity, AST_FLAG_RECURSE_1);
  ast_setflag(entity, AST_FLAG_DONE_1);

  return r;
}


// Check that embed fields are not recursive.
static bool embed_fields(ast_t* entity, pass_opt_t* opt)
{
  pony_assert(entity != NULL);

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
      pony_assert(0);
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
      pony_assert(def != NULL);

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
  pony_assert(ast != NULL);

  // Setup type or mark as inferred now to allow calling create on a
  // non-inferred local to initialise itself
  AST_GET_CHILDREN(ast, id, type);
  pony_assert(type != NULL);

  if(ast_id(type) == TK_NONE)
  {
    if(is_name_dontcare(ast_name(id)))
      type = ast_from(id, TK_DONTCARETYPE);
    else
      type = ast_from(id, TK_INFERTYPE);
  }

  ast_settype(id, type);
  ast_settype(ast, type);
}


// Add eq() and ne() functions to the given entity.
static bool add_comparable(ast_t* ast, pass_opt_t* opt)
{
  pony_assert(ast != NULL);

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

  if(!has_member(members, "eq", opt))
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
        NONE
        NODE(TK_SEQ,
          NODE(TK_IS,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE));

    // Need to set function data field to point to originating type, ie ast.
    // This won't be done when we catch up the passes since we've already
    // processed that type.
    ast_setdata(eq, ast);
    ast_setflag(eq, AST_FLAG_SYNTHESIZED);
    ast_append(members, eq);
    ast_set(ast, stringtab(opt->strtab, "eq"), eq, SYM_DEFINED, false,
      opt->strtab);

    if(!ast_passes_subtree(&eq, opt, PASS_TRAITS))
      r = false;
  }

  if(!has_member(members, "ne", opt))
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
        NONE
        NODE(TK_SEQ,
          NODE(TK_ISNT,
            NODE(TK_THIS)
            NODE(TK_REFERENCE, ID("that"))))
        NONE));

    // Need to set function data field to point to originating type, ie ast.
    // This won't be done when we catch up the passes since we've already
    // processed that type.
    ast_setdata(ne, ast);
    ast_setflag(ne, AST_FLAG_SYNTHESIZED);
    ast_append(members, ne);
    ast_set(ast, stringtab(opt->strtab, "ne"), ne, SYM_DEFINED, false,
      opt->strtab);

    if(!ast_passes_subtree(&ne, opt, PASS_TRAITS))
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
