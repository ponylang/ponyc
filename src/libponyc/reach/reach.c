#include "reach.h"
#include "../codegen/genname.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../type/reify.h"
#include "../../libponyrt/mem/pool.h"
#include <assert.h>

static uint64_t reified_method_hash(reified_method_t* m)
{
  return hash_ptr(m->name);
}

static bool reified_method_cmp(reified_method_t* a, reified_method_t* b)
{
  return a->name == b->name;
}

static void reified_method_free(reified_method_t* m)
{
  ast_free(m->typeargs);
  POOL_FREE(reified_method_t, m);
}

DEFINE_HASHMAP(reified_methods, reified_method_t, reified_method_hash,
  reified_method_cmp, pool_alloc_size, pool_free_size, reified_method_free,
  NULL);

static uint64_t method_hash(method_t* m)
{
  return hash_ptr(m->name);
}

static bool method_cmp(method_t* a, method_t* b)
{
  return a->name == b->name;
}

static void method_free(method_t* m)
{
  reified_methods_destroy(&m->r_methods);
  POOL_FREE(method_t, m);
}

DEFINE_HASHMAP(methods, method_t, method_hash, method_cmp, pool_alloc_size,
  pool_free_size, method_free, NULL);

static uint64_t reified_type_hash(reified_type_t* t)
{
  return hash_ptr(t->name);
}

static bool reified_type_cmp(reified_type_t* a, reified_type_t* b)
{
  return a->name == b->name;
}

static void reified_type_free(reified_type_t* t)
{
  ast_free(t->type);
  methods_destroy(&t->methods);
  POOL_FREE(reified_type_t, t);
}

DEFINE_HASHMAP(reified_types, reified_type_t, reified_type_hash,
  reified_type_cmp, pool_alloc_size, pool_free_size, reified_type_free, NULL);

static reified_method_t* find_rmethod(method_t* r, const char* name,
  ast_t* typeargs)
{
  reified_method_t m1 = {name, NULL, NULL};
  reified_method_t* m2 = reified_methods_get(&r->r_methods, &m1);

  if(m2 == NULL)
  {
    m2 = POOL_ALLOC(reified_method_t);
    m2->name = name;
    m2->typeargs = ast_dup(typeargs);
    m2->r_fun = NULL;
    reified_methods_put(&r->r_methods, m2);
  }

  return m2;
}

static method_t* find_method(reified_type_t* r, const char* name)
{
  method_t m1 = {name, {HASH_INIT}};
  method_t* m2 = methods_get(&r->methods, &m1);

  if(m2 == NULL)
  {
    m2 = POOL_ALLOC(method_t);
    m2->name = name;
    reified_methods_init(&m2->r_methods, 0);
    methods_put(&r->methods, m2);
  }

  return m2;
}

static reified_type_t* find_type(reified_types_t* r, const char* name,
  ast_t* type)
{
  reified_type_t t1 = {name, NULL, {HASH_INIT}};
  reified_type_t* t2 = reified_types_get(r, &t1);

  if(t2 == NULL)
  {
    t2 = POOL_ALLOC(reified_type_t);
    t2->name = name;
    t2->type = ast_dup(type);
    methods_init(&t2->methods, 0);
    reified_types_put(r, t2);
  }

  return t2;
}

static void reach_call(reified_types_t* r, ast_t* ast)
{
  AST_GET_CHILDREN(ast, positional, named, postfix);
  AST_GET_CHILDREN(postfix, receiver, method);
  ast_t* typeargs = NULL;

  // Dig through function qualification.
  switch(ast_id(receiver))
  {
    case TK_NEWREF:
    case TK_BEREF:
    case TK_FUNREF:
      typeargs = method;
      AST_GET_CHILDREN_NO_DECL(receiver, receiver, method);
      break;

    default: {}
  }

  ast_t* type = ast_type(receiver);
  const char* method_name = ast_name(method);
  reach_method(r, type, method_name, typeargs);
}

static void reach_expr(reified_types_t* r, ast_t* ast)
{
  // If this is a method call, mark the method as reachable.
  if(ast_id(ast) == TK_CALL)
    reach_call(r, ast);

  // TODO: look here for pattern matching on type when looking for interfaces
  // could build a set of fully reified interfaces that get pattern matched
  // later, we could go through all our reified types, and if they are that
  // inteface, we can add the interface to the provides list

  // Traverse all child expressions looking for calls.
  ast_t* child = ast_child(ast);

  while(child != NULL)
    reach_expr(r, child);
}

static void reach_body(reified_types_t* r, ast_t* fun)
{
  AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error, body);
  reach_expr(r, body);
}

void reach_method(reified_types_t* r, ast_t* type, const char* name,
  ast_t* typeargs)
{
  // TODO: what if `type` is an isect type?

  // Get a type.
  const char* type_name = genname_type(type);
  ast_t* this_type = set_cap_and_ephemeral(type, TK_REF, TK_NONE);
  reified_type_t* rtype = find_type(r, type_name, this_type);

  // Get a method.
  method_t* method = find_method(rtype, name);

  // Get a reified method.
  const char* method_name = genname_fun(type_name, name, typeargs);
  reified_method_t* r_method = find_rmethod(method, method_name, typeargs);

  // Trace if this reified method on this reified type has never been traced.
  if(r_method->r_fun == NULL)
  {
    ast_t* fun = lookup(NULL, NULL, this_type, name);

    if(typeargs != NULL)
    {
      // Reify the method with its typeargs, if it has any.
      AST_GET_CHILDREN(fun, cap, id, typeparams, params, result, can_error,
        body);

      ast_t* r_fun = reify(fun, typeparams, typeargs);
      ast_free_unattached(fun);
      fun = r_fun;
    }

    r_method->r_fun = ast_dup(fun);
    ast_free_unattached(fun);
    reach_body(r, r_method->r_fun);
  }

  ast_free_unattached(this_type);
}
