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

typedef enum
{
  T_NOMINAL,
  T_STRUCTURAL,
  T_UNION,
  T_ISECT,
  T_TUPLE
} type_id;

typedef enum
{
  T_UNCHECKED,
  T_CHECKING,
  T_VALID,
  T_INVALID
} type_ok;

typedef struct type_t type_t;
static bool type_cmp(type_t* a, type_t* b);
static type_t* type_dup(type_t* data);
static void type_free(type_t* t);

DEFINE_LIST(typelist, type_t, type_hash, type_cmp, NULL);
DEFINE_TABLE(typetab, type_t, type_hash, type_cmp, type_dup, type_free);

typedef struct structural_t
{
  funlist_t* functions;
} structural_t;

// FIX: substitute type params in functions and fields
typedef struct nominal_t
{
  ast_t* ast;
  typelist_t* type_params;
  typelist_t* traits;
  funlist_t* functions;
} nominal_t;

typedef struct typeexpr_t
{
  type_t* left;
  type_t* right;
} typeexpr_t;

struct type_t
{
  type_id id;
  type_ok valid;
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

// static type_t* type_new(type_id id)
// {
//   type_t* type = calloc(1, sizeof(type_t));
//   type->id = id;
//   return type;
// }

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
static bool a_is_obj_b(const type_t* a, const type_t* b)
{
  // invariant formal parameters
  return (a->id == T_OBJECT)
    && (a->obj.ast == b->obj.ast)
    && list_equals(a->obj.params, b->obj.params, type_cmp);
}

static bool a_in_obj_b(const type_t* a, const type_t* b)
{
  if(a->id != T_OBJECT) { return false; }
  if(a_is_obj_b(a, b)) { return true; }

  // check reified traits
  ast_t* trait = ast_child(ast_childidx(a->obj.ast, 4));

  while(trait != NULL)
  {
    if(type_sub(type_subst(ast_data(trait), a->obj.params), b))
    {
      return true;
    }

    trait = ast_sibling(trait);
  }

  return false;
}

static bool a_is_fun_b(const type_t* a, const type_t* b)
{
  switch(a->id)
  {
    case T_FUNCTION:
    {
      // invariant parameters
      if(!list_equals(a->fun.params, b->fun.params, type_cmp)) return false;

      // invariant result
      if(!type_eq(a->fun.result, b->fun.result)) return false;

      // invariant throw
      if(a->fun.throws != b->fun.throws) return false;

      return true;
    }

    default: {}
  }

  return false;
}

static bool a_in_fun_b(const type_t* a, const type_t* b)
{
  switch(a->id)
  {
    case T_FUNCTION:
    {
      // contravariant parameters
      if(!list_equals(b->fun.params, a->fun.params, type_cmpsub)) return false;

      // covariant result
      if(!type_sub(a->fun.result, b->fun.result)) { return false; }

      // a can't throw if b doesn't throw
      if(!b->fun.throws && a->fun.throws) { return false; }

      return true;
    }

    case T_OBJECT:
      // FIX: see if the apply() method conforms
      return false;

    default: {}
  }

  return false;
}

static bool a_in_adt_b(const type_t* a, const type_t* b)
{
  return list_find(b->adt.types, type_cmpsub, a) != NULL;
}

static bool adt_a_in_b(const type_t* a, const type_t* b)
{
  return list_test(a->adt.types, type_cmpsup, (void*)b);
}

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

static type_t* nametype(ast_t* scope, const char* name)
{
  ast_t* type_ast = ast_get(scope, name);

  if(type_ast == NULL)
  {
    if(!strcmp(name, "_")) { return &infer; }
    return NULL;
  }

  type_t* type = type_new(T_OBJECT);
  type->obj.ast = type_ast;

  return type;
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

  // FIX: get the mode with the viewpoint

  return expand_alias(type);
}

static type_t* funtype(ast_t* ast)
{
  type_t* type = type_new(T_FUNCTION);
  ast_t* child = ast_child(ast);

  if(ast_id(child) == TK_QUESTION) { type->fun.throws = true; }
  child = ast_sibling(child);

  // FIX: get the mode with the viewpoint
  child = ast_sibling(child);
  list_t* list = typelist(child);

  if(list == POINTER_ERROR)
  {
    type_free(type);
    return NULL;
  }

  type->fun.params = list;

  child = ast_sibling(child);
  type->fun.result = type_ast(child);

  if(type->fun.result == NULL)
  {
    type_free(type);
    return NULL;
  }

  return type;
}

static type_t* adttype(ast_t* ast)
{
  type_t* type = type_new(T_ADT);
  list_t* list = typelist(ast);

  if(list == POINTER_ERROR)
  {
    type_free(type);
    return NULL;
  }

  type->adt.types = list;

  switch(list_length(type->adt.types))
  {
    case 0:
      // an ADT with no elements is an error
      ast_error(ast, "ADT is empty");
      type_free(type);
      return NULL;

    case 1:
    {
      // if only one element, ditch the ADT wrapper
      type_t* child = list_data(type->adt.types);
      type_free(type);
      return child;
    }

    default: {}
  }

  return type;
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
  // FIX:
  return false;
}

static bool nominal_in_structural(type_t* a, type_t* b)
{
  return funlist_sub(a->s.functions, b->n.functions);
}

static type_t* union_type(ast_t* ast)
{
  // FIX:
  return NULL;
}

static type_t* isect_type(ast_t* ast)
{
  // FIX:
  return NULL;
}

static type_t* tuple_type(ast_t* ast)
{
  // FIX:
  return NULL;
}

static type_t* nominal_type(ast_t* ast)
{
  // FIX:
  return NULL;
}

static type_t* structural_type(ast_t* ast)
{
  // FIX:
  return NULL;
}

bool typelist_sub(typelist_t* a, typelist_t* b)
{
  return list_equals((list_t*)a, (list_t*)b, (cmp_fn)type_sub);
}

type_t* type_ast(ast_t* ast)
{
  if(ast == NULL)
    return NULL;

  type_t* type;

  switch(ast_id(ast))
  {
    case TK_NOMINAL:
      type = nominal_type(ast);
      break;

    case TK_STRUCTURAL:
      type = structural_type(ast);
      break;

    case TK_UNIONTYPE:
      type = union_type(ast);
      break;

    case TK_ISECTTYPE:
      type = isect_type(ast);
      break;

    case TK_TUPLETYPE:
      type = tuple_type(ast);
      break;

    default:
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
