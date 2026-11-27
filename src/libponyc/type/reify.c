#include "reify.h"
#include "../pkg/program.h"
#include "../pass/traits.h"
#include "subtype.h"
#include "typealias.h"
#include "typeparam.h"
#include "viewpoint.h"
#include "assemble.h"
#include "alias.h"
#include "../ast/token.h"
#include "../ast/astbuild.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"

static ast_t* find_typearg(ast_t* ast, ast_t* typeparams, ast_t* typeargs)
{
  ast_t* ref_def = NULL;
  switch(ast_id(ast))
  {
    case TK_REFERENCE:
      ref_def = (ast_t*)ast_data(ast);
      if(ref_def == NULL)
        ref_def = ast_get(ast, ast_name(ast_child(ast)), NULL);
      if(ref_def == NULL)
        return NULL;

      // Resolve to the root of the ast_data chain for TK_TYPEPARAM nodes,
      // same as the TK_TYPEPARAMREF case below.
      if(ast_id(ref_def) == TK_TYPEPARAM)
        ref_def = typeparam_root(ref_def);

      break;

    case TK_TYPEPARAMREF:
      ref_def = (ast_t*)ast_data(ast);
      pony_assert(ref_def != NULL);
      ref_def = typeparam_root(ref_def);
      break;

    default:
      pony_assert(0);
  }

  // Iterate pairwise through the typeparams and typeargs,
  // until we find the one corresponding to this ref
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while((typeparam != NULL) && (typearg != NULL))
  {
    ast_t* param_def = (ast_t*)ast_data(typeparam);
    pony_assert(param_def != NULL);
    param_def = typeparam_root(param_def);

    if(ref_def == param_def)
      return typearg;

    // Not the type variable we are looking for, move on to next
    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  return NULL;
}

static void reify_typeparamref(pass_opt_t* opt, ast_t** astp, ast_t* typeparams, ast_t* typeargs)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_TYPEPARAMREF);

  ast_t* typearg = find_typearg(ast, typeparams, typeargs);
  if (typearg == NULL)
    return;

  // Keep ephemerality.
  switch(ast_id(ast_childidx(ast, 2)))
  {
    case TK_EPHEMERAL:
    {
      ast_t* new_typearg = consume_type(typearg, TK_NONE, true, opt);
      // Will be NULL when instantiation produces double ephemerals A^^
      // or equivalent.
      //
      // This might result from perfectly sound code,
      // but probably implies a signature is too weak
      // (e.g. returning T instead of T^).
      //
      // It could be avoided if we require a constraint on the parameter
      // for this case.
      if (new_typearg != NULL)
      {
        typearg = new_typearg;
      }
      break;
    }

    case TK_NONE:
      break;

    case TK_ALIASED:
      typearg = alias(typearg, opt);
      break;

    default:
      pony_assert(0);
  }

  ast_replace(astp, typearg);
}

static void reify_arrow(ast_t** astp, pass_opt_t* opt)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_ARROW);
  AST_GET_CHILDREN(ast, left, right);

  ast_t* r_left = left;
  ast_t* r_right = right;

  if(ast_id(left) == TK_ARROW)
  {
    AST_GET_CHILDREN(left, l_left, l_right);
    r_left = l_left;
    r_right = viewpoint_type(l_right, right, opt);
  }

  ast_t* r_type = viewpoint_type(r_left, r_right, opt);
  ast_replace(astp, r_type);
}

static void reify_reference(ast_t** astp, ast_t* typeparams, ast_t* typeargs)
{
  ast_t* ast = *astp;
  pony_assert(ast_id(ast) == TK_REFERENCE);

  ast_t* typearg = find_typearg(ast, typeparams, typeargs);
  if (typearg == NULL)
    return;

  ast_setid(ast, TK_TYPEREF);
  ast_add(ast, ast_from(ast, TK_NONE));    // 1st child: module reference
  ast_append(ast, ast_from(ast, TK_NONE)); // 3rd child: type args
  ast_settype(ast, typearg);
}


const char* find_dangling_default_ref(ast_t* ast, ast_t* typeparams,
  size_t pos)
{
  if(ast == NULL)
    return NULL;

  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    ast_t* ref_def = typeparam_root((ast_t*)ast_data(ast));
    ast_t* tp = ast_child(typeparams);
    size_t j = 0;

    while(tp != NULL)
    {
      if(j >= pos)
      {
        ast_t* tp_root = typeparam_root(tp);
        if(ref_def == tp_root)
          return ast_name(ast_child(tp));
      }
      tp = ast_sibling(tp);
      j++;
    }
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    const char* name = find_dangling_default_ref(child, typeparams, pos);
    if(name != NULL)
      return name;
  }

  return NULL;
}

// Reify a type parameter's default against the type arguments decided so far.
// Sibling references in the default must already be TK_TYPEPARAMREF nodes
// (via resolve_default_typeargs) so that reify() can substitute them.
ast_t* reify_default(ast_t* typeparam, ast_t* typeparams,
  ast_t* typeargs_so_far, pass_opt_t* opt)
{
  ast_t* defarg = ast_childidx(typeparam, 2);

  if(ast_id(defarg) == TK_NONE)
    return NULL;

  return reify(defarg, typeparams, typeargs_so_far, opt, true);
}

bool reify_defaults(ast_t* typeparams, ast_t* typeargs, bool errors,
  pass_opt_t* opt)
{
  pony_assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  pony_assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  size_t param_count = ast_childcount(typeparams);
  size_t arg_count = ast_childcount(typeargs);

  if(param_count == arg_count)
    return true;

  if(param_count < arg_count)
  {
    if(errors)
    {
      ast_error(opt->check.errors, typeargs, "too many type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  // Pick up default type arguments if they exist.
  ast_setid(typeargs, TK_TYPEARGS);
  ast_t* typeparam = ast_childidx(typeparams, arg_count);

  while(typeparam != NULL)
  {
    ast_t* defarg = ast_childidx(typeparam, 2);

    const char* dangling = find_dangling_default_ref(defarg, typeparams,
      arg_count);

    if(dangling != NULL)
    {
      if(errors)
      {
        ast_error(opt->check.errors, typeargs, "not enough type arguments");
        ast_error_continue(opt->check.errors, defarg,
          "default refers to type parameter %s", dangling);
      }

      return false;
    }

    ast_t* reified = reify_default(typeparam, typeparams, typeargs, opt);

    if(reified == NULL)
      break;

    ast_append(typeargs, reified);
    typeparam = ast_sibling(typeparam);
    arg_count++;
  }

  if(typeparam != NULL)
  {
    if(errors)
    {
      ast_error(opt->check.errors, typeargs, "not enough type arguments");
      ast_error_continue(opt->check.errors, typeparams, "definition is here");
    }

    return false;
  }

  return true;
}

// Whether two types are written the same: same tokens, names and
// definitions.
static bool same_type(ast_t* a, ast_t* b)
{
  if(ast_id(a) != ast_id(b))
    return false;

  switch(ast_id(a))
  {
    case TK_ID:
      return ast_name(a) == ast_name(b);

    case TK_NOMINAL:
    {
      // The same definition with the same arguments and capability, however
      // its package was named where it was written. An ephemeral val, box or
      // tag is the same type as the plain one.
      if((ast_data(a) != ast_data(b)) ||
        !same_type(ast_childidx(a, 2), ast_childidx(b, 2)) ||
        !same_type(ast_childidx(a, 3), ast_childidx(b, 3)))
        return false;

      switch(ast_id(ast_childidx(a, 3)))
      {
        case TK_VAL:
        case TK_BOX:
        case TK_TAG:
          return true;

        default:
          return same_type(ast_childidx(a, 4), ast_childidx(b, 4));
      }
    }

    case TK_TYPEPARAMREF:
      if(ast_data(a) != ast_data(b))
        return false;
      break;

    default: {}
  }

  ast_t* ca = ast_child(a);
  ast_t* cb = ast_child(b);

  while((ca != NULL) && (cb != NULL))
  {
    if(!same_type(ca, cb))
      return false;

    ca = ast_sibling(ca);
    cb = ast_sibling(cb);
  }

  return (ca == NULL) && (cb == NULL);
}

// Whether two members of a union or intersection are the same type, looking
// through an alias reference on either side. A type written with an alias
// reaches reify either with the reference still in it or with it unfolded,
// depending on the path, so a comparison that stopped at the reference would
// keep a member on one path and drop it on the other.
static bool same_member(ast_t* a, ast_t* b)
{
  ast_t* ua = (ast_id(a) == TK_TYPEALIASREF) ? typealias_unfold(a) : NULL;
  ast_t* ub = (ast_id(b) == TK_TYPEALIASREF) ? typealias_unfold(b) : NULL;

  bool same = same_type((ua != NULL) ? ua : a, (ub != NULL) ? ub : b);

  if(ua != NULL)
    ast_free_unattached(ua);

  if(ub != NULL)
    ast_free_unattached(ub);

  return same;
}

// Whether a member of a union or intersection is a type argument put in place
// of a type parameter (or a viewpoint of one) by this reification.
static bool is_substituted(ast_t* member, ast_t** before, size_t index,
  size_t count)
{
  return (index < count) && (before[index] != NULL) &&
    (before[index] != member);
}

// Append `m` to `to` unless a member already there is the same type and one
// of the two is a substituted type argument.
static void append_member(ast_t* to, ast_t* m, bool m_subst,
  bool* subst, size_t* count)
{
  size_t i = 0;

  for(ast_t* e = ast_child(to); e != NULL; e = ast_sibling(e), i++)
  {
    if((m_subst || subst[i]) && same_member(m, e))
      return;
  }

  ast_append(to, m);
  subst[(*count)++] = m_subst;
}

// A type argument that is a union, put in place of a member of a union, makes
// a nested union equal to the flat one, and the same type argument for two
// members, or for a member written beside it, repeats a member. Keep what a
// substitution makes flat and without repeats, so instantiations that put
// (A | None) in place of A again and again reach the same type instead of
// growing, and (A | B) with U8 for both is U8.
//
// Only what the substitution made changes: a nested union or repeated member
// written in the source stays as written. Other paths reach the same type
// without reifying it, and they must name the same type.
static void flatten_substituted(ast_t** astp, ast_t** before, size_t count)
{
  ast_t* ast = *astp;
  bool changes = false;
  size_t index = 0;

  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m), index++)
  {
    if(is_substituted(m, before, index, count) && (ast_id(m) == ast_id(ast)))
    {
      changes = true;
      break;
    }
  }

  if(!changes)
  {
    index = 0;

    for(ast_t* m = ast_child(ast); (m != NULL) && !changes;
      m = ast_sibling(m), index++)
    {
      size_t j = 0;

      for(ast_t* e = ast_child(ast); e != m; e = ast_sibling(e), j++)
      {
        if((is_substituted(m, before, index, count) ||
          is_substituted(e, before, j, count)) && same_member(m, e))
        {
          changes = true;
          break;
        }
      }
    }
  }

  if(!changes)
    return;

  size_t cap = 0;
  index = 0;

  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m), index++)
  {
    if(is_substituted(m, before, index, count) && (ast_id(m) == ast_id(ast)))
      cap += ast_childcount(m);
    else
      cap++;
  }

  bool* subst = (bool*)ponyint_pool_alloc_size(cap * sizeof(bool));
  size_t n = 0;
  ast_t* flat = ast_from(ast, ast_id(ast));
  index = 0;

  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m), index++)
  {
    bool m_subst = is_substituted(m, before, index, count);

    if(m_subst && (ast_id(m) == ast_id(ast)))
    {
      for(ast_t* c = ast_child(m); c != NULL; c = ast_sibling(c))
        append_member(flat, c, true, subst, &n);
    } else {
      append_member(flat, m, m_subst, subst, &n);
    }
  }

  ponyint_pool_free_size(cap * sizeof(bool), subst);

  if(ast_childcount(flat) == 1)
  {
    ast_t* only = ast_dup(ast_child(flat));
    ast_free(flat);
    flat = only;
  }

  ast_replace(astp, flat);
}

// Whether this reification replaces a type parameter reference in `ast`.
static bool has_substituted_typeparam(ast_t* ast, ast_t* typeparams,
  ast_t* typeargs)
{
  if((ast_id(ast) == TK_TYPEPARAMREF) &&
    (find_typearg(ast, typeparams, typeargs) != NULL))
    return true;

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
  {
    if(has_substituted_typeparam(c, typeparams, typeargs))
      return true;
  }

  return false;
}

static void reify_ast(ast_t** astp, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt)
{
  ast_t* ast = *astp;
  ast_t* child = ast_child(ast);

  // For a union or intersection, the members that are type parameters or
  // viewpoints of one, so that flatten_substituted can tell which members
  // this reification replaced.
  bool is_typeexpr = (ast_id(ast) == TK_UNIONTYPE) ||
    (ast_id(ast) == TK_ISECTTYPE);
  size_t count = 0;
  ast_t** before = NULL;

  if(is_typeexpr)
  {
    for(ast_t* m = child; m != NULL; m = ast_sibling(m))
    {
      if((ast_id(m) == TK_TYPEPARAMREF) || (ast_id(m) == TK_ARROW))
      {
        count = ast_childcount(ast);
        break;
      }
    }
  }

  if(count > 0)
  {
    before = (ast_t**)ponyint_pool_alloc_size(count * sizeof(ast_t*));
    size_t i = 0;

    for(ast_t* m = child; m != NULL; m = ast_sibling(m), i++)
    {
      before[i] = ((ast_id(m) == TK_TYPEPARAMREF) ||
        ((ast_id(m) == TK_ARROW) &&
          has_substituted_typeparam(m, typeparams, typeargs))) ? m : NULL;
    }
  }

  while(child != NULL)
  {
    reify_ast(&child, typeparams, typeargs, opt);
    child = ast_sibling(child);
  }

  ast_t* type = ast_type(ast);

  if(type != NULL)
    reify_ast(&type, typeparams, typeargs, opt);

  switch(ast_id(ast))
  {
    case TK_TYPEPARAMREF:
      reify_typeparamref(opt, astp, typeparams, typeargs);
      break;

    case TK_ARROW:
      reify_arrow(astp, opt);
      break;

    case TK_REFERENCE:
      reify_reference(astp, typeparams, typeargs);
      break;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      // With no member a type parameter or a viewpoint, none was replaced.
      if(before != NULL)
        flatten_substituted(astp, before, count);
      break;

    default: {}
  }

  if(before != NULL)
    ponyint_pool_free_size(count * sizeof(ast_t*), before);
}

ast_t* reify(ast_t* ast, ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt,
  bool duplicate)
{
  (void)opt;
  pony_assert(
    (ast_id(typeparams) == TK_TYPEPARAMS) ||
    (ast_id(typeparams) == TK_NONE)
    );
  pony_assert(
    (ast_id(typeargs) == TK_TYPEARGS) ||
    (ast_id(typeargs) == TK_NONE)
    );

  ast_t* r_ast;
  if(duplicate)
    r_ast = ast_dup(ast);
  else
    r_ast = ast;

  reify_ast(&r_ast, typeparams, typeargs, opt);
  return r_ast;
}

// Whether a type refers to one of `method_typeparams`.
static bool mentions_own_typeparams(ast_t* ast, ast_t* method_typeparams)
{
  if((ast == NULL) || (ast_id(method_typeparams) != TK_TYPEPARAMS))
    return false;

  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(ast);
    ast_t* root = (def != NULL) ? typeparam_root(def) : NULL;

    for(ast_t* p = ast_child(method_typeparams); p != NULL; p = ast_sibling(p))
    {
      if((root != NULL) && (typeparam_root(p) == root))
        return true;
    }
  }

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
  {
    if(mentions_own_typeparams(c, method_typeparams))
      return true;
  }

  return false;
}

// Point the references to `root` in `ast` at `alpha`.
static void rename_typeparam_refs(ast_t* ast, ast_t* root, ast_t* alpha)
{
  if(ast_id(ast) == TK_TYPEPARAMREF)
  {
    ast_t* def = (ast_t*)ast_data(ast);
    if((def != NULL) && (typeparam_root(def) == root))
      ast_setdata(ast, alpha);
  }

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
    rename_typeparam_refs(c, root, alpha);
}

// A method reified for a receiver whose type arguments name the method's own
// type parameters, as when a method's body names its own type with one of
// them, would have those arguments captured by its own declarations. The
// copy `r_ast` of `method` declares the renamed copy of each instead (always
// the same one, so recursive subtype checks meet the same types again).
// Returns whether it renamed.
static bool rename_own_typeparams(ast_t* method, ast_t* r_ast,
  ast_t* typeargs)
{
  ast_t* own = ast_childidx(method, 2);

  if((typeargs == NULL) || !mentions_own_typeparams(typeargs, own))
    return false;

  ast_t* tp = ast_child(ast_childidx(r_ast, 2));

  for(ast_t* orig = ast_child(own); orig != NULL;
    orig = ast_sibling(orig), tp = ast_sibling(tp))
  {
    ast_t* root = typeparam_root(orig);
    ast_t* alpha = program_alpha_typeparam(root);
    rename_typeparam_refs(r_ast, root, alpha);
    ast_setdata(tp, alpha);
  }

  return true;
}

ast_t* reify_method_def(ast_t* ast, ast_t* typeparams, ast_t* typeargs,
  pass_opt_t* opt)
{
  switch(ast_id(ast))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      break;

    default:
      pony_assert(false);
  }

  // Duplicate everything except the body and docstring (children 7-8).
  bool dup_child[9] = {true, true, true, true, true, true, true, false, false};
  ast_t* r_ast = ast_dup_partial(ast, dup_child, true, true, true);

  // Only a substitution of the receiver's type parameters can capture.
  if(typeparams != ast_childidx(ast, 2))
    rename_own_typeparams(ast, r_ast, typeargs);

  return reify(r_ast, typeparams, typeargs, opt, false);
}

ast_t* typeargs_naming_typeparams(ast_t* typeparams)
{
  BUILD(typeargs, typeparams, NODE(TK_TYPEARGS));

  for(ast_t* tp = ast_child(typeparams); tp != NULL; tp = ast_sibling(tp))
  {
    ast_t* id = ast_child(tp);
    BUILD(typearg, tp, NODE(TK_TYPEPARAMREF, TREE(id) NONE NONE));

    // A reified method's type parameters are copies, whose symbol table
    // still binds each name to the original parameter, which its signature
    // refers to.
    ast_t* def = ast_get(tp, ast_name(id), NULL);

    // Unless the copy is of a method rename_own_typeparams renamed: its
    // parameter's data is then the renamed copy, which is its own data and
    // in no tree, and its signature refers to that.
    ast_t* alpha = (ast_t*)ast_data(tp);
    if((alpha != NULL) && (alpha != tp) && (ast_data(alpha) == alpha) &&
      (ast_parent(alpha) == NULL))
      def = alpha;

    ast_setdata(typearg, def);
    typeparam_set_cap(typearg);
    ast_append(typeargs, typearg);
  }

  return typeargs;
}

deferred_reification_t* deferred_reify_new(ast_t* ast, ast_t* typeparams,
  ast_t* typeargs, ast_t* thistype)
{
  pony_assert(((typeparams != NULL) && (typeargs != NULL)) ||
    ((typeparams == NULL) && (typeargs == NULL)));

  deferred_reification_t* deferred = POOL_ALLOC(deferred_reification_t);

  deferred->ast = ast;
  deferred->type_typeparams = ast_dup(typeparams);
  deferred->type_typeargs = ast_dup(typeargs);
  deferred->thistype = ast_dup(thistype);
  deferred->method_typeparams = NULL;
  deferred->method_typeargs = NULL;

  return deferred;
}

void deferred_reify_add_method_typeparams(deferred_reification_t* deferred,
  ast_t* typeparams, ast_t* typeargs, pass_opt_t* opt)
{
  pony_assert((deferred->method_typeparams == NULL) &&
    (deferred->method_typeargs == NULL));

  pony_assert(((typeparams != NULL) && (typeargs != NULL)) ||
    ((typeparams == NULL) && (typeargs == NULL)));

  if(typeparams == NULL)
    return;

  ast_t* r_typeparams = ast_dup(typeparams);
  deferred->method_typeargs = ast_dup(typeargs);

  // Must replace `this` before typeparam reification.
  if(deferred->thistype != NULL)
    r_typeparams = viewpoint_replacethis(r_typeparams, deferred->thistype, false,
      opt);

  if(deferred->type_typeparams != NULL)
    r_typeparams = reify(r_typeparams, deferred->type_typeparams,
      deferred->type_typeargs, opt, false);

  deferred->method_typeparams = r_typeparams;
}

ast_t* deferred_reify(deferred_reification_t* deferred, ast_t* ast,
  pass_opt_t* opt)
{
  ast_t* r_ast = ast_dup(ast);

  // Must replace `this` before typeparam reification.
  if(deferred->thistype != NULL)
    r_ast = viewpoint_replacethis(r_ast, deferred->thistype, false, opt);

  if(deferred->type_typeparams != NULL)
    r_ast = reify(r_ast, deferred->type_typeparams, deferred->type_typeargs,
      opt, false);

  if(deferred->method_typeparams != NULL)
    r_ast = reify(r_ast, deferred->method_typeparams, deferred->method_typeargs,
      opt, false);

  return r_ast;
}

ast_t* deferred_reify_method_def(deferred_reification_t* deferred, ast_t* ast,
  pass_opt_t* opt)
{
  switch(ast_id(ast))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
      break;

    default:
      pony_assert(false);
  }

  // Duplicate everything except the body and docstring (children 7-8).
  bool dup_child[9] = {true, true, true, true, true, true, true, false, false};
  ast_t* r_ast = ast_dup_partial(ast, dup_child, true, true, true);

  bool renamed = rename_own_typeparams(ast, r_ast,
    deferred->type_typeargs);

  // Must replace `this` before typeparam reification.
  if(deferred->thistype != NULL)
    r_ast = viewpoint_replacethis(r_ast, deferred->thistype, false, opt);

  if(deferred->type_typeparams != NULL)
    r_ast = reify(r_ast, deferred->type_typeparams, deferred->type_typeargs,
      opt, false);

  // The method's own type arguments replace its renamed type parameters.
  if(deferred->method_typeparams != NULL)
    r_ast = reify(r_ast,
      renamed ? ast_childidx(r_ast, 2) : deferred->method_typeparams,
      deferred->method_typeargs, opt, false);

  return r_ast;
}

deferred_reification_t* deferred_reify_dup(deferred_reification_t* deferred)
{
  if(deferred == NULL)
    return NULL;

  deferred_reification_t* copy = POOL_ALLOC(deferred_reification_t);

  copy->ast = deferred->ast;
  copy->type_typeparams = ast_dup(deferred->type_typeparams);
  copy->type_typeargs = ast_dup(deferred->type_typeargs);
  copy->method_typeparams = ast_dup(deferred->method_typeparams);
  copy->method_typeargs = ast_dup(deferred->method_typeargs);
  copy->thistype = ast_dup(deferred->thistype);

  return copy;
}

void deferred_reify_free(deferred_reification_t* deferred)
{
  if(deferred != NULL)
  {
    ast_free_unattached(deferred->type_typeparams);
    ast_free_unattached(deferred->type_typeargs);
    ast_free_unattached(deferred->method_typeparams);
    ast_free_unattached(deferred->method_typeargs);
    ast_free_unattached(deferred->thistype);
    POOL_FREE(deferred_reification_t, deferred);
  }
}

bool check_constraints(ast_t* orig, ast_t* typeparams, ast_t* typeargs,
  bool report_errors, pass_opt_t* opt)
{
  ast_t* typeparam = ast_child(typeparams);
  ast_t* typearg = ast_child(typeargs);

  while(typeparam != NULL)
  {
    if(is_bare(typearg, opt))
    {
      if(report_errors)
      {
        ast_error(opt->check.errors, typearg,
          "a bare type cannot be used as a type argument");
      }

      return false;
    }

    switch(ast_id(typearg))
    {
      case TK_NOMINAL:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if((def != NULL) && (ast_id(def) == TK_STRUCT))
        {
          if(report_errors)
          {
            ast_error(opt->check.errors, typearg,
              "a struct cannot be used as a type argument");
          }

          return false;
        }
        break;
      }

      case TK_TYPEPARAMREF:
      {
        ast_t* def = (ast_t*)ast_data(typearg);

        if(def == typeparam)
        {
          typeparam = ast_sibling(typeparam);
          typearg = ast_sibling(typearg);
          continue;
        }
        break;
      }

      case TK_TYPEALIASREF:
      {
        // Unfold to check if the underlying type is a struct.
        ast_t* unfolded = typealias_unfold(typearg);

        if(unfolded != NULL)
        {
          if(ast_id(unfolded) == TK_NOMINAL)
          {
            ast_t* def = (ast_t*)ast_data(unfolded);

            if((def != NULL) && (ast_id(def) == TK_STRUCT))
            {
              ast_free_unattached(unfolded);

              if(report_errors)
              {
                ast_error(opt->check.errors, typearg,
                  "a struct cannot be used as a type argument");
              }

              return false;
            }
          }

          ast_free_unattached(unfolded);
        }
        break;
      }

      default: {}
    }

    // Reify the constraint.
    ast_t* constraint = ast_childidx(typeparam, 1);
    ast_t* r_constraint = reify(constraint, typeparams, typeargs, opt,
      true);

    // A bound type must be a subtype of the constraint.
    // The constraint check handles generic cap sets correctly
    // (val is a member of #read = {ref, val, box}), and the subtype check
    // handles concrete caps correctly (val <: box).
    // A type argument is valid if either check passes.
    if(!is_subtype_constraint(typearg, r_constraint, NULL, opt) &&
      !is_subtype(typearg, r_constraint, NULL, opt) &&
      !is_subtype_via_narrowing(opt, orig, typearg, r_constraint))
    {
      if(report_errors)
      {
        errorframe_t info = NULL;
        is_subtype_constraint(typearg, r_constraint, &info, opt);

        errorframe_t frame = NULL;
        ast_error_frame(&frame, orig,
          "type argument is outside its constraint");
        ast_error_frame(&frame, typearg,
          "argument: %s", ast_print_type(typearg, opt->strtab));
        ast_error_frame(&frame, typeparam,
          "constraint: %s", ast_print_type(r_constraint, opt->strtab));
        errorframe_append(&frame, &info);
        errorframe_report(&frame, opt->check.errors);
      }

      ast_free_unattached(r_constraint);
      return false;
    }

    ast_free_unattached(r_constraint);

    // A constructable constraint can only be fulfilled by a concrete typearg.
    if(is_constructable(constraint) && !is_concrete(typearg))
    {
      if(report_errors)
      {
        ast_error(opt->check.errors, orig, "a constructable constraint can "
          "only be fulfilled by a concrete type argument");
        ast_error_continue(opt->check.errors, typearg, "argument: %s",
          ast_print_type(typearg, opt->strtab));
        ast_error_continue(opt->check.errors, typeparam, "constraint: %s",
          ast_print_type(constraint, opt->strtab));
      }

      return false;
    }

    typeparam = ast_sibling(typeparam);
    typearg = ast_sibling(typearg);
  }

  pony_assert(typeparam == NULL);
  pony_assert(typearg == NULL);
  return true;
}
