#include "lookup.h"
#include "typeparam.h"
#include "assemble.h"
#include "cap.h"
#include "reify.h"
#include "typealias.h"
#include "viewpoint.h"
#include "subtype.h"
#include "../ast/token.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../../libponyrt/mem/pool.h"
#include "../pass/pass.h"
#include "../pass/expr.h"
#include "../pass/traits.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../ast/frame.h"
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

static bool is_method_typeparam(const char* name, ast_t* method_typeparams)
{
  ast_t* tp = ast_child(method_typeparams);

  while(tp != NULL)
  {
    if(ast_name(ast_child(tp)) == name)
      return true;
    tp = ast_sibling(tp);
  }

  return false;
}

static bool guard_references_method_typeparams(ast_t* guard,
  ast_t* method_typeparams)
{
  switch(ast_id(guard))
  {
    case TK_IFTYPEGUARD:
    {
      ast_t* subtype = ast_child(guard);

      if(ast_id(subtype) == TK_TYPEPARAMREF)
      {
        ast_t* def = (ast_t*)ast_data(subtype);
        const char* name = ast_name(ast_child(def));
        return is_method_typeparam(name, method_typeparams);
      }

      if(ast_id(subtype) == TK_NOMINAL)
      {
        const char* name = ast_name(ast_childidx(subtype, 1));
        return is_method_typeparam(name, method_typeparams);
      }

      if(ast_id(subtype) == TK_TUPLETYPE)
      {
        ast_t* elem = ast_child(subtype);
        while(elem != NULL)
        {
          if(ast_id(elem) == TK_TYPEPARAMREF)
          {
            ast_t* def = (ast_t*)ast_data(elem);
            const char* name = ast_name(ast_child(def));
            if(is_method_typeparam(name, method_typeparams))
              return true;
          }
          else if(ast_id(elem) == TK_NOMINAL)
          {
            const char* name = ast_name(ast_childidx(elem, 1));
            if(is_method_typeparam(name, method_typeparams))
              return true;
          }
          elem = ast_sibling(elem);
        }
      }

      return false;
    }

    case TK_IFTYPEGUARD_AND:
    case TK_IFTYPEGUARD_OR:
    {
      ast_t* child = ast_child(guard);
      while(child != NULL)
      {
        if(guard_references_method_typeparams(child, method_typeparams))
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
    ast_t* spec_typeparams = ast_childidx(spec, 2);

    // When the guard references a method-level type parameter, it cannot
    // be evaluated here — the concrete method type arguments are only known
    // at the call site (reachability pass). Skip this specialization and
    // let reach.c handle it. Check the specialization's own type parameters,
    // not the default's, because they may use different names.
    if((ast_id(spec_typeparams) != TK_NONE) &&
      guard_references_method_typeparams(guard, spec_typeparams))
    {
      spec = ast_sibling(spec);
      continue;
    }

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

// The package (TK_MODULE) holding the file whose source is `source`.
static ast_t* module_of_source(ast_t* ast, source_t* source)
{
  if(source == NULL)
    return NULL;

  for(; ast != NULL; ast = ast_sibling(ast))
  {
    switch(ast_id(ast))
    {
      case TK_FILE:
        if((source_t*)ast_data(ast) == source)
          return ast_nearest(ast, TK_MODULE);
        break;

      case TK_PROGRAM:
      case TK_MODULE:
      {
        ast_t* found = module_of_source(ast_child(ast), source);
        if(found != NULL)
          return found;
        break;
      }

      default: {}
    }
  }

  return NULL;
}

// The type aliases being unfolded, outermost last.
typedef struct unfolding_t
{
  ast_t* alias;
  struct unfolding_t* outer;
} unfolding_t;

static bool names_private_of_other_module(ast_t* type, ast_t* written,
  ast_t* program, unfolding_t* unfolding)
{
  if(type == NULL)
    return false;

  if(ast_id(type) == TK_NOMINAL)
  {
    ast_t* def = (ast_t*)ast_data(type);

    if((def != NULL) && is_name_private(ast_name(ast_child(def))))
    {
      ast_t* where = (written != NULL) ? written :
        module_of_source(program, ast_source(type));

      if(where != ast_nearest(def, TK_MODULE))
        return true;
    }
  }

  if(ast_id(type) == TK_TYPEALIASREF)
  {
    // A recursive alias names itself in what it unfolds to; that reference
    // names nothing the outer unfolding doesn't.
    ast_t* alias = (ast_t*)ast_data(type);
    bool unfolding_it = false;

    for(unfolding_t* u = unfolding; u != NULL; u = u->outer)
    {
      if(u->alias == alias)
      {
        unfolding_it = true;
        break;
      }
    }

    if(!unfolding_it)
    {
      // What an alias names counts as written where the alias is named.
      ast_t* where = (written != NULL) ? written :
        module_of_source(program, ast_source(type));
      unfolding_t inner = {alias, unfolding};
      ast_t* unfolded = typealias_unfold(type);
      bool r = (unfolded != NULL) &&
        names_private_of_other_module(unfolded, where, program, &inner);

      if(unfolded != NULL)
        ast_free_unattached(unfolded);

      if(r)
        return true;
    }
  }

  for(ast_t* child = ast_child(type); child != NULL; child = ast_sibling(child))
  {
    if(names_private_of_other_module(child, written, program, unfolding))
      return true;
  }

  return false;
}

bool type_names_foreign_private(pass_opt_t* opt, ast_t* type)
{
  ast_t* program = ast_nearest(opt->check.frame->module, TK_PROGRAM);
  return names_private_of_other_module(type, NULL, program, NULL);
}

static const char* package_display_name(ast_t* module)
{
  ast_t* program = ast_nearest(module, TK_PROGRAM);

  if(program == NULL)
    return module_qualified_name(module);

  // The package the program is built from is "main"; one below it is named
  // by its path from there.
  ast_t* root = ast_child(program);
  if(root == module)
    return "main";

  const char* name = module_qualified_name(module);
  const char* root_name = module_qualified_name(root);
  size_t len = strlen(root_name);

  if((strncmp(name, root_name, len) == 0) && (name[len] == '/'))
    return name + len + 1;

  return name;
}

// Whether a type is `def`, or a union or intersection with `def` as a member.
static bool names_def(ast_t* type, ast_t* def)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      return ast_data(type) == def;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      for(ast_t* m = ast_child(type); m != NULL; m = ast_sibling(m))
      {
        if(names_def(m, def))
          return true;
      }
      return false;

    default:
      return false;
  }
}

// In a type argument, a place that names the private type `def` from a
// package other than `def_module`: an alias that unfolds to it, written
// outside that package.
static ast_t* foreign_private_in(ast_t* type, ast_t* def, ast_t* def_module,
  ast_t* program)
{
  if(ast_id(type) == TK_TYPEALIASREF)
  {
    ast_t* unfolded = typealias_unfold(type);
    bool names = (unfolded != NULL) && names_def(unfolded, def);
    ast_free_unattached(unfolded);

    if(names && (module_of_source(program, ast_source(type)) != def_module))
      return type;
  }

  for(ast_t* c = ast_child(type); c != NULL; c = ast_sibling(c))
  {
    ast_t* found = foreign_private_in(c, def, def_module, program);
    if(found != NULL)
      return found;
  }

  return NULL;
}

// Whether `ast` holds a node written at the same place as `at`.
static bool holds_position(ast_t* ast, ast_t* at)
{
  if((ast_source(ast) == ast_source(at)) && (ast_line(ast) == ast_line(at)) &&
    (ast_pos(ast) == ast_pos(at)))
    return true;

  for(ast_t* c = ast_child(ast); c != NULL; c = ast_sibling(c))
  {
    if(holds_position(c, at))
      return true;
  }

  return false;
}

// The types a provides walk has visited, so each is walked once.
typedef struct visited_defs_t
{
  ast_t** defs;
  size_t count;
  size_t size;
} visited_defs_t;

// Whether `def` was visited before; records it if not.
static bool visited_before(visited_defs_t* v, ast_t* def)
{
  for(size_t i = 0; i < v->count; i++)
  {
    if(v->defs[i] == def)
      return true;
  }

  if(v->count == v->size)
  {
    size_t size = (v->size == 0) ? 8 : (v->size * 2);
    v->defs = (ast_t**)ponyint_pool_realloc_size(v->size * sizeof(ast_t*),
      size * sizeof(ast_t*), v->defs);
    v->size = size;
  }

  v->defs[v->count++] = def;
  return false;
}

static void visited_free(visited_defs_t* v)
{
  if(v->size > 0)
    ponyint_pool_free_size(v->size * sizeof(ast_t*), v->defs);
}

// Whether a trait or interface declares a method named `name`.
static bool declares_method(ast_t* def, const char* name)
{
  if((ast_id(def) != TK_TRAIT) && (ast_id(def) != TK_INTERFACE))
    return false;

  for(ast_t* m = ast_child(ast_childidx(def, 4)); m != NULL;
    m = ast_sibling(m))
  {
    ast_t* each = (ast_id(m) == TK_METHODGROUP) ? ast_child(m) : m;

    switch(ast_id(each))
    {
      case TK_FUN:
      case TK_BE:
      case TK_NEW:
        if(ast_name(ast_childidx(each, 1)) == name)
          return true;
        break;

      default: {}
    }
  }

  return false;
}

static bool entry_reaches(ast_t* entry, const char* name, visited_defs_t* v);

// Whether `def` is a trait or interface declaring the method `name`, or
// provides one directly or through other types. A type alias's provides slot
// holds the type it names.
static bool def_reaches(ast_t* def, const char* name, visited_defs_t* v)
{
  // A type visited before led to no such trait, or the walk would have
  // stopped.
  if((def == NULL) || visited_before(v, def))
    return false;

  if(declares_method(def, name))
    return true;

  for(ast_t* p = ast_child(ast_childidx(def, 3)); p != NULL;
    p = ast_sibling(p))
  {
    if(entry_reaches(p, name, v))
      return true;
  }

  return false;
}

// Whether a provides entry, or a member of an intersection there, leads to a
// trait or interface declaring the method `name`.
static bool entry_reaches(ast_t* entry, const char* name, visited_defs_t* v)
{
  switch(ast_id(entry))
  {
    case TK_ISECTTYPE:
      for(ast_t* m = ast_child(entry); m != NULL; m = ast_sibling(m))
      {
        if(entry_reaches(m, name, v))
          return true;
      }
      return false;

    case TK_NOMINAL:
    case TK_TYPEALIASREF:
      return def_reaches((ast_t*)ast_data(entry), name, v);

    default:
      return false;
  }
}

// Whether `entry` leads to a trait or interface declaring the method `name`.
static bool leads_to_method(ast_t* entry, const char* name)
{
  visited_defs_t v = {NULL, 0, 0};
  bool r = entry_reaches(entry, name, &v);
  visited_free(&v);
  return r;
}

static bool written_on_path_from(ast_t* def, const char* name,
  ast_t* occurrence, visited_defs_t* v);

// written_on_provides_path for one provides entry of a type on the path.
static bool written_in_entry(ast_t* entry, const char* name,
  ast_t* occurrence, visited_defs_t* v)
{
  switch(ast_id(entry))
  {
    case TK_ISECTTYPE:
      for(ast_t* m = ast_child(entry); m != NULL; m = ast_sibling(m))
      {
        if(written_in_entry(m, name, occurrence, v))
          return true;
      }
      return false;

    case TK_NOMINAL:
    case TK_TYPEALIASREF:
    {
      if(!leads_to_method(entry, name))
        return false;

      return holds_position(entry, occurrence) ||
        written_on_path_from((ast_t*)ast_data(entry), name, occurrence, v);
    }

    default:
      return false;
  }
}

static bool written_on_path_from(ast_t* def, const char* name,
  ast_t* occurrence, visited_defs_t* v)
{
  if((def == NULL) || visited_before(v, def))
    return false;

  for(ast_t* p = ast_child(ast_childidx(def, 3)); p != NULL;
    p = ast_sibling(p))
  {
    if(written_in_entry(p, name, occurrence, v))
      return true;
  }

  return false;
}

// Whether the type at `occurrence` was written in an entry of a provides list
// on a path from `def` to a trait or interface that declares the inherited
// method `name`: an entry that leads to one, including the body of a type
// alias named there, which the list holds unfolded. Entries that lead only
// elsewhere don't count.
static bool written_on_provides_path(ast_t* def, const char* name,
  ast_t* occurrence)
{
  if(name == NULL)
    return false;

  visited_defs_t v = {NULL, 0, 0};
  bool r = written_on_path_from(def, name, occurrence, &v);
  visited_free(&v);
  return r;
}

// In a provides entry's type arguments, the first type defined in `module`.
static ast_t* argument_from(ast_t* typeargs, ast_t* module)
{
  for(ast_t* a = ast_child(typeargs); a != NULL; a = ast_sibling(a))
  {
    if(((ast_id(a) == TK_NOMINAL) || (ast_id(a) == TK_TYPEALIASREF)) &&
      (ast_data(a) != NULL) &&
      (ast_nearest((ast_t*)ast_data(a), TK_MODULE) == module))
      return a;
  }

  return NULL;
}

// Among the provides entries of a type, one naming `trait` directly whose
// type arguments name the private type `def` from another package.
static ast_t* foreign_provides_occurrence(ast_t* provides, ast_t* trait,
  ast_t* def, ast_t* def_module, ast_t* program)
{
  for(ast_t* p = ast_child(provides); p != NULL; p = ast_sibling(p))
  {
    switch(ast_id(p))
    {
      case TK_ISECTTYPE:
      {
        ast_t* found = foreign_provides_occurrence(p, trait, def, def_module,
          program);
        if(found != NULL)
          return found;
        break;
      }

      case TK_NOMINAL:
        if(ast_data(p) == trait)
        {
          ast_t* found = foreign_private_in(ast_childidx(p, 2), def,
            def_module, program);
          if(found != NULL)
            return found;
        }
        break;

      default: {}
    }
  }

  return NULL;
}

// The method of `entity`, written by the entity itself, whose result type is
// the text at `at`; NULL if there is none.
static ast_t* result_type_method(ast_t* entity, ast_t* at)
{
  if(entity == NULL)
    return NULL;

  ast_t* members = ast_childidx(entity, 4);

  for(ast_t* m = ast_child(members); m != NULL; m = ast_sibling(m))
  {
    switch(ast_id(m))
    {
      case TK_FUN:
      case TK_BE:
      case TK_NEW:
        if(((ast_t*)ast_data(m) == entity) &&
          holds_position(ast_childidx(m, 4), at))
          return m;
        break;

      default: {}
    }
  }

  return NULL;
}

// Whether inherited code, checked for the type inheriting it, may look up
// fields or methods on the private type `def`. The rule: the code may when
// `def` is the inheriting type, or when the text that brought `def` to the
// code is in `def`'s package and
//  - the code is written in `def`'s package, or
//  - the inheriting type is in `def`'s package, or
//  - that text is a type argument in a provides entry on a path from the
//    inheriting type to a trait or interface that declares the method,
//    including the body of a type alias used as such an entry.
// Text in `def`'s package that names it anywhere else, such as in a method's
// result or in a provides entry that leads elsewhere, doesn't count. A type
// argument written elsewhere that names `def` through an alias doesn't count
// either. Even when the rule allows it, the lookup is refused when the method
// also reaches the type through another provides entry whose type argument
// names it from another package. When the lookup is refused, a note says
// which text brought the type to the code.
static bool private_visible_in_copy(pass_opt_t* opt, ast_t* def,
  ast_t* occurrence, ast_t* from, bool errors)
{
  typecheck_t* t = &opt->check;

  // The inheriting type, through the hosts of classes made from literals, and
  // the method there that the code is in or that held its literal.
  ast_t* inheritor = t->frame->type;
  ast_t* inherited_method = t->frame->method;

  for(ast_t* entity = t->frame->type; entity != NULL; )
  {
    if(entity == def)
      return true;

    inheritor = entity;

    ast_t* host_entity;
    ast_t* host_method;
    if(!ast_has_annotation(entity, ANON_COPY_ANNOTATION, opt->strtab) ||
      !program_anon_host(entity, &host_entity, &host_method))
      break;

    entity = host_entity;
    inherited_method = host_method;
  }

  // That method's name: the traits and interfaces declaring it are the ones
  // whose type arguments its code receives.
  const char* method_name = (inherited_method != NULL) ?
    ast_name(ast_childidx(inherited_method, 1)) : NULL;

  ast_t* def_module = ast_nearest(def, TK_MODULE);
  ast_t* program = ast_nearest(t->frame->module, TK_PROGRAM);
  ast_t* written = module_of_source(program, ast_source(occurrence));

  // A private type of a third package, neither the inheriting type's nor the
  // one the code is written in, that its own package's text named somewhere
  // other than a provides path to the method: blame the type argument it
  // arrived through.
  ast_t* donor = (t->frame->method != NULL) ?
    (ast_t*)ast_data(t->frame->method) : NULL;
  ast_t* code_module = ((donor != NULL) && ((ast_id(donor) == TK_TRAIT) ||
    (ast_id(donor) == TK_INTERFACE))) ? ast_nearest(donor, TK_MODULE) :
    t->frame->module;
  bool third_package = (def_module != code_module) &&
    (def_module != ast_nearest(inheritor, TK_MODULE)) &&
    !written_on_provides_path(inheritor, method_name, occurrence);

  if((written == def_module) && third_package)
  {
    ast_t* entry = inherited_provides_entry(t->frame->type, t->frame->method,
      opt);
    ast_t* arg = (entry != NULL) ?
      argument_from(ast_childidx(entry, 2), def_module) : NULL;

    if(arg != NULL)
    {
      occurrence = arg;
      written = module_of_source(program, ast_source(arg));
    }
  }

  if((written == def_module) && !third_package)
  {
    // When the type came through a type argument (an alias, or a type an
    // alias unfolded to), the method may also arrive through another provides
    // entry whose type argument names it from another package.
    bool via_argument = (ast_id(occurrence) == TK_TYPEALIASREF) ||
      (ast_id(occurrence) == TK_NONE);
    ast_t* other = (via_argument && (donor != NULL)) ?
      foreign_provides_occurrence(ast_childidx(t->frame->type, 3), donor, def,
        def_module, program) : NULL;

    if(other == NULL)
      return true;

    occurrence = other;
    written = module_of_source(program, ast_source(other));
  }

  if(errors)
  {
    ast_error(opt->check.errors, from, "can't lookup fields or methods "
      "on private types from other modules");

    ast_t* method = (written != NULL) ?
      result_type_method(t->frame->type, occurrence) : NULL;

    if(method != NULL)
      ast_error_continue(opt->check.errors, occurrence,
        "'%s' is private to package '%s" BLAME_NOTE_REACHES "the "
        "result type of %s's method '%s', written in package '%s'",
        ast_name(ast_child(def)), package_display_name(def_module),
        ast_name(ast_child(t->frame->type)),
        ast_name(ast_childidx(method, 1)), package_display_name(written));
    else if(written != NULL)
      ast_error_continue(opt->check.errors, occurrence,
        "'%s' is private to package '%s" BLAME_NOTE_REACHES "this "
        "type argument, written in package '%s'",
        ast_name(ast_child(def)), package_display_name(def_module),
        package_display_name(written));
  }

  return false;
}

// Whether the call that `from` belongs to supplies the parameter at `index`
// named `name`, so its default is not needed there.
static bool call_supplies_param(ast_t* from, size_t index, const char* name)
{
  if(from == NULL)
    return false;

  ast_t* call = ast_parent(from);

  // A call with type arguments, recv.f[A](...), puts them between the call
  // and the method's name.
  if((call != NULL) && (ast_id(call) == TK_QUALIFY))
    call = ast_parent(call);

  if((call == NULL) || (ast_id(call) != TK_CALL))
    return false;

  AST_GET_CHILDREN(call, postfix, positional, named);
  ast_t* arg = ast_childidx(positional, index);

  if((arg != NULL) && (ast_id(arg) != TK_NONE))
    return true;

  for(ast_t* n = ast_child(named); n != NULL; n = ast_sibling(n))
  {
    if(ast_name(ast_child(n)) == name)
      return true;
  }

  return false;
}

// Append to `buf` the name of the method or lambda that declares `param`,
// with the name of its type for a method: "T.m".
static void print_param_owner(printbuf_t* buf, ast_t* param)
{
  ast_t* owner = ast_parent(ast_parent(param));

  switch(ast_id(owner))
  {
    case TK_FUN:
    case TK_BE:
    case TK_NEW:
    {
      ast_t* entity = method_entity(owner);

      if((entity != NULL) && (ast_id(ast_child(entity)) == TK_ID))
        printbuf(buf, "%s.", ast_name(ast_child(entity)));

      printbuf(buf, "%s", ast_name(ast_childidx(owner, 1)));
      break;
    }

    default:
      printbuf(buf, "a lambda");
  }
}

// Check the default argument of `param`, a parameter of `method`, that a call
// at `from` needs before the method's own check has reached it. It is checked
// in the frames of the method that declares it. A default that needs itself
// through other defaults is an error.
static bool check_default_param(pass_opt_t* opt, ast_t* from, ast_t* method,
  ast_t* param)
{
  typecheck_t* t = &opt->check;
  ast_t* anchor = t->frame->module;

  if((anchor != NULL) && program_default_check_in_progress(anchor, param))
  {
    printbuf_t* owner = printbuf_new();
    print_param_owner(owner, param);
    ast_error(opt->check.errors, from,
      "default argument of parameter '%s' of '%s' uses itself through "
      "default arguments", ast_name(ast_child(param)), owner->m);
    printbuf_free(owner);

    // The parameters on the cycle, from the one re-entered to the latest.
    astlist_t* cycle = NULL;
    for(astlist_t* p = program_default_checks(anchor); p != NULL;
      p = astlist_next(p))
    {
      cycle = astlist_push(cycle, astlist_data(p));
      if(astlist_data(p) == param)
        break;
    }

    for(astlist_t* p = cycle; p != NULL; p = astlist_next(p))
    {
      ast_t* on_cycle = astlist_data(p);
      printbuf_t* its_owner = printbuf_new();
      print_param_owner(its_owner, on_cycle);
      ast_error_continue(opt->check.errors, ast_child(on_cycle),
        "default argument of parameter '%s' of '%s' is here",
        ast_name(ast_child(on_cycle)), its_owner->m);
      printbuf_free(its_owner);
    }

    astlist_free(cycle);
    return false;
  }

  ast_t* entity = method_entity(method);

  ast_t* file = ast_nearest(method, TK_FILE);
  ast_t* module = (file != NULL) ? ast_parent(file) : NULL;

  if((file == NULL) || (module == NULL) || (entity == NULL))
    return ast_visit_scope(&param, pass_pre_expr, pass_expr, opt, PASS_EXPR)
      == AST_OK;

  frame_push(t, NULL);
  frame_push(t, module);
  frame_push(t, file);
  frame_push(t, entity);
  frame_push(t, method);

  size_t before = errors_get_count(t->errors);
  ast_result_t r = ast_visit(&param, pass_pre_expr, pass_expr, opt,
    PASS_EXPR);
  program_add_unnoted_range(anchor, before, errors_get_count(t->errors));

  // A check that stopped early didn't end its mark.
  program_default_check_end(anchor, param);

  frame_pop(t);
  frame_pop(t);
  frame_pop(t);
  frame_pop(t);
  frame_pop(t);

  return r == AST_OK;
}

static bool provides_private_from_in(ast_t* def, const char* name,
  ast_t* package, visited_defs_t* v)
{
  ast_t* provides = ast_childidx(def, 3);

  for(ast_t* p = ast_child(provides); p != NULL; p = ast_sibling(p))
  {
    if(ast_id(p) != TK_NOMINAL)
      continue;

    ast_t* trait = (ast_t*)ast_data(p);

    // A trait visited before provided no such method, or the walk would have
    // stopped.
    if((trait == NULL) || visited_before(v, trait))
      continue;

    if((ast_nearest(trait, TK_MODULE) == package) &&
      (ast_get(trait, name, NULL) != NULL))
      return true;

    if(provides_private_from_in(trait, name, package, v))
      return true;
  }

  return false;
}

// Whether `def` provides, directly or through its providers, a trait or
// interface from `package` that declares the method `name`.
static bool provides_private_from(ast_t* def, const char* name,
  ast_t* package)
{
  visited_defs_t v = {NULL, 0, 0};
  bool r = provides_private_from_in(def, name, package, &v);
  visited_free(&v);
  return r;
}

static deferred_reification_t* lookup_nominal(pass_opt_t* opt, ast_t* from,
  ast_t* orig, ast_t* type, const char* name, bool errors, bool allow_private)
{
  pony_assert(ast_id(type) == TK_NOMINAL);

  ast_t* def = (ast_t*)ast_data(type);
  AST_GET_CHILDREN(def, type_id, typeparams);
  const char* type_name = ast_name(type_id);

  if(is_name_private(type_name) && (from != NULL) && (opt != NULL)
    && !allow_private && is_copy_context(opt))
  {
    ast_t* occurrence = (ast_id(orig) == TK_TYPEALIASREF) ? orig : type;

    // A private type an alias unfolded to was named where the alias was.
    if((occurrence == type) && (ast_childcount(type) >= 7))
      occurrence = ast_childidx(type, 6);

    if(!private_visible_in_copy(opt, def, occurrence, from, errors))
      return NULL;
  } else if(is_name_private(type_name) && (from != NULL) && (opt != NULL)
    && !allow_private)
  {
    typecheck_t* t = &opt->check;

    if(ast_nearest(def, TK_MODULE) != t->frame->module)
    {
      if(errors)
      {
        ast_error(opt->check.errors, from, "can't lookup fields or methods "
          "on private types from other modules");
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

            // A default is checked when a call needs it, and only then: a
            // default the call doesn't use can be one whose check is what
            // led here.
            if((ast_id(def_arg) != TK_NONE) && (ast_type(def_arg) == NULL) &&
              !call_supplies_param(from, ast_index(param),
                ast_name(ast_child(param))))
            {
              ast_t* child = ast_child(def_arg);

              if(ast_id(child) == TK_CALL)
                ast_settype(child, ast_from(child, TK_INFERTYPE));

              if(!check_default_param(opt, from, find, param))
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

      case TK_NEW:
      case TK_BE:
      case TK_FUN:
      {
        // Given that we can pick up method bodies from default methods on
        // traits, we need to allow the lookup of private methods from within
        // the same module.
        // We do this by getting the method we are calling from:
        //
        // t->frame->method
        //
        // And getting its body donor.
        // If the body_donor is a trait then check if that trait is in the same
        // module the method we are trying to lookup is defined in.
        //
        // If the method we are looking up is in the same module as the trait
        // that we got our method body from then we can allow the lookup.
        bool skip_check = false;
        ast_t* body_donor = (ast_t*)ast_data(t->frame->method);
        if ((body_donor != NULL) && (ast_id(body_donor) == TK_TRAIT) && (ast_nearest(find, TK_MODULE) == ast_nearest(body_donor, TK_MODULE)))
           skip_check = true;

        // In inherited code, a private method is found as the code written in
        // the providing trait's package finds it: on a type from that package,
        // or on one that provides a trait from that package declaring it.
        if(!skip_check && (body_donor != NULL) &&
          ((ast_id(body_donor) == TK_TRAIT) ||
            (ast_id(body_donor) == TK_INTERFACE)) &&
          is_copy_context(opt))
        {
          ast_t* written = ast_nearest(body_donor, TK_MODULE);
          skip_check = (ast_nearest(def, TK_MODULE) == written) ||
            provides_private_from(def, name, written);
        }

        if(!skip_check && (ast_nearest(def, TK_MODULE) != t->frame->module))
        {
          if(errors)
          {
            ast_error(opt->check.errors, from,
              "can't lookup private methods from outside the module");
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

// Clear the capabilities in a type, so it prints as written without them.
static void clear_caps(ast_t* type)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      ast_setid(ast_childidx(type, 3), TK_NONE);
      ast_setid(ast_childidx(type, 4), TK_NONE);
      break;

    case TK_TYPEPARAMREF:
      ast_setid(ast_childidx(type, 1), TK_NONE);
      ast_setid(ast_childidx(type, 2), TK_NONE);
      break;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      for(ast_t* c = ast_child(type); c != NULL; c = ast_sibling(c))
        clear_caps(c);
      break;

    default: {}
  }
}

typedef struct
{
  pass_opt_t* opt;
  ast_t* from;
  ast_t* type;
  const char* name;
  deferred_reification_t* found;
  bool failed;
} narrowed_lookup_t;

static bool lookup_in_narrowing(ast_t* subject, ast_t* super,
  ast_t* typeparams, void* ctx)
{
  narrowed_lookup_t* c = (narrowed_lookup_t*)ctx;
  pass_opt_t* opt = c->opt;
  ast_t* narrowed = type_isect(opt, c->type, super);
  deferred_reification_t* found = lookup_base(opt, c->from, narrowed,
    narrowed, c->name, false, false);
  ast_free_unattached(narrowed);

  if(found == NULL)
    return false;

  if(!type_mentions_typeparam(subject))
  {
    c->found = found;
    return true;
  }

  // The narrowed code can run for some instantiations.
  deferred_reify_free(found);
  ast_t* param = ast_child(typeparams);
  ast_t* written = ast_dup(c->type);
  clear_caps(written);
  ast_error(opt->check.errors, c->from, "couldn't find '%s' in '%s'; it is "
    "found only through the narrowed type parameter '%s', and inherited code "
    "can't call a member through a narrowing that may not hold for the "
    "inheriting type", c->name, ast_print_type(written, opt->strtab),
    (param != NULL) ? ast_name(ast_child(param)) : "?");
  ast_free(written);
  ast_error_continue(opt->check.errors, c->from, "bind the value to a local "
    "of a type that has '%s' first, for example: let v: %s = ...", c->name,
    ast_print_type(super, opt->strtab));
  c->failed = true;
  return true;
}

// In inherited code, a member that the receiver's type lacks can be found
// through the narrowing of an enclosing iftype, as the providing trait's own
// check found it. In a branch that can't run for the inheriting type (its
// condition is false for the substituted types), the member is taken from
// there. In a branch that may run for some instantiations, it is an error:
// the narrowing may not hold for the value's actual type.
static deferred_reification_t* lookup_through_narrowing(pass_opt_t* opt,
  ast_t* from, ast_t* type, const char* name)
{
  narrowed_lookup_t c = {opt, from, type, name, NULL, false};

  if(visit_copy_narrowings(opt, from, lookup_in_narrowing, &c))
    return c.failed ? NULL : c.found;

  return lookup_base(opt, from, type, type, name, true, false);
}

deferred_reification_t* lookup(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name)
{
  if((opt != NULL) && (from != NULL) && is_copy_context(opt))
  {
    deferred_reification_t* found = lookup_base(opt, from, type, type, name,
      false, false);

    if(found != NULL)
      return found;

    return lookup_through_narrowing(opt, from, type, name);
  }

  return lookup_base(opt, from, type, type, name, true, false);
}

deferred_reification_t* lookup_try(pass_opt_t* opt, ast_t* from, ast_t* type,
  const char* name, bool allow_private)
{
  return lookup_base(opt, from, type, type, name, false, allow_private);
}
