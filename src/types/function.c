#include "function.h"
#include "cap.h"
#include "types.h"
#include "../ds/hash.h"
#include <stdlib.h>

static bool function_cmp(function_t* a, function_t* b);
static uint64_t function_hash(function_t* f);
static function_t* function_dup(function_t* f);
static void function_free(function_t* f);

DEFINE_LIST(funlist, function_t, function_hash, function_cmp, function_free);
DEFINE_TABLE(funtab, function_t, function_hash, function_cmp, function_dup,
  function_free);

typedef struct function_t
{
  cap_id cap;
  const char* name;
  typelist_t* type_params;
  typelist_t* params;
  type_t* result;
  bool throws;
} function_t;

static funtab_t* fun_table;

static bool function_cmp(function_t* a, function_t* b)
{
  return (a->cap == b->cap) && (a->name == b->name) &&
    typelist_equals(a->type_params, b->type_params) &&
    typelist_equals(a->params, b->params) &&
    type_eq(a->result, b->result) &&
    (a->throws == b->throws);
}

static uint64_t function_hash(function_t* f)
{
  uint64_t h = inthash(f->cap);
  h ^= strhash(f->name);
  h ^= typelist_hash(f->type_params);
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
  typelist_free(f->params);
}

static bool function_sup(function_t* a, function_t* b)
{
  return function_sub(b, a);
}

bool funlist_sub(funlist_t* a, funlist_t* b)
{
  return list_subset((list_t*)a, (list_t*)b, (cmp_fn)function_sup);
}

function_t* function_new()
{
  function_t* f = calloc(1, sizeof(function_t));
  return f;
}

function_t* function_store(function_t* f)
{
  if(fun_table == NULL)
    fun_table = funtab_create(4096);

  bool present;
  function_t* found = funtab_insert(fun_table, f, &present);

  if(present)
    function_free(f);

  return found;
}

bool function_eq(function_t* a, function_t* b)
{
  return a == b;
}

bool function_sub(function_t* a, function_t* b)
{
  return
    // accept at least the receivers
    cap_sub(b->cap, a->cap) &&
    // invariant type parameters
    typelist_equals(a->type_params, b->type_params) &&
    // contravariant parameters
    typelist_sub(b->params, a->params) &&
    // covariant result
    type_sub(a->result, b->result) &&
    // compatible throw
    (!a->throws || b->throws);
}

void function_done()
{
  funtab_free(fun_table);
}
