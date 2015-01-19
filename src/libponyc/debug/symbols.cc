#include "symbols.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"

enum
{
  SYMBOL_DBG,
  SYMBOL_FILE
};

typedef struct anchor_t
{
  llvm::MDNode* type;
  llvm::MDNode* qualified;
} anchor_t;

typedef struct symbol_t
{
  const char* name;

  union
  {
    anchor_t* anchor;
    llvm::MDNode* file;
  };

  uint16_t kind;
} symbol_t;

static uint64_t symbol_hash(symbol_t* symbol);
static bool symbol_cmp(symbol_t* a, symbol_t* b);
static void symbol_free(symbol_t* symbol);

DECLARE_HASHMAP(symbolmap, symbol_t);
DEFINE_HASHMAP(symbolmap, symbol_t, symbol_hash, symbol_cmp,
  pool_alloc_size, pool_free_size, symbol_free, NULL);

struct symbols_t
{
  symbolmap_t map;
  
  Module* module;
  DataLayout* layout;
  DIBuilder* builder;
  DICompileUnit unit;
};

static uint64_t symbol_hash(symbol_t* symbol)
{
  return hash_ptr(symbol->name);
}

static bool symbol_cmp(symbol_t* a, symbol_t* b)
{
  return a->name == b->name;
}

static void symbol_free(symbol_t* symbol)
{
  if(symbol->kind == SYMBOL_DBG)
    POOL_FREE(anchor_t, symbol->anchor);

  POOL_FREE(symbol_t, symbol);
}

symbols_t* symbols_init(compile_t* c)
{
  symbols_t* n = POOL_ALLOC(symbols_t);
  memset(n, 0, sizeof(symbols_t));

  symbolmap_init(&n->map, 0);
  n->module = unwrap(c->module);
  n->layout = unwrap(c->target_data);
  n->builder = new DIBuilder(*c->module);

  return n;
}

void symbols_finalise(symbols_t* symbols, bool verify)
{
  assert(symbols->unit.Verify());
  symbols->builder->finalize();

  symbolmap_destroy(&symbols->map);
  POOL_FREE(symbols_t, symbols);
}

/*symbol_t* symbols_get(symbols_t* symbols, const char* name, bool file)
{
  symbol_t key;
  key.name = name;

  symbol_t* value = symbolmap_get(&symbols->map, &key);

  if(value == NULL)
  {
    value = POOL_ALLOC(symbol_t);
    value->name = name;
    value->file = NULL;
    value->kind = SYMBOL_FILE;

    if(!file)
    {
      value->anchor = POOL_ALLOC(anchor_t);
      value->kind = SYMBOL_DBG;
      memset(value->anchor, 0, sizeof(anchor_t));
    }

    symbolmap_put(&symbols->map, value);
  }

  return value;
}*/
