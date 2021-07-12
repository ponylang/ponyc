#include "reach.h"
#include "subtype.h"
#include "../ast/astbuild.h"
#include "../codegen/genname.h"
#include "../pass/expr.h"
#include "../type/assemble.h"
#include "../type/cap.h"
#include "../type/lookup.h"
#include "../type/subtype.h"
#include "../../libponyrt/gc/serialise.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdio.h>
#include <string.h>

DEFINE_STACK(reach_method_stack, reach_method_stack_t, reach_method_t);

static reach_method_t* add_rmethod(reach_t* r, reach_type_t* t,
  reach_method_name_t* n, token_id cap, ast_t* typeargs, pass_opt_t* opt,
  bool internal);

static reach_type_t* add_type(reach_t* r, ast_t* type, pass_opt_t* opt);

static void reachable_method(reach_t* r, deferred_reification_t* reify,
  ast_t* type, const char* name, ast_t* typeargs, pass_opt_t* opt);

static void reachable_expr(reach_t* r, deferred_reification_t* reify,
  ast_t* ast, pass_opt_t* opt);

static size_t reach_method_hash(reach_method_t* m)
{
  return ponyint_hash_str(m->name);
}

static bool reach_method_cmp(reach_method_t* a, reach_method_t* b)
{
  return a->name == b->name;
}

DEFINE_HASHMAP_SERIALISE(reach_methods, reach_methods_t, reach_method_t,
  reach_method_hash, reach_method_cmp, NULL, reach_method_pony_type());

static size_t reach_mangled_hash(reach_method_t* m)
{
  return ponyint_hash_str(m->mangled_name);
}

static bool reach_mangled_cmp(reach_method_t* a, reach_method_t* b)
{
  return a->mangled_name == b->mangled_name;
}

static void reach_mangled_free(reach_method_t* m)
{
  if(m->param_count > 0)
    ponyint_pool_free_size(m->param_count * sizeof(reach_param_t), m->params);

  if(m->c_method != NULL)
    m->c_method->free_fn(m->c_method);

  // If m->fun exists, it owns the typeargs.
  if(m->fun == NULL)
    ast_free_unattached(m->typeargs);

  deferred_reify_free(m->fun);

  POOL_FREE(reach_method_t, m);
}

DEFINE_HASHMAP_SERIALISE(reach_mangled, reach_mangled_t, reach_method_t,
  reach_mangled_hash, reach_mangled_cmp, reach_mangled_free,
  reach_method_pony_type());

static size_t reach_method_name_hash(reach_method_name_t* n)
{
  return ponyint_hash_str(n->name);
}

static bool reach_method_name_cmp(reach_method_name_t* a,
  reach_method_name_t* b)
{
  return a->name == b->name;
}

static void reach_method_name_free(reach_method_name_t* n)
{
  reach_methods_destroy(&n->r_methods);
  reach_mangled_destroy(&n->r_mangled);
  POOL_FREE(reach_method_name_t, n);
}

DEFINE_HASHMAP_SERIALISE(reach_method_names, reach_method_names_t,
  reach_method_name_t, reach_method_name_hash, reach_method_name_cmp,
  reach_method_name_free, reach_method_name_pony_type());

static size_t reach_type_hash(reach_type_t* t)
{
  return ponyint_hash_str(t->name);
}

static bool reach_type_cmp(reach_type_t* a, reach_type_t* b)
{
  return a->name == b->name;
}

static void reach_type_free(reach_type_t* t)
{
  ast_free(t->ast);
  ast_free(t->ast_cap);
  reach_method_names_destroy(&t->methods);
  reach_type_cache_destroy(&t->subtypes);

  if(t->field_count > 0)
  {
    for(uint32_t i = 0; i < t->field_count; i++)
      ast_free(t->fields[i].ast);

    ponyint_pool_free_size(t->field_count * sizeof(reach_field_t), t->fields);
    t->field_count = 0;
    t->fields = NULL;
  }

  if(t->c_type != NULL)
    t->c_type->free_fn(t->c_type);

  POOL_FREE(reach_type_t, t);
}

DEFINE_HASHMAP_SERIALISE(reach_types, reach_types_t, reach_type_t,
  reach_type_hash, reach_type_cmp, reach_type_free, reach_type_pony_type());

DEFINE_HASHMAP_SERIALISE(reach_type_cache, reach_type_cache_t, reach_type_t,
  reach_type_hash, reach_type_cmp, NULL, reach_type_pony_type());

static reach_method_t* reach_rmethod(reach_method_name_t* n, const char* name)
{
  reach_method_t k;
  k.name = name;
  size_t index = HASHMAP_UNKNOWN;
  return reach_methods_get(&n->r_methods, &k, &index);
}

static reach_method_name_t* add_method_name(reach_type_t* t, const char* name,
  bool internal)
{
  reach_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
  {
    n = POOL_ALLOC(reach_method_name_t);
    n->name = name;
    reach_methods_init(&n->r_methods, 0);
    reach_mangled_init(&n->r_mangled, 0);
    reach_method_names_put(&t->methods, n);

    if(internal)
    {
      n->id = TK_FUN;
      n->cap = TK_BOX;
      n->internal = true;
    } else {
      deferred_reification_t* fun = lookup(NULL, NULL, t->ast, name);
      ast_t* fun_ast = fun->ast;
      n->id = ast_id(fun_ast);
      n->cap = ast_id(ast_child(fun_ast));
      n->internal = false;
      deferred_reify_free(fun);
    }
  }

  return n;
}

static void set_method_types(reach_t* r, reach_method_t* m,
  pass_opt_t* opt)
{
  ast_t* params = ast_childidx(m->fun->ast, 3);
  ast_t* result = ast_sibling(params);

  m->param_count = ast_childcount(params);

  if(m->param_count > 0)
  {
    m->params = (reach_param_t*)ponyint_pool_alloc_size(
      m->param_count * sizeof(reach_param_t));

    ast_t* param = ast_child(params);
    size_t i = 0;

    while(param != NULL)
    {
      ast_t* p_type = deferred_reify(m->fun, ast_type(param), opt);
      ast_set_scope(p_type, NULL);

      m->params[i].name = ast_name(ast_child(param));
      m->params[i].ast = p_type;
      m->params[i].type = add_type(r, p_type, opt);

      if((ast_id(p_type) != TK_NOMINAL) && (ast_id(p_type) != TK_TYPEPARAMREF))
        m->params[i].cap = TK_REF;
      else
        m->params[i].cap = ast_id(cap_fetch(p_type));

      ++i;
      param = ast_sibling(param);
    }
  }

  ast_t* r_result = deferred_reify(m->fun, result, opt);
  m->result = add_type(r, r_result, opt);
  ast_free_unattached(r_result);
}

static const char* make_mangled_name(reach_method_t* m)
{
  // Generate the mangled name.
  // cap_name[_Arg1_Arg2]_args_result
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s_", m->name);

  for(size_t i = 0; i < m->param_count; i++)
    printbuf(buf, "%s", m->params[i].type->mangle);

  if(!m->internal)
    printbuf(buf, "%s", m->result->mangle);
  const char* name = stringtab(buf->m);
  printbuf_free(buf);
  return name;
}

static const char* make_full_name(reach_type_t* t, reach_method_t* m)
{
  // Generate the full mangled name.
  // pkg_Type[_Arg1_Arg2]_cap_name[_Arg1_Arg2]_args_result
  printbuf_t* buf = printbuf_new();
  printbuf(buf, "%s_%s", t->name, m->mangled_name);
  const char* name = stringtab(buf->m);
  printbuf_free(buf);
  return name;
}

static void add_rmethod_to_subtype(reach_t* r, reach_type_t* t,
  reach_method_name_t* n, reach_method_t* m, pass_opt_t* opt)
{
  // Add the method to the type if it isn't already there.
  reach_method_name_t* n2 = add_method_name(t, n->name, false);
  add_rmethod(r, t, n2, m->cap, m->typeargs, opt, false);

  // Add this mangling to the type if it isn't already there.
  size_t index = HASHMAP_UNKNOWN;
  reach_method_t* mangled = reach_mangled_get(&n2->r_mangled, m, &index);

  if(mangled != NULL)
  {
    mangled = POOL_ALLOC(reach_method_t);
    memset(mangled, 0, sizeof(reach_method_t));

    mangled->name = m->name;
    mangled->mangled_name = m->mangled_name;
    mangled->full_name = make_full_name(t, mangled);

    mangled->cap = m->cap;
    mangled->fun = deferred_reify_dup(m->fun);

    if(mangled->fun != NULL)
      mangled->typeargs = mangled->fun->method_typeargs;
    else
      mangled->typeargs = ast_dup(m->typeargs);

    mangled->forwarding = true;

    mangled->param_count = m->param_count;
    mangled->params = (reach_param_t*)ponyint_pool_alloc_size(
      mangled->param_count * sizeof(reach_param_t));
    memcpy(mangled->params, m->params, m->param_count * sizeof(reach_param_t));
    mangled->result = m->result;
  }

  mangled->needs_vtable_index = true;
}

static void add_rmethod_to_subtypes(reach_t* r, reach_type_t* t,
  reach_method_name_t* n, reach_method_t* m, pass_opt_t* opt)
{
  pony_assert(!m->internal);

  switch(t->underlying)
  {
    case TK_UNIONTYPE:
    case TK_INTERFACE:
    case TK_TRAIT:
    {
      size_t i = HASHMAP_BEGIN;
      reach_type_t* t2;

      m->needs_vtable_index = true;
      while((t2 = reach_type_cache_next(&t->subtypes, &i)) != NULL)
        add_rmethod_to_subtype(r, t2, n, m, opt);

      break;
    }

    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(t->ast_cap);
      reach_type_t* t2;

      for(; child != NULL; child = ast_sibling(child))
      {
        deferred_reification_t* find = lookup_try(NULL, NULL, child, n->name,
          false);

        if(find == NULL)
          continue;

        deferred_reify_free(find);

        t2 = add_type(r, child, opt);
        add_rmethod_to_subtype(r, t2, n, m, opt);
      }

      break;
    }

    default: {}
  }
}

static reach_method_t* add_rmethod(reach_t* r, reach_type_t* t,
  reach_method_name_t* n, token_id cap, ast_t* typeargs, pass_opt_t* opt,
  bool internal)
{
  const char* name = genname_fun(cap, n->name, typeargs);
  reach_method_t* m = reach_rmethod(n, name);

  if(m != NULL)
    return m;

  m = POOL_ALLOC(reach_method_t);
  memset(m, 0, sizeof(reach_method_t));
  m->name = name;
  m->cap = cap;
  m->vtable_index = (uint32_t)-1;
  m->internal = internal;
  m->intrinsic = internal;

  if(!internal)
  {
    ast_t* r_ast = set_cap_and_ephemeral(t->ast, cap, TK_NONE);
    deferred_reification_t* fun = lookup(NULL, NULL, r_ast, n->name);
    pony_assert(fun != NULL);

    // The typeargs and thistype are in the scope of r_ast but we're going to
    // free it. Change the scope to a durable AST.
    if(fun->type_typeargs != NULL)
      ast_set_scope(fun->type_typeargs, t->ast);

    ast_set_scope(fun->thistype, t->ast);

    ast_free_unattached(r_ast);

    if(typeargs != NULL)
    {
      ast_t* typeparams = ast_childidx(fun->ast, 2);
      deferred_reify_add_method_typeparams(fun, typeparams, typeargs, opt);
      m->typeargs = fun->method_typeargs;
    }

    m->fun = fun;
    set_method_types(r, m, opt);
  }

  m->mangled_name = make_mangled_name(m);
  m->full_name = make_full_name(t, m);

  // Add to both tables.
  reach_methods_put(&n->r_methods, m);
  reach_mangled_put(&n->r_mangled, m);

  if(!internal)
  {
    // Put on a stack of reachable methods to trace.
    r->method_stack = reach_method_stack_push(r->method_stack, m);

    // Add the method to any subtypes.
    add_rmethod_to_subtypes(r, t, n, m, opt);
  }

  return m;
}

static void add_methods_to_type(reach_t* r, reach_type_t* from,
  reach_type_t* to, pass_opt_t* opt)
{
  size_t i = HASHMAP_BEGIN;
  reach_method_name_t* n;

  while((n = reach_method_names_next(&from->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reach_method_t* m;

    while((m = reach_mangled_next(&n->r_mangled, &j)) != NULL)
    {
      if(!m->internal)
        add_rmethod_to_subtype(r, to, n, m, opt);
    }
  }
}

static void add_internal(reach_t* r, reach_type_t* t, const char* name,
  pass_opt_t* opt)
{
  name = stringtab(name);
  reach_method_name_t* n = add_method_name(t, name, true);
  reach_method_t* m = add_rmethod(r, t, n, TK_BOX, NULL, opt, true);
  m->needs_vtable_index = true;
}

static void add_types_to_trait(reach_t* r, reach_type_t* t,
  pass_opt_t* opt)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t2;

  bool interface = false;
  switch(ast_id(t->ast))
  {
    case TK_NOMINAL:
    {
      ast_t* def = (ast_t*)ast_data(t->ast);
      interface = ast_id(def) == TK_INTERFACE;
      break;
    }

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      interface = true;
      break;

    default: {}
  }

  while((t2 = reach_types_next(&r->types, &i)) != NULL)
  {
    switch(ast_id(t2->ast))
    {
      case TK_NOMINAL:
      {
        ast_t* def2 = (ast_t*)ast_data(t2->ast);

        switch(ast_id(def2))
        {
          case TK_INTERFACE:
            // Use the same typeid.
            if(interface && is_eqtype(t->ast, t2->ast, NULL, opt))
              t->type_id = t2->type_id;
            break;

          case TK_PRIMITIVE:
          case TK_CLASS:
          case TK_ACTOR:
            if(is_subtype(t2->ast, t->ast, NULL, opt))
            {
              reach_type_cache_put(&t->subtypes, t2);
              reach_type_cache_put(&t2->subtypes, t);

              if(ast_id(t->ast) == TK_NOMINAL)
                add_methods_to_type(r, t, t2, opt);

              if(t2->can_be_boxed)
                add_internal(r, t, "__digestof", opt);
            }
            break;

          default: {}
        }

        break;
      }

      case TK_UNIONTYPE:
      case TK_ISECTTYPE:
        // Use the same typeid.
        if(interface && is_eqtype(t->ast, t2->ast, NULL, opt))
          t->type_id = t2->type_id;
        break;

      case TK_TUPLETYPE:
        if(is_subtype(t2->ast, t->ast, NULL, opt))
        {
          reach_type_cache_put(&t->subtypes, t2);
          reach_type_cache_put(&t2->subtypes, t);
          add_internal(r, t, "__is", opt);
          add_internal(r, t, "__digestof", opt);
        }

        break;

      default: {}
    }
  }
}

static void add_traits_to_type(reach_t* r, reach_type_t* t,
  pass_opt_t* opt)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t2;

  while((t2 = reach_types_next(&r->types, &i)) != NULL)
  {
    if(ast_id(t2->ast) == TK_NOMINAL)
    {
      ast_t* def = (ast_t*)ast_data(t2->ast);

      switch(ast_id(def))
      {
        case TK_INTERFACE:
        case TK_TRAIT:
          if(is_subtype(t->ast, t2->ast, NULL, opt))
          {
            reach_type_cache_put(&t->subtypes, t2);
            reach_type_cache_put(&t2->subtypes, t);
            add_methods_to_type(r, t2, t, opt);

            if(t->underlying == TK_TUPLETYPE)
              add_internal(r, t2, "__is", opt);

            if(t->can_be_boxed)
              add_internal(r, t2, "__digestof", opt);
          }
          break;

        default: {}
      }
    } else {
      switch(ast_id(t2->ast))
      {
        case TK_UNIONTYPE:
        case TK_ISECTTYPE:
          if(is_subtype(t->ast, t2->ast, NULL, opt))
          {
            reach_type_cache_put(&t->subtypes, t2);
            reach_type_cache_put(&t2->subtypes, t);
            add_methods_to_type(r, t2, t, opt);

            if(t->underlying == TK_TUPLETYPE)
              add_internal(r, t2, "__is", opt);

            if(t->can_be_boxed)
              add_internal(r, t2, "__digestof", opt);
          }
          break;

        default: {}
      }
    }
  }
}

static void add_special(reach_t* r, reach_type_t* t, ast_t* type,
  const char* special, pass_opt_t* opt)
{
  special = stringtab(special);
  deferred_reification_t* find = lookup_try(NULL, NULL, type, special, false);

  if(find != NULL)
  {
    switch(ast_id(find->ast))
    {
      case TK_NEW:
      case TK_FUN:
      case TK_BE:
        reachable_method(r, NULL, t->ast, special, NULL, opt);
        break;

      default: {}
    }

    deferred_reify_free(find);
  }
}

static bool embed_has_finaliser(ast_t* ast, const char* str_final)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      break;

    default:
      return false;
  }

  ast_t* def = (ast_t*)ast_data(ast);
  if(ast_get(def, str_final, NULL) != NULL)
    return true;

  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    if((ast_id(member) == TK_EMBED) &&
      embed_has_finaliser(ast_type(member), str_final))
      return true;

    member = ast_sibling(member);
  }

  return false;
}

static void add_fields(reach_t* r, reach_type_t* t, pass_opt_t* opt)
{
  ast_t* def = (ast_t*)ast_data(t->ast);
  ast_t* typeargs = ast_childidx(t->ast, 2);
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        t->field_count++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(t->field_count == 0)
    return;

  t->fields = (reach_field_t*)ponyint_pool_alloc_size(
    t->field_count * sizeof(reach_field_t));
  member = ast_child(members);
  size_t index = 0;

  const char* str_final = stringtab("_final");
  bool has_finaliser = ast_get(def, str_final, NULL) != NULL;
  bool needs_finaliser = false;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
      {
        deferred_reification_t* member_lookup = lookup(NULL, NULL, t->ast,
          ast_name(ast_child(member)));
        pony_assert(member_lookup != NULL);

        ast_t* r_member = deferred_reify(member_lookup, member_lookup->ast,
          opt);
        deferred_reify_free(member_lookup);

        ast_t* name = ast_pop(r_member);
        ast_t* type = ast_pop(r_member);
        ast_add(r_member, name);
        ast_set_scope(type, member);

        bool embed = t->fields[index].embed = ast_id(member) == TK_EMBED;
        t->fields[index].ast = reify(ast_type(member), typeparams, typeargs,
          opt, true);
        ast_setpos(t->fields[index].ast, NULL, ast_line(name), ast_pos(name));
        t->fields[index].type = add_type(r, type, opt);

        if(embed && !has_finaliser && !needs_finaliser)
          needs_finaliser = embed_has_finaliser(type, str_final);

        ast_free_unattached(r_member);

        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  if(!has_finaliser && needs_finaliser)
  {
    reach_method_name_t* n = add_method_name(t, stringtab("_final"), true);
    add_rmethod(r, t, n, TK_BOX, NULL, opt, true);
  }
}

// Type IDs are assigned as:
// - Object type IDs:  1, 3, 5, 7, 9, ...
// - Numeric type IDs: 0, 4, 8, 12, 16, ...
// - Tuple type IDs:   2, 6, 10, 14, 18, ...
// This allows to quickly check whether a type is unboxed or not.
// Trait IDs use their own incremental numbering.

static uint32_t get_new_trait_id(reach_t* r)
{
  return r->trait_type_count++;
}

static uint32_t get_new_object_id(reach_t* r)
{
  r->total_type_count++;
  return (r->object_type_count++ * 2) + 1;
}

static uint32_t get_new_numeric_id(reach_t* r)
{
  r->total_type_count++;
  return r->numeric_type_count++ * 4;
}

static uint32_t get_new_tuple_id(reach_t* r)
{
  r->total_type_count++;
  return (r->tuple_type_count++ * 4) + 2;
}

static reach_type_t* add_reach_type(reach_t* r, ast_t* type)
{
  reach_type_t* t = POOL_ALLOC(reach_type_t);
  memset(t, 0, sizeof(reach_type_t));

  t->name = genname_type(type);
  t->mangle = "o";
  t->ast = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
  t->ast_cap = ast_dup(type);
  t->type_id = (uint32_t)-1;

  ast_set_scope(t->ast, NULL);
  ast_set_scope(t->ast_cap, NULL);

  reach_method_names_init(&t->methods, 0);
  reach_type_cache_init(&t->subtypes, 0);
  reach_types_put(&r->types, t);

  return t;
}

static reach_type_t* add_isect_or_union(reach_t* r, ast_t* type,
  pass_opt_t* opt)
{
  reach_type_t* t = reach_type(r, type);

  if(t != NULL)
    return t;

  t = add_reach_type(r, type);
  t->underlying = ast_id(t->ast);
  t->is_trait = true;

  add_types_to_trait(r, t, opt);

  if(t->type_id == (uint32_t)-1)
    t->type_id = get_new_trait_id(r);

  ast_t* child = ast_child(type);

  while(child != NULL)
  {
    add_type(r, child, opt);
    child = ast_sibling(child);
  }

  return t;
}

static reach_type_t* add_tuple(reach_t* r, ast_t* type, pass_opt_t* opt)
{
  if(contains_dontcare(type))
    return NULL;

  reach_type_t* t = reach_type(r, type);

  if(t != NULL)
    return t;

  t = add_reach_type(r, type);
  t->underlying = TK_TUPLETYPE;
  t->type_id = get_new_tuple_id(r);
  t->can_be_boxed = true;

  t->field_count = (uint32_t)ast_childcount(t->ast);
  t->fields = (reach_field_t*)ponyint_pool_alloc_size(
      t->field_count * sizeof(reach_field_t));

  add_traits_to_type(r, t, opt);
  add_internal(r, t, "__is", opt);
  add_internal(r, t, "__digestof", opt);

  printbuf_t* mangle = printbuf_new();
  printbuf(mangle, "%d", t->field_count);

  ast_t* child = ast_child(t->ast_cap);
  size_t index = 0;

  while(child != NULL)
  {
    t->fields[index].ast = ast_dup(child);
    t->fields[index].type = add_type(r, child, opt);
    t->fields[index].embed = false;
    printbuf(mangle, "%s", t->fields[index].type->mangle);
    index++;

    child = ast_sibling(child);
  }

  t->mangle = stringtab(mangle->m);
  printbuf_free(mangle);
  return t;
}

static reach_type_t* add_nominal(reach_t* r, ast_t* type, pass_opt_t* opt)
{
  reach_type_t* t = reach_type(r, type);

  if(t != NULL)
    return t;

  t = add_reach_type(r, type);
  ast_t* def = (ast_t*)ast_data(type);
  t->underlying = ast_id(def);

  AST_GET_CHILDREN(type, pkg, id, typeargs);
  ast_t* typearg = ast_child(typeargs);

  while(typearg != NULL)
  {
    add_type(r, typearg, opt);
    typearg = ast_sibling(typearg);
  }

  switch(t->underlying)
  {
    case TK_INTERFACE:
    case TK_TRAIT:
      add_types_to_trait(r, t, opt);
      t->is_trait = true;
      break;

    case TK_PRIMITIVE:
      if(is_machine_word(type))
      {
        t->can_be_boxed = true;
        add_internal(r, t, "__digestof", opt);
      }

      add_traits_to_type(r, t, opt);
      add_special(r, t, type, "_init", opt);
      add_special(r, t, type, "_final", opt);
      break;

    case TK_STRUCT:
    case TK_CLASS:
      add_traits_to_type(r, t, opt);
      add_special(r, t, type, "_final", opt);
      add_special(r, t, type, "_serialise_space", opt);
      add_special(r, t, type, "_serialise", opt);
      add_special(r, t, type, "_deserialise", opt);
      add_fields(r, t, opt);
      break;

    case TK_ACTOR:
      add_traits_to_type(r, t, opt);
      add_special(r, t, type, "_event_notify", opt);
      add_special(r, t, type, "_final", opt);
      add_fields(r, t, opt);
      break;

    default: {}
  }

  bool bare = false;

  if(is_bare(type))
  {
    bare = true;

    ast_t* bare_method = NULL;
    ast_t* member = ast_child(ast_childidx(def, 4));

    while(member != NULL)
    {
      if((ast_id(member) == TK_FUN) && (ast_id(ast_child(member)) == TK_AT))
      {
        // Only one bare method per bare type.
        pony_assert(bare_method == NULL);
        bare_method = member;
#ifdef PONY_NDEBUG
        break;
#endif
      }

      member = ast_sibling(member);
    }

    pony_assert(bare_method != NULL);
    AST_GET_CHILDREN(bare_method, cap, name, typeparams);
    pony_assert(ast_id(typeparams) == TK_NONE);

    reach_method_name_t* n = add_method_name(t, ast_name(name), false);
    t->bare_method = add_rmethod(r, t, n, TK_AT, NULL, opt, false);
  }

  if(t->type_id == (uint32_t)-1)
  {
    if(t->is_trait && !bare)
      t->type_id = get_new_trait_id(r);
    else if(t->can_be_boxed)
      t->type_id = get_new_numeric_id(r);
    else if(t->underlying != TK_STRUCT)
      t->type_id = get_new_object_id(r);
  }

  if(ast_id(def) != TK_PRIMITIVE)
    return t;

  if(strcmp(ast_name(pkg), "$0"))
    return t;

  const char* name = ast_name(id);

  if(name[0] == 'I')
  {
    if(!strcmp(name, "I8"))
      t->mangle = "c";
    else if(!strcmp(name, "I16"))
      t->mangle = "s";
    else if(!strcmp(name, "I32"))
      t->mangle = "i";
    else if(!strcmp(name, "I64"))
      t->mangle = "w";
    else if(!strcmp(name, "I128"))
      t->mangle = "q";
    else if(!strcmp(name, "ILong"))
      t->mangle = "l";
    else if(!strcmp(name, "ISize"))
      t->mangle = "z";
  } else if(name[0] == 'U') {
    if(!strcmp(name, "U8"))
      t->mangle = "C";
    else if(!strcmp(name, "U16"))
      t->mangle = "S";
    else if(!strcmp(name, "U32"))
      t->mangle = "I";
    else if(!strcmp(name, "U64"))
      t->mangle = "W";
    else if(!strcmp(name, "U128"))
      t->mangle = "Q";
    else if(!strcmp(name, "ULong"))
      t->mangle = "L";
    else if(!strcmp(name, "USize"))
      t->mangle = "Z";
  } else if(name[0] == 'F') {
    if(!strcmp(name, "F32"))
      t->mangle = "f";
    else if(!strcmp(name, "F64"))
      t->mangle = "d";
  } else if(!strcmp(name, "Bool")) {
    t->mangle = "b";
  }

  return t;
}

static reach_type_t* add_type(reach_t* r, ast_t* type, pass_opt_t* opt)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
      return add_isect_or_union(r, type, opt);

    case TK_TUPLETYPE:
      return add_tuple(r, type, opt);

    case TK_NOMINAL:
      return add_nominal(r, type, opt);

    default:
      pony_assert(0);
  }

  return NULL;
}

static void reachable_pattern(reach_t* r, deferred_reification_t* reify,
  ast_t* ast, pass_opt_t* opt)
{
  switch(ast_id(ast))
  {
    case TK_NONE:
      break;

    case TK_MATCH_CAPTURE:
    case TK_MATCH_DONTCARE:
    {
      AST_GET_CHILDREN(ast, idseq, type);
      ast_t* r_type = deferred_reify(reify, type, opt);
      add_type(r, r_type, opt);
      ast_free_unattached(r_type);
      break;
    }

    case TK_TUPLE:
    case TK_SEQ:
    {
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        reachable_pattern(r, reify, child, opt);
        child = ast_sibling(child);
      }
      break;
    }

    default:
    {
      ast_t* type = ast_type(ast);

      if(ast_id(type) != TK_DONTCARETYPE)
      {
        // type will be reified in reachable_method
        reachable_method(r, reify, type, stringtab("eq"), NULL, opt);
        reachable_expr(r, reify, ast, opt);
      }
      break;
    }
  }
}

static void reachable_fun(reach_t* r, deferred_reification_t* reify, ast_t* ast,
  pass_opt_t* opt)
{
  AST_GET_CHILDREN(ast, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
    case TK_BECHAIN:
    case TK_FUNCHAIN:
      typeargs = deferred_reify(reify, method, opt);
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);

  // type will be reified in reachable_method
  reachable_method(r, reify, type, method_name, typeargs, opt);

  ast_free_unattached(typeargs);
}

static void reachable_addressof(reach_t* r, deferred_reification_t* reify,
  ast_t* ast, pass_opt_t* opt)
{
  ast_t* expr = ast_child(ast);

  switch(ast_id(expr))
  {
    case TK_FUNREF:
    case TK_BEREF:
      reachable_fun(r, reify, expr, opt);
      break;

    default: {}
  }
}

static void reachable_call(reach_t* r, deferred_reification_t* reify,
  ast_t* ast, pass_opt_t* opt)
{
  AST_GET_CHILDREN(ast, postfix, positional, named, question);
  reachable_fun(r, reify, postfix, opt);
}

static void reachable_ffi(reach_t* r, ast_t* ast, pass_opt_t* opt)
{
  ast_t* decl = (ast_t*)ast_data(ast);
  pony_assert(decl != NULL);

  AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
    decl_error);

  ast_t* arg = ast_child(params);
  ast_t* return_type = ast_child(decl_ret_typeargs);
  add_type(r, return_type, opt);
  while(arg != NULL)
  {
    if(ast_id(arg) != TK_ELLIPSIS)
    {
      ast_t* type = ast_type(arg);

      if(type == NULL)
        type = ast_childidx(arg, 1);

      add_type(r, type, opt);
    }

    arg = ast_sibling(arg);
  }
}

static void reachable_expr(reach_t* r, deferred_reification_t* reify,
  ast_t* ast, pass_opt_t* opt)
{
  // If this is a method call, mark the method as reachable.
  switch(ast_id(ast))
  {
    case TK_TRUE:
    case TK_FALSE:
    case TK_INT:
    case TK_FLOAT:
    case TK_STRING:
    {
      ast_t* type = ast_type(ast);

      // type will be reified in reachable_method
      if(type != NULL)
        reachable_method(r, reify, type, stringtab("create"), NULL, opt);

      break;
    }

    case TK_LET:
    case TK_VAR:
    case TK_FLETREF:
    case TK_FVARREF:
    case TK_EMBEDREF:
    case TK_TUPLE:
    {
      ast_t* type = deferred_reify(reify, ast_type(ast), opt);
      add_type(r, type, opt);
      ast_free_unattached(type);
      break;
    }

    case TK_CASE:
    {
      AST_GET_CHILDREN(ast, pattern, guard, body);
      reachable_pattern(r, reify, pattern, opt);
      reachable_expr(r, reify, guard, opt);
      reachable_expr(r, reify, body, opt);
      break;
    }

    case TK_CALL:
      reachable_call(r, reify, ast, opt);
      break;

    case TK_FFICALL:
      reachable_ffi(r, ast, opt);
      break;

    case TK_ADDRESS:
      reachable_addressof(r, reify, ast, opt);
      break;

    case TK_IF:
    {
      AST_GET_CHILDREN(ast, cond, then_clause, else_clause);
      pony_assert(ast_id(cond) == TK_SEQ);
      cond = ast_child(cond);

      if(is_result_needed(ast) && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
      {
        ast_t* type = deferred_reify(reify, ast_type(ast), opt);
        add_type(r, type, opt);
        ast_free_unattached(type);
      }

      if(ast_sibling(cond) == NULL)
      {
        if(ast_id(cond) == TK_TRUE)
        {
          reachable_expr(r, reify, then_clause, opt);
          return;
        } else if(ast_id(cond) == TK_FALSE) {
          reachable_expr(r, reify, else_clause, opt);
          return;
        }
      }
      break;
    }

    case TK_IFTYPE_SET:
    {
      AST_GET_CHILDREN(ast, left_clause, right);
      AST_GET_CHILDREN(left_clause, sub, super, left);


      if(is_result_needed(ast) && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
      {
        ast_t* type = deferred_reify(reify, ast_type(ast), opt);
        add_type(r, type, opt);
        ast_free_unattached(type);
      }

      ast_t* r_sub = deferred_reify(reify, sub, opt);
      ast_t* r_super = deferred_reify(reify, super, opt);

      if(is_subtype_constraint(r_sub, r_super, NULL, opt))
        reachable_expr(r, reify, left, opt);
      else
        reachable_expr(r, reify, right, opt);

      ast_free_unattached(r_sub);
      ast_free_unattached(r_super);

      return;
    }

    case TK_MATCH:
    case TK_WHILE:
    case TK_REPEAT:
    case TK_TRY:
    case TK_RECOVER:
    {
      if(is_result_needed(ast) && !ast_checkflag(ast, AST_FLAG_JUMPS_AWAY))
      {
        ast_t* type = deferred_reify(reify, ast_type(ast), opt);
        add_type(r, type, opt);
        ast_free_unattached(type);
      }

      break;
    }

    case TK_IS:
    case TK_ISNT:
    {
      AST_GET_CHILDREN(ast, left, right);

      ast_t* type = deferred_reify(reify, ast_type(left), opt);
      add_type(r, type, opt);
      ast_free_unattached(type);

      type = deferred_reify(reify, ast_type(right), opt);
      add_type(r, type, opt);
      ast_free_unattached(type);

      break;
    }

    case TK_DIGESTOF:
    {
      ast_t* expr = ast_child(ast);
      ast_t* type = deferred_reify(reify, ast_type(expr), opt);
      add_type(r, type, opt);
      ast_free_unattached(type);

      break;
    }

    default: {}
  }

  // Traverse all child expressions looking for calls.
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    reachable_expr(r, reify, child, opt);
    child = ast_sibling(child);
  }
}

static void reachable_method(reach_t* r, deferred_reification_t* reify,
  ast_t* type, const char* name, ast_t* typeargs, pass_opt_t* opt)
{
  reach_type_t* t;

  if(reify != NULL)
  {
    ast_t* r_type = deferred_reify(reify, type, opt);
    t = add_type(r, r_type, opt);
    ast_free_unattached(r_type);
  } else {
    t = add_type(r, type, opt);
  }

  reach_method_name_t* n = add_method_name(t, name, false);
  reach_method_t* m = add_rmethod(r, t, n, n->cap, typeargs, opt, false);

  if((n->id == TK_FUN) && ((n->cap == TK_BOX) || (n->cap == TK_TAG)))
  {
    if(name == stringtab("_final"))
    {
      // If the method is a finaliser, don't mark the ref and val versions as
      // reachable.
      pony_assert(n->cap == TK_BOX);
      return;
    }

    // TODO: if it doesn't use this-> in a constructor, we could reuse the
    // function, which means always reuse in a fun tag
    bool subordinate = (n->cap == TK_TAG);
    reach_method_t* m2;

    if(t->underlying != TK_PRIMITIVE)
    {
      m2 = add_rmethod(r, t, n, TK_REF, typeargs, opt, false);

      if(subordinate)
      {
        m2->intrinsic = true;
        m->subordinate = m2;
        m = m2;
      }
    }

    m2 = add_rmethod(r, t, n, TK_VAL, typeargs, opt, false);

    if(subordinate)
    {
      m2->intrinsic = true;
      m->subordinate = m2;
      m = m2;
    }

    if(n->cap == TK_TAG)
    {
      m2 = add_rmethod(r, t, n, TK_BOX, typeargs, opt, false);
      m2->intrinsic = true;
      m->subordinate = m2;
      m = m2;
    }
  }
}

static void handle_method_stack(reach_t* r, pass_opt_t* opt)
{
  while(r->method_stack != NULL)
  {
    reach_method_t* m;
    r->method_stack = reach_method_stack_pop(r->method_stack, &m);

    ast_t* body = ast_childidx(m->fun->ast, 6);
    reachable_expr(r, m->fun, body, opt);
  }
}

reach_t* reach_new()
{
  reach_t* r = POOL_ALLOC(reach_t);
  r->method_stack = NULL;
  r->object_type_count = 0;
  r->numeric_type_count = 0;
  r->tuple_type_count = 0;
  r->total_type_count = 0;
  r->trait_type_count = 0;
  reach_types_init(&r->types, 64);
  return r;
}

void reach_free(reach_t* r)
{
  if(r == NULL)
    return;

  reach_types_destroy(&r->types);
  POOL_FREE(reach_t, r);
}

void reach(reach_t* r, ast_t* type, const char* name, ast_t* typeargs,
  pass_opt_t* opt)
{
  reachable_method(r, NULL, type, name, typeargs, opt);
  handle_method_stack(r, opt);
}

reach_type_t* reach_type(reach_t* r, ast_t* type)
{
  reach_type_t k;
  k.name = genname_type(type);
  size_t index = HASHMAP_UNKNOWN;
  return reach_types_get(&r->types, &k, &index);
}

reach_type_t* reach_type_name(reach_t* r, const char* name)
{
  reach_type_t k;
  k.name = stringtab(name);
  size_t index = HASHMAP_UNKNOWN;
  return reach_types_get(&r->types, &k, &index);
}

reach_method_t* reach_method(reach_type_t* t, token_id cap,
  const char* name, ast_t* typeargs)
{
  reach_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
    return NULL;

  if((n->id == TK_FUN) && ((n->cap == TK_BOX) || (n->cap == TK_TAG)))
  {
    switch(cap)
    {
      case TK_ISO:
      case TK_TRN:
        cap = TK_REF;
        break;

      case TK_REF:
      case TK_VAL:
      case TK_BOX:
        break;

      default:
        cap = n->cap;
    }
  } else {
    cap = n->cap;
  }

  name = genname_fun(cap, n->name, typeargs);
  return reach_rmethod(n, name);
}

reach_method_name_t* reach_method_name(reach_type_t* t, const char* name)
{
  reach_method_name_t k;
  k.name = name;
  size_t index = HASHMAP_UNKNOWN;
  return reach_method_names_get(&t->methods, &k, &index);
}

uint32_t reach_vtable_index(reach_type_t* t, const char* name)
{
  reach_method_t* m = reach_method(t, TK_NONE, name, NULL);

  if(m == NULL)
    return (uint32_t)-1;

  return m->vtable_index;
}

uint32_t reach_max_type_id(reach_t* r)
{
  uint32_t object_id_max = (r->object_type_count * 2) + 1;
  uint32_t numeric_id_max = r->numeric_type_count * 4;
  uint32_t tuple_id_max = (r->tuple_type_count * 4) + 2;

  uint32_t len = object_id_max;

  if(len < numeric_id_max)
    len = numeric_id_max;

  if(len < tuple_id_max)
    len = tuple_id_max;

  return len;
}

void reach_dump(reach_t* r)
{
  printf("REACH\n");

  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;

  while((t = reach_types_next(&r->types, &i)) != NULL)
  {
    printf("  %d: %s, %s\n", t->type_id, t->name, t->mangle);
    size_t j = HASHMAP_BEGIN;
    reach_method_name_t* n;

    printf("    vtable: %d\n", t->vtable_size);

    while((n = reach_method_names_next(&t->methods, &j)) != NULL)
    {
      size_t k = HASHMAP_BEGIN;
      reach_method_t* m;

      while((m = reach_mangled_next(&n->r_mangled, &k)) != NULL)
        printf("      %d: %s\n", m->vtable_index, m->mangled_name);
    }

    j = HASHMAP_BEGIN;
    reach_type_t* t2;

    while((t2 = reach_type_cache_next(&t->subtypes, &j)) != NULL)
    {
      printf("    %s\n", t2->name);
    }
  }
}

static void reach_param_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_param_t* p = (reach_param_t*)object;

  string_trace(ctx, p->name);
  pony_traceknown(ctx, p->ast, ast_pony_type(), PONY_TRACE_MUTABLE);
  pony_traceknown(ctx, p->type, reach_type_pony_type(), PONY_TRACE_MUTABLE);
}

static void reach_param_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  reach_param_t* p = (reach_param_t*)object;
  reach_param_t* dst = (reach_param_t*)((uintptr_t)buf + offset);

  dst->name = (const char*)pony_serialise_offset(ctx, (char*)p->name);
  dst->ast = (ast_t*)pony_serialise_offset(ctx, p->ast);
  dst->type = (reach_type_t*)pony_serialise_offset(ctx, p->type);
  dst->cap = p->cap;
}

static void reach_param_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_param_t* p = (reach_param_t*)object;

  p->name = string_deserialise_offset(ctx, (uintptr_t)p->name);
  p->type = (reach_type_t*)pony_deserialise_offset(ctx, reach_type_pony_type(),
    (uintptr_t)p->type);
  p->ast = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)p->ast);
}

static pony_type_t reach_param_pony =
{
  0,
  sizeof(reach_param_t),
  0,
  0,
  NULL,
  NULL,
  reach_param_serialise_trace,
  reach_param_serialise,
  reach_param_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_param_pony_type()
{
  return &reach_param_pony;
}

static void reach_method_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_method_t* m = (reach_method_t*)object;

  string_trace(ctx, m->name);
  string_trace(ctx, m->mangled_name);
  string_trace(ctx, m->full_name);

  if(m->fun != NULL)
    pony_traceknown(ctx, m->fun, deferred_reification_pony_type(),
      PONY_TRACE_MUTABLE);

  if(m->typeargs != NULL)
    pony_traceknown(ctx, m->typeargs, ast_pony_type(), PONY_TRACE_MUTABLE);

  if(m->subordinate != NULL)
    pony_traceknown(ctx, m->subordinate, reach_method_pony_type(),
      PONY_TRACE_MUTABLE);

  if(m->params != NULL)
  {
    pony_serialise_reserve(ctx, m->params,
      m->param_count * sizeof(reach_param_t));

    for(size_t i = 0; i < m->param_count; i++)
      reach_param_serialise_trace(ctx, &m->params[i]);
  }

  if(m->result != NULL)
    pony_traceknown(ctx, m->result, reach_type_pony_type(), PONY_TRACE_MUTABLE);
}

static void reach_method_serialise(pony_ctx_t* ctx, void* object, void* buf,
   size_t offset, int mutability)
{
  (void)mutability;

  reach_method_t* m = (reach_method_t*)object;
  reach_method_t* dst = (reach_method_t*)((uintptr_t)buf + offset);

  dst->name = (const char*)pony_serialise_offset(ctx, (char*)m->name);
  dst->mangled_name = (const char*)pony_serialise_offset(ctx,
    (char*)m->mangled_name);
  dst->full_name = (const char*)pony_serialise_offset(ctx, (char*)m->full_name);

  dst->cap = m->cap;
  dst->fun = (deferred_reification_t*)pony_serialise_offset(ctx, m->fun);
  dst->typeargs = (ast_t*)pony_serialise_offset(ctx, m->typeargs);
  dst->vtable_index = m->vtable_index;

  dst->intrinsic = m->intrinsic;
  dst->internal = m->internal;
  dst->forwarding = m->forwarding;

  dst->subordinate = (reach_method_t*)pony_serialise_offset(ctx,
    m->subordinate);

  dst->param_count = m->param_count;
  dst->params = (reach_param_t*)pony_serialise_offset(ctx, m->params);

  if(m->params != NULL)
  {
    size_t param_offset = (size_t)dst->params;

    for(size_t i = 0; i < m->param_count; i++)
    {
      reach_param_serialise(ctx, &m->params[i], buf, param_offset,
        PONY_TRACE_MUTABLE);
      param_offset += sizeof(reach_param_t);
    }
  }

  dst->result = (reach_type_t*)pony_serialise_offset(ctx, m->result);
  dst->c_method = NULL;
}

static void reach_method_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_method_t* m = (reach_method_t*)object;

  m->name = string_deserialise_offset(ctx, (uintptr_t)m->name);
  m->mangled_name = string_deserialise_offset(ctx, (uintptr_t)m->mangled_name);
  m->full_name = string_deserialise_offset(ctx, (uintptr_t)m->full_name);

  m->fun = (deferred_reification_t*)pony_deserialise_offset(ctx,
    deferred_reification_pony_type(), (uintptr_t)m->fun);

  m->typeargs = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)m->typeargs);

  m->subordinate = (reach_method_t*)pony_deserialise_offset(ctx,
    reach_method_pony_type(), (uintptr_t)m->subordinate);

  if(m->param_count > 0)
  {
    m->params = (reach_param_t*)pony_deserialise_block(ctx, (uintptr_t)m->params,
      m->param_count * sizeof(reach_param_t));

    for(size_t i = 0; i < m->param_count; i++)
      reach_param_deserialise(ctx, &m->params[i]);
  } else {
    m->params = NULL;
  }

  m->result = (reach_type_t*)pony_deserialise_offset(ctx, reach_type_pony_type(),
    (uintptr_t)m->result);
}

static pony_type_t reach_method_pony =
{
  0,
  sizeof(reach_method_t),
  0,
  0,
  NULL,
  NULL,
  reach_method_serialise_trace,
  reach_method_serialise,
  reach_method_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_method_pony_type()
{
  return &reach_method_pony;
}

static void reach_method_name_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_method_name_t* n = (reach_method_name_t*)object;

  string_trace(ctx, n->name);
  reach_methods_serialise_trace(ctx, &n->r_methods);
  reach_mangled_serialise_trace(ctx, &n->r_mangled);
}

static void reach_method_name_serialise(pony_ctx_t* ctx, void* object,
  void* buf, size_t offset, int mutability)
{
  (void)mutability;

  reach_method_name_t* n = (reach_method_name_t*)object;
  reach_method_name_t* dst = (reach_method_name_t*)((uintptr_t)buf + offset);

  dst->id = n->id;
  dst->cap = n->cap;
  dst->name = (const char*)pony_serialise_offset(ctx, (char*)n->name);
  dst->internal = n->internal;
  reach_methods_serialise(ctx, &n->r_methods, buf,
    offset + offsetof(reach_method_name_t, r_methods), PONY_TRACE_MUTABLE);
  reach_mangled_serialise(ctx, &n->r_mangled, buf,
    offset + offsetof(reach_method_name_t, r_mangled), PONY_TRACE_MUTABLE);
}

static void reach_method_name_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_method_name_t* n = (reach_method_name_t*)object;

  n->name = string_deserialise_offset(ctx, (uintptr_t)n->name);
  reach_methods_deserialise(ctx, &n->r_methods);
  reach_mangled_deserialise(ctx, &n->r_mangled);
}

static pony_type_t reach_method_name_pony =
{
  0,
  sizeof(reach_method_name_t),
  0,
  0,
  NULL,
  NULL,
  reach_method_name_serialise_trace,
  reach_method_name_serialise,
  reach_method_name_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_method_name_pony_type()
{
  return &reach_method_name_pony;
}

static void reach_field_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_field_t* f = (reach_field_t*)object;

  pony_traceknown(ctx, f->ast, ast_pony_type(), PONY_TRACE_MUTABLE);
  pony_traceknown(ctx, f->type, reach_type_pony_type(), PONY_TRACE_MUTABLE);
}

static void reach_field_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  reach_field_t* f = (reach_field_t*)object;
  reach_field_t* dst = (reach_field_t*)((uintptr_t)buf + offset);

  dst->ast = (ast_t*)pony_serialise_offset(ctx, f->ast);
  dst->type = (reach_type_t*)pony_serialise_offset(ctx, f->type);
  dst->embed = f->embed;
}

static void reach_field_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_field_t* f = (reach_field_t*)object;

  f->ast = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)f->ast);
  f->type = (reach_type_t*)pony_deserialise_offset(ctx, reach_type_pony_type(),
    (uintptr_t)f->type);
}

static pony_type_t reach_field_pony =
{
  0,
  sizeof(reach_field_t),
  0,
  0,
  NULL,
  NULL,
  reach_field_serialise_trace,
  reach_field_serialise,
  reach_field_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_field_pony_type()
{
  return &reach_field_pony;
}

static void reach_type_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_type_t* t = (reach_type_t*)object;

  string_trace(ctx, t->name);
  string_trace(ctx, t->mangle);
  pony_traceknown(ctx, t->ast, ast_pony_type(), PONY_TRACE_MUTABLE);
  pony_traceknown(ctx, t->ast_cap, ast_pony_type(), PONY_TRACE_MUTABLE);

  reach_method_names_serialise_trace(ctx, &t->methods);

  if(t->bare_method != NULL)
    pony_traceknown(ctx, t->bare_method, reach_method_pony_type(),
      PONY_TRACE_MUTABLE);

  reach_type_cache_serialise_trace(ctx, &t->subtypes);

  if(t->fields != NULL)
  {
    pony_serialise_reserve(ctx, t->fields,
      t->field_count * sizeof(reach_field_t));

    for(size_t i = 0; i < t->field_count; i++)
      reach_field_serialise_trace(ctx, &t->fields[i]);
  }
}

static void reach_type_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  reach_type_t* t = (reach_type_t*)object;
  reach_type_t* dst = (reach_type_t*)((uintptr_t)buf + offset);

  dst->name = (const char*)pony_serialise_offset(ctx, (char*)t->name);
  dst->mangle = (const char*)pony_serialise_offset(ctx, (char*)t->mangle);
  dst->ast = (ast_t*)pony_serialise_offset(ctx, t->ast);
  dst->ast_cap = (ast_t*)pony_serialise_offset(ctx, t->ast_cap);
  dst->underlying = t->underlying;

  reach_method_names_serialise(ctx, &t->methods, buf,
    offset + offsetof(reach_type_t, methods), PONY_TRACE_MUTABLE);
  dst->bare_method = (reach_method_t*)pony_serialise_offset(ctx, t->bare_method);
  reach_type_cache_serialise(ctx, &t->subtypes, buf,
    offset + offsetof(reach_type_t, subtypes), PONY_TRACE_MUTABLE);
  dst->type_id = t->type_id;
  dst->vtable_size = t->vtable_size;
  dst->can_be_boxed = t->can_be_boxed;
  dst->is_trait = t->is_trait;

  dst->field_count = t->field_count;
  dst->fields = (reach_field_t*)pony_serialise_offset(ctx, t->fields);

  if(t->fields != NULL)
  {
    size_t field_offset = (size_t)dst->fields;

    for(size_t i = 0; i < t->field_count; i++)
    {
      reach_field_serialise(ctx, &t->fields[i], buf, field_offset,
        PONY_TRACE_MUTABLE);
      field_offset += sizeof(reach_field_t);
    }
  }

  dst->c_type = NULL;
}

static void reach_type_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_type_t* t = (reach_type_t*)object;

  t->name = string_deserialise_offset(ctx, (uintptr_t)t->name);
  t->mangle = string_deserialise_offset(ctx, (uintptr_t)t->mangle);
  t->ast = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)t->ast);
  t->ast_cap = (ast_t*)pony_deserialise_offset(ctx, ast_pony_type(),
    (uintptr_t)t->ast_cap);

  reach_method_names_deserialise(ctx, &t->methods);
  t->bare_method = (reach_method_t*)pony_deserialise_offset(ctx,
    reach_method_pony_type(), (uintptr_t)t->bare_method);
  reach_type_cache_deserialise(ctx, &t->subtypes);

  if(t->field_count > 0)
  {
    t->fields = (reach_field_t*)pony_deserialise_block(ctx, (uintptr_t)t->fields,
      t->field_count * sizeof(reach_field_t));

    for(size_t i = 0; i < t->field_count; i++)
      reach_field_deserialise(ctx, &t->fields[i]);
  } else {
    t->fields = NULL;
  }
}

static pony_type_t reach_type_pony =
{
  0,
  sizeof(reach_type_t),
  0,
  0,
  NULL,
  NULL,
  reach_type_serialise_trace,
  reach_type_serialise,
  reach_type_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_type_pony_type()
{
  return &reach_type_pony;
}

static void reach_serialise_trace(pony_ctx_t* ctx, void* object)
{
  reach_t* r = (reach_t*)object;

  reach_types_serialise_trace(ctx, &r->types);
}

static void reach_serialise(pony_ctx_t* ctx, void* object, void* buf,
  size_t offset, int mutability)
{
  (void)mutability;

  reach_t* r = (reach_t*)object;
  reach_t* dst = (reach_t*)((uintptr_t)buf + offset);

  reach_types_serialise(ctx, &r->types, buf, offset + offsetof(reach_t, types),
    PONY_TRACE_MUTABLE);
  dst->method_stack = NULL;
  dst->object_type_count = r->object_type_count;
  dst->numeric_type_count = r->numeric_type_count;
  dst->tuple_type_count = r->tuple_type_count;
  dst->total_type_count = r->total_type_count;
  dst->trait_type_count = r->trait_type_count;
}

static void reach_deserialise(pony_ctx_t* ctx, void* object)
{
  reach_t* r = (reach_t*)object;

  reach_types_deserialise(ctx, &r->types);
}

static pony_type_t reach_pony =
{
  0,
  sizeof(reach_t),
  0,
  0,
  NULL,
  NULL,
  reach_serialise_trace,
  reach_serialise,
  reach_deserialise,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  NULL,
  NULL,
  NULL
};

pony_type_t* reach_pony_type()
{
  return &reach_pony;
}
