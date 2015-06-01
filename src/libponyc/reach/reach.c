#include "reach.h"
#include "../codegen/genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/reify.h"
#include "../type/subtype.h"
#include "../../libponyrt/ds/stack.h"
#include "../../libponyrt/mem/pool.h"
#include <stdio.h>
#include <assert.h>

DECLARE_STACK(reachable_method_stack, reachable_method_t);
DEFINE_STACK(reachable_method_stack, reachable_method_t);

static reachable_method_stack_t* add_type(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs);

static reachable_method_stack_t* reachable_method(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs);

static reachable_method_stack_t* reachable_expr(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* ast);

static uint64_t reachable_method_hash(reachable_method_t* m)
{
  return hash_ptr(m->name);
}

static bool reachable_method_cmp(reachable_method_t* a, reachable_method_t* b)
{
  return a->name == b->name;
}

static void reachable_method_free(reachable_method_t* m)
{
  ast_free(m->typeargs);
  ast_free(m->r_fun);
  POOL_FREE(reachable_method_t, m);
}

DEFINE_HASHMAP(reachable_methods, reachable_method_t, reachable_method_hash,
  reachable_method_cmp, pool_alloc_size, pool_free_size, reachable_method_free
  );

static uint64_t reachable_method_name_hash(reachable_method_name_t* m)
{
  return hash_ptr(m->name);
}

static bool reachable_method_name_cmp(reachable_method_name_t* a,
  reachable_method_name_t* b)
{
  return a->name == b->name;
}

static void reachable_method_name_free(reachable_method_name_t* m)
{
  reachable_methods_destroy(&m->r_methods);
  POOL_FREE(reachable_method_name_t, m);
}

DEFINE_HASHMAP(reachable_method_names, reachable_method_name_t,
  reachable_method_name_hash, reachable_method_name_cmp, pool_alloc_size,
  pool_free_size, reachable_method_name_free);

static uint64_t reachable_type_hash(reachable_type_t* t)
{
  return hash_ptr(t->name);
}

static bool reachable_type_cmp(reachable_type_t* a, reachable_type_t* b)
{
  return a->name == b->name;
}

static void reachable_type_free(reachable_type_t* t)
{
  ast_free(t->type);
  reachable_method_names_destroy(&t->methods);
  reachable_type_cache_destroy(&t->subtypes);
  POOL_FREE(reachable_type_t, t);
}

DEFINE_HASHMAP(reachable_types, reachable_type_t, reachable_type_hash,
  reachable_type_cmp, pool_alloc_size, pool_free_size, reachable_type_free);

DEFINE_HASHMAP(reachable_type_cache, reachable_type_t, reachable_type_hash,
  reachable_type_cmp, pool_alloc_size, pool_free_size, NULL);

static reachable_method_stack_t* add_rmethod(reachable_method_stack_t* s,
  reachable_type_t* t, reachable_method_name_t* n, ast_t* typeargs)
{
  const char* name = genname_fun(NULL, n->name, typeargs);
  reachable_method_t* m = reach_method(n, name);

  if(m == NULL)
  {
    m = POOL_ALLOC(reachable_method_t);
    m->name = name;
    m->typeargs = ast_dup(typeargs);
    m->vtable_index = (uint32_t)-1;

    ast_t* fun = lookup(NULL, NULL, t->type, n->name);

    if(typeargs != NULL)
    {
      // Reify the method with its typeargs, if it has any.
      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error,
        body);

      ast_t* r_fun = reify(fun, fun, typeparams, typeargs);
      ast_free_unattached(fun);
      fun = r_fun;
    }

    m->r_fun = ast_dup(fun);
    ast_free_unattached(fun);

    reachable_methods_put(&n->r_methods, m);

    // Put on a stack of reachable methods to trace.
    s = reachable_method_stack_push(s, m);
  }

  return s;
}

static reachable_method_stack_t* add_method(reachable_method_stack_t* s,
  reachable_type_t* t, const char* name, ast_t* typeargs)
{
  reachable_method_name_t* n = reach_method_name(t, name);

  if(n == NULL)
  {
    n = POOL_ALLOC(reachable_method_name_t);
    n->name = name;
    reachable_methods_init(&n->r_methods, 0);
    reachable_method_names_put(&t->methods, n);
  }

  s = add_rmethod(s, t, n, typeargs);

  // Add to subtypes if we're an interface or trait.
  ast_t* def = (ast_t*)ast_data(t->type);

  switch(ast_id(def))
  {
    case TK_INTERFACE:
    case TK_TRAIT:
    {
      size_t i = HASHMAP_BEGIN;
      reachable_type_t* t2;

      while((t2 = reachable_type_cache_next(&t->subtypes, &i)) != NULL)
        s = add_method(s, t2, name, typeargs);
    }

    default: {}
  }

  return s;
}

static reachable_method_stack_t* add_methods_to_type(
  reachable_method_stack_t* s, reachable_type_t* from, reachable_type_t* to)
{
  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;

  while((n = reachable_method_names_next(&from->methods, &i)) != NULL)
  {
    size_t j = HASHMAP_BEGIN;
    reachable_method_t* m;

    while((m = reachable_methods_next(&n->r_methods, &j)) != NULL)
      s = add_method(s, to, n->name, m->typeargs);
  }

  return s;
}

static reachable_method_stack_t* add_types_to_trait(
  reachable_method_stack_t* s, reachable_types_t* r, reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t2;

  while((t2 = reachable_types_next(r, &i)) != NULL)
  {
    ast_t* def = (ast_t*)ast_data(t2->type);

    switch(ast_id(def))
    {
      case TK_PRIMITIVE:
      case TK_CLASS:
      case TK_ACTOR:
        if(is_subtype(t2->type, t->type))
        {
          reachable_type_cache_put(&t->subtypes, t2);
          reachable_type_cache_put(&t2->subtypes, t);
          s = add_methods_to_type(s, t, t2);
        }
        break;

      default: {}
    }
  }

  return s;
}

static reachable_method_stack_t* add_traits_to_type(
  reachable_method_stack_t* s, reachable_types_t* r, reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t2;

  while((t2 = reachable_types_next(r, &i)) != NULL)
  {
    ast_t* def = (ast_t*)ast_data(t2->type);

    switch(ast_id(def))
    {
      case TK_INTERFACE:
      case TK_TRAIT:
        if(is_subtype(t->type, t2->type))
        {
          reachable_type_cache_put(&t->subtypes, t2);
          reachable_type_cache_put(&t2->subtypes, t);
          s = add_methods_to_type(s, t2, t);
        }
        break;

      default: {}
    }
  }

  return s;
}

static reachable_method_stack_t* add_fields(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* type)
{
  ast_t* def = (ast_t*)ast_data(type);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        const char* name = ast_name(ast_child(member));
        ast_t* r_member = lookup(NULL, NULL, type, name);
        assert(r_member != NULL);

        AST_GET_CHILDREN(r_member, id, ftype, body);
        s = add_type(s, r, ftype, NULL, NULL);

        if(ast_id(body) != TK_NONE)
          s = reachable_expr(s, r, body);

        if(r_member != member)
          ast_free_unattached(r_member);
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return s;
}

static reachable_method_stack_t* add_special(reachable_method_stack_t* s,
  reachable_type_t* t, ast_t* type, const char* special)
{
  special = stringtab(special);
  ast_t* find = lookup_try(NULL, NULL, type, special);

  if(find != NULL)
  {
    s = add_method(s, t, special, NULL);
    ast_free_unattached(find);
  }

  return s;
}

static reachable_method_stack_t* add_type(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs)
{
  switch(ast_id(type))
  {
    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
    {
      assert(name == NULL);
      assert(typeargs == NULL);
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        s = add_type(s, r, child, NULL, NULL);
        child = ast_sibling(child);
      }

      return s;
    }

    case TK_NOMINAL:
      break;

    default:
      assert(0);
      return s;
  }

  const char* type_name = genname_type(type);
  reachable_type_t* t = reach_type(r, type_name);

  if(t == NULL)
  {
    t = POOL_ALLOC(reachable_type_t);
    t->name = type_name;
    t->type = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
    reachable_method_names_init(&t->methods, 0);
    reachable_type_cache_init(&t->subtypes, 0);
    t->vtable_size = 0;
    reachable_types_put(r, t);

    AST_GET_CHILDREN(type, pkg, id, typeparams);
    ast_t* typeparam = ast_child(typeparams);

    while(typeparam != NULL)
    {
      s = add_type(s, r, typeparam, NULL, NULL);
      typeparam = ast_sibling(typeparam);
    }

    ast_t* def = (ast_t*)ast_data(type);

    switch(ast_id(def))
    {
      case TK_INTERFACE:
      case TK_TRAIT:
        s = add_types_to_trait(s, r, t);
        break;

      case TK_PRIMITIVE:
        s = add_fields(s, r, type);
        s = add_traits_to_type(s, r, t);
        s = add_special(s, t, type, "_init");
        s = add_special(s, t, type, "_final");
        break;

      case TK_CLASS:
        s = add_fields(s, r, type);
        s = add_traits_to_type(s, r, t);
        s = add_special(s, t, type, "_final");
        break;

      case TK_ACTOR:
        s = add_fields(s, r, type);
        s = add_traits_to_type(s, r, t);
        s = add_special(s, t, type, "_event_notify");
        s = add_special(s, t, type, "_final");
        break;

      default: {}
    }
  }

  if(name != NULL)
    s = add_method(s, t, name, typeargs);

  return s;
}

static reachable_method_stack_t* reachable_pattern(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_DONTCARE:
    case TK_NONE:
      break;

    case TK_VAR:
    case TK_LET:
    {
      AST_GET_CHILDREN(ast, idseq, type);
      s = add_type(s, r, type, NULL, NULL);
      break;
    }

    case TK_TUPLE:
    case TK_SEQ:
    {
      ast_t* child = ast_child(ast);

      while(child != NULL)
      {
        s = reachable_pattern(s, r, child);
        child = ast_sibling(child);
      }
      break;
    }

    default:
    {
      s = reachable_method(s, r, ast_type(ast), stringtab("eq"), NULL);
      s = reachable_expr(s, r, ast);
      break;
    }
  }

  return s;
}

static reachable_method_stack_t* reachable_call(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_NEWBEREF:
    case TK_BEREF:
    case TK_FUNREF:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);

  return reachable_method(s, r, type, method_name, typeargs);
}

static reachable_method_stack_t* reachable_ffi(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* ast)
{
  AST_GET_CHILDREN(ast, name, return_typeargs, args, namedargs, question);
  ast_t* decl = ast_get(ast, ast_name(name), NULL);

  if(decl != NULL)
  {
    AST_GET_CHILDREN(decl, decl_name, decl_ret_typeargs, params, named_params,
      decl_error);

    return_typeargs = decl_ret_typeargs;
  }

  ast_t* return_type = ast_child(return_typeargs);
  return add_type(s, r, return_type, NULL, NULL);
}

static reachable_method_stack_t* reachable_expr(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* ast)
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

      if(type != NULL)
        s = reachable_method(s, r, type, stringtab("create"), NULL);
      break;
    }

    case TK_CASE:
    {
      AST_GET_CHILDREN(ast, pattern, guard, body);
      s = reachable_pattern(s, r, pattern);
      s = reachable_expr(s, r, guard);
      return reachable_expr(s, r, body);
    }

    case TK_CALL:
      s = reachable_call(s, r, ast);
      break;

    case TK_FFICALL:
      s = reachable_ffi(s, r, ast);
      break;

    default: {}
  }

  // Traverse all child expressions looking for calls.
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    s = reachable_expr(s, r, child);
    child = ast_sibling(child);
  }

  return s;
}

static reachable_method_stack_t* reachable_body(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* fun)
{
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error, body);
  return reachable_expr(s, r, body);
}

static reachable_method_stack_t* reachable_method(reachable_method_stack_t* s,
  reachable_types_t* r, ast_t* type, const char* name, ast_t* typeargs)
{
  switch(ast_id(type))
  {
    case TK_NOMINAL:
      break;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    {
      ast_t* child = ast_child(type);

      while(child != NULL)
      {
        ast_t* find = lookup_try(NULL, NULL, child, name);

        if(find != NULL)
          s = reachable_method(s, r, child, name, typeargs);

        child = ast_sibling(child);
      }

      return s;
    }

    default:
      assert(0);
      return NULL;
  }

  return add_type(s, r, type, name, typeargs);
}

static void handle_stack(reachable_method_stack_t* s, reachable_types_t* r)
{
  while(s != NULL)
  {
    reachable_method_t* m;
    s = reachable_method_stack_pop(s, &m);
    s = reachable_body(s, r, m->r_fun);
  }
}

reachable_types_t* reach_new()
{
  reachable_types_t* r = POOL_ALLOC(reachable_types_t);
  reachable_types_init(r, 64);
  return r;
}

void reach_primitives(reachable_types_t* r, pass_opt_t* opt, ast_t* from)
{
  reachable_method_stack_t* s = NULL;

  s = add_type(s, r, type_builtin(opt, from, "Bool"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "I8"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "I16"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "I32"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "I64"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "I128"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "U8"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "U16"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "U32"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "U64"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "U128"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "F32"), NULL, NULL);
  s = add_type(s, r, type_builtin(opt, from, "F64"), NULL, NULL);

  handle_stack(s, r);
}

void reach_free(reachable_types_t* r)
{
  if(r == NULL)
    return;

  reachable_types_destroy(r);
  POOL_FREE(reachable_types_t, r);
}

void reach(reachable_types_t* r, ast_t* type, const char* name,
  ast_t* typeargs)
{
  reachable_method_stack_t* s = reachable_method(NULL, r, type, name,
    typeargs);

  handle_stack(s, r);
}

reachable_type_t* reach_type(reachable_types_t* r, const char* name)
{
  reachable_type_t k;
  k.name = name;
  return reachable_types_get(r, &k);
}

reachable_method_name_t* reach_method_name(reachable_type_t* t,
  const char* name)
{
  reachable_method_name_t k;
  k.name = name;
  return reachable_method_names_get(&t->methods, &k);
}

reachable_method_t* reach_method(reachable_method_name_t* n,
  const char* name)
{
  reachable_method_t k;
  k.name = name;
  return reachable_methods_get(&n->r_methods, &k);
}

size_t reach_method_count(reachable_type_t* t)
{
  size_t i = HASHMAP_BEGIN;
  reachable_method_name_t* n;
  size_t count = 0;

  while((n = reachable_method_names_next(&t->methods, &i)) != NULL)
    count += reachable_methods_size(&n->r_methods);

  return count;
}

void reach_dump(reachable_types_t* r)
{
  printf("REACH\n");

  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(r, &i)) != NULL)
  {
    printf("  %s vtable size %d\n", t->name, t->vtable_size);
    size_t j = HASHMAP_BEGIN;
    reachable_method_name_t* m;

    while((m = reachable_method_names_next(&t->methods, &j)) != NULL)
    {
      size_t k = HASHMAP_BEGIN;
      reachable_method_t* p;

      while((p = reachable_methods_next(&m->r_methods, &k)) != NULL)
      {
        printf("    %s vtable index %d (%p)\n", p->name, p->vtable_index, p);
      }
    }
  }
}
