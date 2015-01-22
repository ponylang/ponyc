#include "symbols.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"

#define DW_LANG_Pony 0x8002
#define DW_TAG_Producer "ponyc"

using namespace llvm;
using namespace llvm::dwarf;

enum
{
  SYMBOL_NEW = 1,
  SYMBOL_LANG = 1 << 1,
  SYMBOL_FILE = 1 << 2
};

typedef struct anchor_t
{
  DIType type;
  DIType qualified;
} anchor_t;

typedef struct symbol_t
{
  const char* name;

  union
  {
    anchor_t* anchor;
    MDNode* file;
  };

  uint16_t kind;
} symbol_t;

struct subnodes_t
{
  size_t size;
  size_t offset;
  DIType prelim;
  MDNode** children;
  /*const char* name;
  anchor_t* anchor;
  gentype_t* type;

  bool is_private;
  size_t line;
  size_t pos;*/
};

static uint64_t symbol_hash(symbol_t* symbol);
static bool symbol_cmp(symbol_t* a, symbol_t* b);
static void symbol_free(symbol_t* symbol);

DECLARE_HASHMAP(symbolmap, symbol_t);
DEFINE_HASHMAP(symbolmap, symbol_t, symbol_hash, symbol_cmp,
  pool_alloc_size, pool_free_size, symbol_free, NULL);

struct symbols_t
{
  symbolmap_t map;
  
  DIBuilder* builder;
  DICompileUnit unit;
  
  bool release;
  bool emit;
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
  // None of the symbols can be new, because we
  // are not generating debug info for unused
  // types.
  assert((symbol->kind & SYMBOL_NEW) == 0);

  if(symbol->kind == SYMBOL_LANG)
    POOL_FREE(anchor_t, symbol->anchor);

  POOL_FREE(symbol_t, symbol);
}

static symbol_t* get_anchor(symbols_t* symbols, const char* name)
{
  symbol_t key;
  key.name = name;

  symbol_t* value = symbolmap_get(&symbols->map, &key);

  if(value == NULL)
  {
    value = POOL_ALLOC(symbol_t);
    memset(value, 0, sizeof(symbol_t));

    value->name = key.name;
    value->kind = SYMBOL_NEW;
    symbolmap_put(&symbols->map, value);

    return value;
  }

  value->kind &= ~SYMBOL_NEW;
  return value;
}

static symbol_t* get_file_anchor(symbols_t* symbols, const char* fullpath)
{
  symbol_t* symbol = get_anchor(symbols, fullpath);

  if(symbol->kind & SYMBOL_NEW)
  {
    StringRef name = sys::path::filename(fullpath);
    StringRef path = sys::path::parent_path(fullpath);

    symbol->file = symbols->builder->createFile(name, path);
    symbol->kind |= SYMBOL_FILE;
  }

  return symbol;
}

static symbol_t* get_lang_anchor(symbols_t* symbols, const char* name)
{
  symbol_t* symbol = get_anchor(symbols, name);

  if(symbol->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = POOL_ALLOC(anchor_t);
    memset(anchor, 0, sizeof(anchor_t));

    symbol->anchor = anchor;
    symbol->kind |= SYMBOL_LANG;
  }

  return symbol;
}

symbols_t* symbols_init(compile_t* c)
{
  symbols_t* s = POOL_ALLOC(symbols_t);
  memset(s, 0, sizeof(symbols_t));

  symbolmap_init(&s->map, 0);
  s->builder = new DIBuilder(*unwrap(c->module));
  s->release = c->opt->release;
  s->emit = c->opt->symbols;

  return s;
}

llvm::DIFile symbols_file(symbols_t* symbols, const char* fullpath)
{
  symbol_t* symbol = get_file_anchor(symbols, fullpath);
  assert(symbol->kind & SYMBOL_FILE);
  
  return (DIFile)symbol->file;
}

bool symbols_known_type(symbols_t* symbols, const char* name)
{
  symbol_t key;
  key.name = name;

  return symbolmap_get(&symbols->map, &key) != NULL;
}

void symbols_declare(symbols_t* symbols, gentype_t* g, subnodes_t** members, 
  size_t size, symbol_scope_t* scope)
{
  symbol_t* symbol = get_lang_anchor(symbols, g->type_name);
  assert(symbol->kind & SYMBOL_LANG);

  if(symbol->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = symbol->anchor;
    subnodes_t* nodes = *members = (subnodes_t*)malloc(sizeof(subnodes_t));

    nodes->size = size;
    nodes->children = (MDNode**)calloc(size, sizeof(MDNode*));

    uint16_t tag = 0;

    if(g->underlying == TK_TUPLETYPE)
      tag = DW_TAG_structure_type;
    else
      tag = DW_TAG_class_type;

    // We do have to store the preliminary dwarf symbol seperately, because
    // of resursive types and the fact that nominal types are used as pointers.
    nodes->prelim = symbols->builder->createReplaceableForwardDecl(tag, 
      g->type_name, symbols->unit, scope->file, (int)scope->line);

    if(g->underlying == TK_TUPLETYPE)
    {
      // The actual use type is the structure itself. Also, tuples are
      // constant structures.
      anchor->type = symbols->builder->createQualifiedType(DW_TAG_constant, 
        nodes->prelim);

      // For convenience, we store the same type as qualified type.
      anchor->qualified = anchor->type;
    } else {
      // The use type is a pointer to the structure.
      anchor->type = symbols->builder->createPointerType(nodes->prelim,
        g->size, g->align);
      
      // A let field or method parameter is equivalent to a 
      // C <type>* const <identifier>.
      anchor->qualified = symbols->builder->createQualifiedType(
        DW_TAG_const_type, anchor->type);
    }
  }
}

void symbols_package(symbols_t* symbols, const char* path, const char* name)
{
  symbols->unit = symbols->builder->createCompileUnit(DW_LANG_Pony, name, path,
    DW_TAG_Producer, symbols->release, StringRef(), 0, StringRef(), 
    llvm::DIBuilder::FullDebug, symbols->emit);
}

void symbols_basic(symbols_t* symbols, gentype_t* g)
{
  symbol_t* basic = get_lang_anchor(symbols, g->type_name);
  assert(basic->kind & SYMBOL_LANG);

  if(basic->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = basic->anchor;
    uint16_t tag = 0;
    
    switch(g->type_name[0])
    {
      case 'U': tag = dwarf::DW_ATE_unsigned; break;
      case 'I': tag = dwarf::DW_ATE_signed; break;
      case 'F': tag = dwarf::DW_ATE_float; break;
      case 'B': tag = dwarf::DW_ATE_boolean; break;
      default: assert(0);
    }
  
    DIType type = symbols->builder->createBasicType(g->type_name, g->size,
      g->align, tag);

    // Eventually, basic builtin types may be used as const, e.g. let field or
    // local, method/behaviour parameter.
    DIType qualified = symbols->builder->createQualifiedType(DW_TAG_constant,
      type);

    anchor->type = type;
    anchor->qualified = qualified;
  }
}

void symbols_trait(symbols_t* symbols, gentype_t* g, symbol_scope_t* scope)
{
  symbol_t* trait = get_lang_anchor(symbols, g->type_name);
  assert(trait->kind & SYMBOL_LANG);

  if(trait->kind & SYMBOL_NEW)
  {
    // There is no qualified type for a trait, because traits are not used
    // directly. Just like classes, traits are always used as pointers to 
    // some underlying runtime class.
    anchor_t* anchor = trait->anchor;

    DIType trait_type = symbols->builder->createClassType(symbols->unit,
      g->type_name, scope->file, (int)scope->line, g->size, g->align, 0, 0,
      DIType(), DIArray());

    anchor->type = symbols->builder->createPointerType(trait_type, g->size,
      g->align);

    anchor->qualified = symbols->builder->createQualifiedType(
      DW_TAG_const_type, anchor->type);
  }
}

void symbols_member(symbols_t* symbols, gentype_t* composite, gentype_t* field,
  subnodes_t* subnodes, symbol_scope_t* scope, const char* name, bool priv, 
  bool constant, size_t index)
{
  unsigned visibility = priv ? DW_ACCESS_private : DW_ACCESS_public;

  symbol_t* field_symbol = get_lang_anchor(symbols, field->type_name);
  symbol_t* composite_symbol = get_lang_anchor(symbols, composite->type_name);
  
  // We must have heard about the members type already. Also, the composite
  // type is at least preliminary.
  assert((field_symbol->kind & SYMBOL_NEW) == 0);
  assert((composite_symbol->kind & SYMBOL_NEW) == 0);

  DIType use_type = DIType();

  if(constant)
    use_type = composite_symbol->anchor->qualified;
  else
    use_type = composite_symbol->anchor->type;

  subnodes->children[index] = symbols->builder->createMemberType(symbols->unit,
    name, scope->file, (int)scope->line, field->size, field->align, 
    subnodes->offset, visibility, use_type);

  subnodes->offset += field->size;
}

void symbols_composite(symbols_t* symbols, gentype_t* g, size_t offset, 
  subnodes_t* subnodes, symbol_scope_t* scope)
{
  // The composite was previously forward declared, and a preliminary
  // debug symbol exists.
  assert(subnodes->prelim != NULL);
  
  DIType actual = DIType();
 
  if(g->underlying == TK_TUPLETYPE)
  {
    actual = symbols->builder->createStructType(symbols->unit, g->type_name,
      DIFile(), 0, g->size, g->align, 0, DIType(), DIArray());
  } else {
    actual = symbols->builder->createClassType(symbols->unit, g->type_name, 
      scope->file, (int)scope->line, g->size, g->align, offset, 0, DIType(),
      DIArray());
  }

  subnodes->prelim.replaceAllUsesWith(actual);
  free(subnodes->children);
  free(subnodes);
}

void symbols_finalise(symbols_t* symbols)
{
  if(symbols->emit)
  {
    printf("Emitting debug symbols\n");
    assert(symbols->unit.Verify());
    symbols->builder->finalize();
  }
  
  symbolmap_destroy(&symbols->map);
  POOL_FREE(symbols_t, symbols);
}