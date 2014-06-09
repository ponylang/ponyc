#include "types.h"
#include "cap.h"
#include "function.h"
#include "../ds/hash.h"
#include "../ds/stringtab.h"
#include "../ds/table.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef enum
{
  T_NOMINAL,
  T_STRUCTURAL,
  T_UNION,
  T_ISECT,
  T_TUPLE
} type_id;

static bool type_cmp(type_t* a, type_t* b);
static type_t* type_dup(type_t* data);
static void type_free(type_t* t);

DEFINE_LIST(typelist, type_t, type_hash, type_cmp, NULL);
DEFINE_TABLE(typetab, type_t, type_hash, type_cmp, type_dup, type_free);

typedef struct nominal_t
{
  ast_t* ast;
  typelist_t* type_params;
  typelist_t* constraints;
  typelist_t* traits;
  funlist_t* functions;
  // TODO: field_t, fieldlist_t
} nominal_t;

typedef struct structural_t
{
  funlist_t* functions;
} structural_t;

typedef struct typeexpr_t
{
  type_t* left;
  type_t* right;
} typeexpr_t;

struct type_t
{
  type_id id;
  cap_id cap;
  bool ephemeral;

  union
  {
    structural_t s;
    nominal_t n;
    typeexpr_t e;
    type_t* d;
  };
};

static typetab_t* type_table;

static bool nominal_eq(type_t* a, type_t* b)
{
  return
    // same name
    (a->n.ast == b->n.ast) &&
    // invariant type params
    typelist_equals(a->n.type_params, b->n.type_params);
}

static bool structural_eq(type_t* a, type_t* b)
{
  return funlist_subset(a->s.functions, b->s.functions) &&
    funlist_subset(b->s.functions, a->s.functions);
}

static bool type_cmp(type_t* a, type_t* b)
{
  // treats a NULL type as matching anything
  if(a == b)
    return true;

  // do a deep comparison here
  // then, in type_eq, we can compare solely by identity
  if(a->id != b->id)
    return false;

  switch(a->id)
  {
    case T_NOMINAL:
      return nominal_eq(a, b);

    case T_STRUCTURAL:
      return structural_eq(a, b);

    case T_UNION:
    case T_ISECT:
      return type_sub(a, b) && type_sub(b, a);

    case T_TUPLE:
      return type_eq(a->e.left, b->e.left) &&
        type_eq(a->e.right, b->e.right);
  }

  return false;
}

static type_t* type_dup(type_t* data)
{
  return data;
}

static void type_free(type_t* t)
{
  if(t == NULL)
    return;

  // don't free any other types, as they are in the table
  switch(t->id)
  {
    case T_NOMINAL:
      typelist_free(t->n.type_params);
      typelist_free(t->n.constraints);
      typelist_free(t->n.traits);
      funlist_free(t->n.functions);
      break;

    case T_STRUCTURAL:
      funlist_free(t->s.functions);
      break;

    case T_UNION:
    case T_ISECT:
    case T_TUPLE:
      break;
  }

  free(t);
}

static type_t* type_new(type_id id)
{
  type_t* type = calloc(1, sizeof(type_t));
  type->id = id;
  return type;
}

static type_t* type_store(type_t* type)
{
  if(type_table == NULL)
    type_table = typetab_create(4096);

  bool present;
  type_t* found = typetab_insert(type_table, type, &present);

  if(present)
    type_free(type);

  return found;
}

#if 0
static list_t* typelist(ast_t* ast)
{
  list_t* list = NULL;
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    type_t* type = type_ast(child);

    if(type == NULL)
    {
      list_free(list, NULL);
      return POINTER_ERROR;
    }

    list = list_append(list, type);
    child = ast_sibling(child);
  }

  return list;
}

static type_t* expand_alias(type_t* type)
{
  if(ast_id(type->obj.ast) != TK_ALIAS) { return type; }

  type_t* alias = type_ast(ast_childidx(type->obj.ast, 2));

  return type_subst(alias, type->obj.params);
}

static type_t* objtype(ast_t* ast)
{
  ast_t* scope = ast_child(ast);
  ast_t* class = ast_sibling(scope);
  ast_t* param = ast_sibling(class);

  if(ast_id(class) == TK_NONE)
  {
    class = scope;
    scope = ast;
  } else {
    ast_t* p = ast_get(ast_nearest(ast, TK_MODULE), ast_name(scope));

    if(p == NULL)
    {
      ast_error(scope, "no package '%s' in scope", ast_name(scope));
      return NULL;
    }

    scope = p;
  }

  const char* name = ast_name(class);
  type_t* type = nametype(scope, name);

  if((type == &infer) && (ast_child(param) != NULL))
  {
    ast_error(class, "type _ cannot have type arguments");
    return NULL;
  }

  if(type == NULL)
  {
    ast_error(class, "no type '%s' in scope", name);
    return NULL;
  }

  list_t* list = typelist(param);

  if(list == POINTER_ERROR)
  {
    type_free(type);
    return NULL;
  }

  type->obj.params = list;

  // TODO: get the mode with the viewpoint

  return expand_alias(type);
}

static void* subst_map(void* map, void* iter)
{
  return type_subst(list_data(iter), map);
}

static type_t* type_subst(type_t* type, list_t* list)
{
  type_t* subst;

  switch(type->id)
  {
    case T_INFER: return type;

    case T_FUNCTION:
      subst = type_new(T_FUNCTION);
      subst->fun.params = list_map(type->fun.params, subst_map, list);
      subst->fun.result = type_subst(type->fun.result, list);
      subst->fun.throws = type->fun.throws;
      break;

    case T_OBJECT:
      switch(ast_id(type->obj.ast))
      {
        case TK_ALIAS:
        case TK_TRAIT:
        case TK_CLASS:
        case TK_ACTOR:
          subst = type_new(T_OBJECT);
          subst->obj.ast = type->obj.ast;
          subst->obj.params = list_map(type->obj.params, subst_map, list);
          break;

        case TK_TYPEPARAM:
          subst = list_data(list_index(list, ast_index(type->obj.ast)));
          break;

        default: return NULL;
      }
      break;

    case T_ADT:
      subst = type_new(T_ADT);
      subst->adt.types = list_map(type->adt.types, subst_map, list);
      break;

    default: subst = NULL;
  }

  return type_store(subst);
}

static bool arg_valid(ast_t* ast, type_t* type, type_t* arg, ast_t* param)
{
  if(arg->id == T_INFER) { return true; }

  if((arg->id == T_OBJECT)
    && (ast_id(arg->obj.ast) == TK_TYPEPARAM)
    )
  {
    // if the argument is the parameter, it matches
    if(arg->obj.ast == param) { return true; }
    arg = ast_data(arg->obj.ast);
  }

  // substitute the parameter list in the bounds
  type_t* subst = type_subst(ast_data(param), type->obj.params);

  // check that the argument is a subtype of the resulting type
  if(!type_sub(arg, subst))
  {
    ast_error(ast, "type argument %zd isn't within the constraint",
      ast_index(param) + 1);
    return false;
  }

  return true;
}

static bool obj_valid(ast_t* ast, type_t* type)
{
  switch(ast_id(type->obj.ast))
  {
    case TK_ALIAS:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
    {
      ast_t* params = ast_childidx(type->obj.ast, 1);
      int count_arg = list_length(type->obj.params);
      int count_param = ast_childcount(params);

      if(count_arg != count_param)
      {
        ast_error(ast, "type has %d type argument%s, expected %d",
          count_arg, (count_arg == 1) ? "" : "s", count_param);
        return false;
      }

      if(count_arg == 0) return true;

      list_t* arg = type->obj.params;
      ast_t* param = ast_child(params);

      while(arg != NULL)
      {
        if(!arg_valid(ast, type, list_data(arg), param)) return false;
        arg = list_next(arg);
        param = ast_sibling(param);
      }

      return true;
    }

    case TK_TYPEPARAM:
    {
      if(type->obj.params != NULL)
      {
        ast_error(ast, "type parameter can't have type arguments");
        return false;
      }

      return true;
    }

    default: {}
  }

  return false;
}
#endif

static bool structural_sub(type_t* a, type_t* b)
{
  return funlist_sub(a->s.functions, b->s.functions);
}

static bool nominal_sub(type_t* a, type_t* b)
{
  return
    // A is a subtype if it's the same type as B
    (a == b) ||
    // A is a subtype if any of its traits are a subtype of B
    typelist_any(a->n.traits, (typelist_pred_fn)nominal_sub, b);
}

static bool nominal_in_structural(type_t* a, type_t* b)
{
  return funlist_sub(a->s.functions, b->n.functions);
}

static type_t* nominal_type(ast_t* ast)
{
  // TODO:
  return NULL;
}

static type_t* structural_type(ast_t* ast)
{
  // TODO:
  return NULL;
}

static type_t* expr_type(ast_t* ast)
{
  type_id id;

  switch(ast_id(ast))
  {
    case TK_UNIONTYPE: id = T_UNION; break;
    case TK_ISECTTYPE: id = T_ISECT; break;
    case TK_TUPLETYPE: id = T_TUPLE; break;

    default:
      assert(0);
      return NULL;
  }

  ast_t* left = ast_child(ast);
  ast_t* right = ast_sibling(left);

  type_t* type = type_new(id);
  type->e.left = type_create(left);
  type->e.right = type_create(right);

  return type;
}

bool typelist_sub(typelist_t* a, typelist_t* b)
{
  return list_equals((list_t*)a, (list_t*)b, (cmp_fn)type_sub);
}

bool typelist_constraints(typelist_t* typeparams, typelist_t* constraints,
  typelist_t* typeargs)
{
  // reify our constraints before checking them.
  // Foo[A: List[B], B: Number]
  // qualifying with [List[U32], I32] should fail
  //   reified constraints [List[I32], I32]
  //   typeargs fail on subtyping
  // qualifying with [List[I32], I32] should succeed
  //   reified constraints [List[I32], I32]
  //   works
  // Foo[A: List[B], B: Map[A, B]]
  // qualifying with [List[Map[I32, String]], Map[I32, String]
  //   reified constraints
  //   List[Map[I32, String]], Map[List[Map[I32, String]], Map[I32, String]]
  //   typeargs fail on subtyping
  // Ordered[A: Ordered[A]]
  // qualify with String is Ordered[String]
  //   reified constraints [Ordered[String]]
  //   works: String <: Ordered[String]
  if(typelist_length(typeargs) != typelist_length(typeparams))
    return false;

  // typeparams and constraints should be the same length
  assert(typelist_length(typeparams) == typelist_length(constraints));

  // reify our constraints using our type arguments
  typelist_t* rcon = typelist_reify(constraints, typeparams, typeargs);

  // we should get the same number of reified constraints
  assert(typelist_length(rcon) == typelist_length(constraints));

  // check our type arguments against our reified constraints
  bool r = typelist_sub(typeargs, rcon);
  typelist_free(rcon);

  return r;
}

typelist_t* typelist_fbounds(typelist_t* typeparams, typelist_t* constraints)
{
  // TODO:
  // fun tag min[A: Seq[B], B: Ordered[B]](list: A, item: B): B
  // typeparams = [A, B]
  // constraints = [Seq[B], Ordered[B]]
  // first, replace self-reference with Any:
  // f_constraints = [Seq[B], Ordered[Any]]
  // reify f_constraints using the typeparams and itself to get the f_bounds
  // f_bounds = [Seq[Ordered[Any]], Ordered[Any]]
  return NULL;
}

typedef struct typelist_pair_t
{
  typelist_t* formal;
  typelist_t* actual;
} typelist_pair_t;

static type_t* reify_map(type_t* type, void* arg)
{
  typelist_pair_t* pair = arg;
  int index = typelist_findindex(pair->formal, type);

  if(index != -1)
    return typelist_data(typelist_index(pair->actual, index));

  return type_reify(type, pair->formal, pair->actual);
}

typelist_t* typelist_reify(typelist_t* list,
  typelist_t* formal, typelist_t* actual)
{
  typelist_pair_t pair = {formal, actual};
  return typelist_map(list, reify_map, &pair);
}

type_t* type_name(ast_t* scope, const char* name, cap_id cap)
{
  // TODO:
  return NULL;
}

type_t* type_create(ast_t* ast)
{
  if(ast == NULL)
  {
    assert(0);
    return NULL;
  }

  type_t* type;

  switch(ast_id(ast))
  {
    case TK_TYPE:
    case TK_TRAIT:
    case TK_ACTOR:
    case TK_CLASS:
      // TODO:
      type = NULL;
      break;

    case TK_NOMINAL:
      type = nominal_type(ast);
      break;

    case TK_STRUCTURAL:
      type = structural_type(ast);
      break;

    case TK_UNIONTYPE:
    case TK_ISECTTYPE:
    case TK_TUPLETYPE:
      type = expr_type(ast);
      break;

    default:
      assert(0);
      return NULL;
  }

  return type_store(type);
}

bool type_eq(type_t* a, type_t* b)
{
  // treats a NULL type as matching anything
  return (a == NULL) || (b == NULL) || (a == b);
}

bool type_sub(type_t* a, type_t* b)
{
  // treats a NULL type as matching anything
  if((a == NULL) || (b == NULL) || (a == b))
    return true;

  switch(a->id)
  {
    case T_NOMINAL:
      switch(b->id)
      {
        case T_NOMINAL:
          return nominal_sub(a, b);

        case T_STRUCTURAL:
          return nominal_in_structural(a, b);

        case T_UNION:
          return type_sub(a, b->e.left) || type_sub(a, b->e.right);

        case T_ISECT:
          return type_sub(a, b->e.left) && type_sub(a, b->e.right);

        case T_TUPLE:
          return false;
      }
      break;

    case T_STRUCTURAL:
      switch(b->id)
      {
        case T_STRUCTURAL:
          return structural_sub(a, b);

        case T_UNION:
          return type_sub(a, b->e.left) || type_sub(a, b->e.right);

        case T_ISECT:
          return type_sub(a, b->e.left) && type_sub(a, b->e.right);

        case T_NOMINAL:
        case T_TUPLE:
          return false;
      }
      break;

    case T_UNION:
      return type_sub(a->e.left, b) && type_sub(a->e.right, b);

    case T_ISECT:
      return type_sub(a->e.left, b) || type_sub(a->e.right, b);

    case T_TUPLE:
      switch(b->id)
      {
        case T_UNION:
          return type_sub(a, b->e.left) || type_sub(a, b->e.right);

        case T_ISECT:
          return type_sub(a, b->e.left) && type_sub(a, b->e.right);

        case T_TUPLE:
          return type_sub(a->e.left, b->e.left) &&
            type_sub(a->e.right, b->e.right);

        case T_NOMINAL:
        case T_STRUCTURAL:
          return false;
      }
      break;
  }

  return false;
}

type_t* type_qualify(type_t* type, typelist_t* typeargs)
{
  if(type->id != T_NOMINAL)
    return NULL;

  if(!typelist_constraints(type->n.type_params, type->n.constraints, typeargs))
    return NULL;

  if(type->n.type_params == NULL)
    return type;

  type_t* t = type_new(T_NOMINAL);
  t->cap = type->cap;
  t->ephemeral = type->ephemeral;
  t->n.ast = type->n.ast;
  t->n.traits = typelist_reify(type->n.traits, type->n.type_params, typeargs);
  t->n.functions = funlist_reify(type->n.functions,
    type->n.type_params, typeargs);

  // we should get the same number of traits and functions
  assert(typelist_length(t->n.traits) == typelist_length(type->n.traits));
  assert(funlist_length(t->n.functions) == funlist_length(type->n.functions));

  return type_store(t);
}

type_t* type_reify(type_t* type, typelist_t* formal, typelist_t* actual)
{
  assert(typelist_length(formal) == typelist_length(actual));

  if(formal == NULL)
    return type;

  type_t* t = type_new(type->id);
  t->cap = type->cap;
  t->ephemeral = type->ephemeral;

  switch(t->id)
  {
    case T_NOMINAL:
      t->n.ast = type->n.ast;
      t->n.type_params = typelist_reify(type->n.type_params, formal, actual);
      t->n.constraints = typelist_reify(type->n.constraints, formal, actual);

      // we should get the same type params and the same number of constraints
      assert(typelist_equals(t->n.type_params, type->n.type_params));
      assert(typelist_length(t->n.type_params) ==
        typelist_length(type->n.type_params));

      t->n.traits = typelist_reify(type->n.traits, formal, actual);
      t->n.functions = funlist_reify(type->n.functions, formal, actual);

      // we should get the same number of traits and functions
      assert(typelist_length(t->n.traits) == typelist_length(type->n.traits));
      assert(funlist_length(t->n.functions) ==
        funlist_length(type->n.functions));
      break;

    case T_STRUCTURAL:
      t->s.functions = funlist_reify(t->s.functions, formal, actual);
      break;

    case T_UNION:
    case T_ISECT:
    case T_TUPLE:
      t->e.left = type_reify(t->e.left, formal, actual);
      t->e.right = type_reify(t->e.right, formal, actual);
      break;
  }

  return type_store(t);
}

uint64_t type_hash(type_t* t)
{
  uint64_t h = inthash(t->cap);
  h ^= inthash(t->ephemeral);

  switch(t->id)
  {
    case T_NOMINAL:
      h ^= strhash(ast_name(ast_child(t->n.ast)));
      h ^= typelist_hash(t->n.type_params);
      break;

    case T_STRUCTURAL:
      h ^= funlist_hash(t->s.functions);
      break;

    case T_UNION:
    case T_ISECT:
    case T_TUPLE:
      h ^= type_hash(t->e.left);
      h ^= type_hash(t->e.right);
      break;
  }

  return h;
}

void type_done()
{
  typetab_free(type_table);
  function_done();
}
