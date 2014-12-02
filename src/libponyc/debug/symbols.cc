#include "symbols.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"

static uint64_t symbol_hash(symbol_t* s);
static bool symbol_cmp(symbol_t* a, symbol_t* b);
static void symbol_free(symbol_t* s);

DECLARE_HASHMAP(symbolmap, symbol_t);
DEFINE_HASHMAP(symbolmap, symbol_t, symbol_hash, symbol_cmp,
  pool_alloc_size, pool_free_size, symbol_free, NULL);

struct symbols_t
{
  symbolmap_t map;
};

static uint64_t symbol_hash(symbol_t* s)
{
  return hash_ptr(s->name);
}

static bool symbol_cmp(symbol_t* a, symbol_t* b)
{
  return a->name == b->name;
}

static void symbol_free(symbol_t* s)
{
  POOL_FREE(symbol_t, s);
}

symbols_t* symbols_init(size_t size)
{
  symbols_t* n = POOL_ALLOC(symbols_t);

  symbolmap_init(&n->map, size);

  return n;
}

void symbols_destroy(symbols_t** symbols)
{
  symbols_t* s = *symbols;

  symbolmap_destroy(&s->map);
  POOL_FREE(symbols_t, s);

  *symbols = NULL;
}

symbol_t* symbols_get(symbols_t* symbols, const char* name)
{
  symbol_t key;
  key.name = name;

  symbol_t* value = symbolmap_get(&symbols->map, &key);

  if(value == NULL)
  {
    value = POOL_ALLOC(symbol_t);
    memset(value, 0, sizeof(symbol_t));

    value->name = name;
    value->node = NULL;

    symbolmap_put(&symbols->map, value);
  }

  return value;
}
