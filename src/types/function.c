#include "function.h"
#include "cap.h"
#include "../ds/stringtab.h"
#include "../ds/hash.h"
#include <stdlib.h>
#include <assert.h>

static bool function_cmp(function_t* a, function_t* b);
static uint64_t function_hash(function_t* f);
static function_t* function_dup(function_t* f);
static void function_free(function_t* f);

DEFINE_LIST(funlist, function_t, function_hash, function_eq, function_free);
DEFINE_TABLE(funtab, function_t, function_hash, function_cmp, function_dup,
  function_free);

typedef struct function_t
{
  cap_id cap;
  const char* name;
  typelist_t* type_params;
  typelist_t* constraints;
  typelist_t* params;
  type_t* result;
  bool throws;

  struct function_t* qualified;
} function_t;

static funtab_t* fun_table;

static bool function_cmp(function_t* a, function_t* b)
{
  return (a->cap == b->cap) && (a->name == b->name) &&
    typelist_equals(a->type_params, b->type_params) &&
    typelist_equals(a->constraints, b->constraints) &&
    typelist_equals(a->params, b->params) &&
    type_eq(a->result, b->result) &&
    (a->throws == b->throws);
}

static uint64_t function_hash(function_t* f)
{
  uint64_t h = inthash(f->cap);
  h ^= strhash(f->name);
  h ^= typelist_hash(f->type_params);
  h ^= typelist_hash(f->constraints);
  h ^= typelist_hash(f->params);
  h ^= type_hash(f->result);
  h ^= inthash(f->throws);

  return h;
}

static function_t* function_dup(function_t* f)
{
  return f;
}

static void function_free(function_t* f)
{
  if(f == NULL)
    return;

  typelist_free(f->type_params);
  typelist_free(f->constraints);
  typelist_free(f->params);
  free(f);
}

static function_t* function_store(function_t* f)
{
  if(fun_table == NULL)
    fun_table = funtab_create(4096);

  bool present;
  function_t* found = funtab_insert(fun_table, f, &present);

  if(present)
    function_free(f);

  return found;
}

static bool function_sup(function_t* b, function_t* a)
{
  return function_sub(a, b);
}

bool funlist_sub(funlist_t* a, funlist_t* b)
{
  return list_subset((list_t*)b, (list_t*)a, (cmp_fn)function_sup);
}

typedef struct typelist_pair_t
{
  typelist_t* formal;
  typelist_t* actual;
} typelist_pair_t;

static function_t* reify_map(function_t* f, void* arg)
{
  typelist_pair_t* pair = arg;
  return function_reify(f, pair->formal, pair->actual);
}

funlist_t* funlist_reify(funlist_t* list,
  typelist_t* formal, typelist_t* actual)
{
  typelist_pair_t pair = {formal, actual};
  return funlist_map(list, reify_map, &pair);
}

function_t* function_create(ast_t* ast)
{
  function_t* f = calloc(1, sizeof(function_t));
  token_id t = ast_id(ast);

  // receiver capability
  ast_t* child = ast_child(ast);

  switch(t)
  {
    case TK_NEW:
    case TK_NEWTYPE:
      assert(ast_id(child) == TK_NONE);
      f->cap = K_NEW;
      break;

    case TK_BE:
    case TK_BETYPE:
      assert(ast_id(child) == TK_NONE);
      f->cap = K_BE;
      break;

    case TK_FUN:
    case TK_FUNTYPE:
      switch(ast_id(child))
      {
        case TK_ISO: f->cap = K_ISO; break;
        case TK_TRN: f->cap = K_TRN; break;
        case TK_MUT: f->cap = K_MUT; break;
        case TK_IMM: f->cap = K_IMM; break;
        case TK_BOX: f->cap = K_BOX; break;
        case TK_TAG: f->cap = K_TAG; break;
        default: assert(0); return NULL;
      }
      break;

    default:
      assert(0);
      return NULL;
  }

  // function name
  child = ast_sibling(child);

  if(ast_id(child) == TK_ID)
  {
    f->name = ast_name(child);
  } else if((t == TK_NEW) || (t == TK_NEWTYPE)) {
    f->name = stringtab("create");
  } else {
    f->name = stringtab("apply");
  }

  // type parameters
  child = ast_sibling(child);
  ast_t* typeparam = ast_child(child);

  while(typeparam != NULL)
  {
    ast_t* type_var = ast_child(typeparam);
    type_t* t = type_create(type_var);

    assert(t != NULL);
    f->type_params = typelist_append(f->type_params, t);

    ast_t* constraint = ast_sibling(type_var);

    // TODO: is Any the right constraint for none?
    // empty structural type?
    if(ast_id(constraint) != TK_NONE)
      t = type_create(constraint);
    else
      t = type_name(ast, "Any", K_TAG);

    assert(t != NULL);
    f->constraints = typelist_append(f->constraints, t);

    // TODO: default type parameters?
    typeparam = ast_sibling(typeparam);
  }

  // parameters
  child = ast_sibling(child);

  if(ast_id(child) == TK_PARAMS)
  {
    ast_t* param = ast_child(child);

    while(param != NULL)
    {
      ast_t* param_type = ast_childidx(param, 1);
      f->params = typelist_append(f->params, type_create(param_type));
    }

    // TODO: default parameters?
    param = ast_sibling(param);
  } else {
    assert(ast_id(child) == TK_TYPES);
    ast_t* param = ast_child(child);

    while(param != NULL)
      f->params = typelist_append(f->params, type_create(param));

    param = ast_sibling(param);
  }

  // result
  child = ast_sibling(child);
  f->result = type_create(child);

  // throws
  child = ast_sibling(child);

  switch(ast_id(child))
  {
    case TK_QUESTION: f->throws = true; break;
    case TK_NONE: f->throws = false; break;
    default: assert(0); break;
  }

  // TODO: is this right?
  f->qualified = function_reify(f, f->type_params, f->constraints);
  return function_store(f);
}

bool function_eq(function_t* a, function_t* b)
{
  return a == b;
}

bool function_sub(function_t* a, function_t* b)
{
  // fun tag paint_car_red(a: Car): Car
  //
  // fun tag paint_vehicle_red(a: Vehicle): Vehicle
  //
  // fun tag paint_vehicle_red2[A: Vehicle](a: A): A
  //
  // fun tag paint_anthing_red[A: Thing](a: A): A
  //
  // paint_vehicle_red NOT <: paint_car_red
  //   return type is contravariant, must be covariant
  //
  // paint_vehicle_red2 <: paint_car_red
  //   when a Car is passed in, a Car is returned
  //   fun tag paint_car_red'(a: Car): Car
  //   fun tag paint_vehicle_red2'(a: Car): Car
  //
  // paint_anything_red <: paint_vehicle_red2
  //   reify paint_vehicle_red2 with its constraints:
  //     fun tag paint_vehicle_red2'(a: Vehicle): Vehicle
  //   plug parameter types from paint_vehicle_red2' into paint_anything_red
  //     by position
  //     fun tag paint_anything_red'(a: Vehicle): Vehicle
  //   now check subtype

  // TODO: infer type arguments for A from the parameter types of B
  // subtype relationship for functions with default parameters?
  // might expect to be able to specify the same parameters by name

  // test using functions qualified with their constraints
  return
    // accept at least the receivers
    cap_sub(b->cap, a->cap) &&
    // b must throw if a does
    (!a->throws || b->throws) &&
    // contravariant parameters
    typelist_sub(b->qualified->params, a->qualified->params) &&
    // covariant result
    type_sub(a->qualified->result, b->qualified->result);
}

function_t* function_qualify(function_t* f, typelist_t* typeargs)
{
  // check the type arguments against the constraints
  if(!typelist_constraints(f->type_params, f->constraints, typeargs))
    return NULL;

  // if we have no type params, we are done
  if(f->type_params == NULL)
    return f;

  // copy the function, with no type params
  function_t* fr = calloc(1, sizeof(function_t));
  fr->cap = f->cap;
  fr->name = f->name;
  fr->throws = f->throws;

  // reify our parameters and our result
  fr->params = typelist_reify(f->params, f->type_params, typeargs);
  fr->result = type_reify(f->result, f->type_params, typeargs);

  assert(typelist_length(fr->params) == typelist_length(f->params));
  assert(fr->result != NULL);

  // this is a qualified function, so it points to itself
  fr = function_store(fr);
  fr->qualified = fr;

  return fr;
}

function_t* function_reify(function_t* f,
  typelist_t* formal, typelist_t* actual)
{
  assert(typelist_length(formal) == typelist_length(actual));

  if(formal == NULL)
    return f;

  // copy the function
  function_t* fr = calloc(1, sizeof(function_t));
  fr->cap = f->cap;
  fr->name = f->name;
  fr->throws = f->throws;

  // reify the type params and constraints
  fr->type_params = typelist_reify(f->type_params, formal, actual);
  fr->constraints = typelist_reify(f->constraints, formal, actual);

  // we should get the same type params and the same number of constraints
  assert(typelist_equals(fr->type_params, f->type_params));
  assert(typelist_length(fr->type_params) == typelist_length(f->type_params));

  // reify the parameters and the result
  fr->params = typelist_reify(f->params, formal, actual);
  fr->result = type_reify(f->result, formal, actual);

  // we should get the same number of parameters and a valid result
  assert(typelist_length(fr->params) == typelist_length(f->params));
  assert(fr->result != NULL);

  fr = function_store(fr);

  // qualify the resulting function with its own constraints
  // TODO: is this right?
  fr->qualified = function_qualify(fr, fr->constraints);

  return fr;
}

void function_done()
{
  funtab_free(fun_table);
}
