#include "symbols.h"
#include "../codegen/gentype.h"
#include "../../libponyrt/ds/hash.h"
#include "../../libponyrt/mem/pool.h"

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4003)
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/Path.h>
#include <llvm/IR/Metadata.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace llvm;
using namespace llvm::dwarf;

typedef struct anchor_t anchor_t;
typedef struct symbol_t symbol_t;

enum
{
  SYMBOL_NEW  = 1 << 0,
  SYMBOL_ANCHOR = 1 << 1,
  SYMBOL_FILE = 1 << 2
};

struct anchor_t
{
  DIType type;
  DIType qualified;
};

struct symbol_t
{
  const char* name;

  union
  {
    anchor_t* anchor;
    MDNode* file;
  };

  uint16_t kind;
};

struct subnodes_t
{
  size_t size;
  size_t offset;
  DIType prelim;
  MDNode** children;
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
  // None of the symbols can be new, because we are not generating debug info
  // for unused types.
  //assert((symbol->kind & SYMBOL_NEW) == 0);

  if(symbol->kind == SYMBOL_ANCHOR)
    POOL_FREE(anchor_t, symbol->anchor);

  POOL_FREE(symbol_t, symbol);
}

static symbol_t* get_entry(symbols_t* symbols, const char* name)
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

static DIFile get_file(symbols_t* symbols, const char* fullpath)
{
  symbol_t* symbol = get_entry(symbols, fullpath);

  if(symbol->kind & SYMBOL_NEW)
  {
    StringRef name = sys::path::filename(fullpath);
    StringRef path = sys::path::parent_path(fullpath);

    symbol->file = symbols->builder->createFile(name, path);
    symbol->kind |= SYMBOL_FILE;
  }

  return (DIFile)symbol->file;
}

static symbol_t* get_anchor(symbols_t* symbols, const char* name)
{
  symbol_t* symbol = get_entry(symbols, name);

  if(symbol->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = POOL_ALLOC(anchor_t);
    memset(anchor, 0, sizeof(anchor_t));

    symbol->anchor = anchor;
    symbol->kind |= SYMBOL_ANCHOR;
  }

  return symbol;
}

void symbols_init(symbols_t** symbols, LLVMModuleRef module, bool optimised)
{
  symbols_t* s = *symbols = POOL_ALLOC(symbols_t);
  memset(s, 0, sizeof(symbols_t));

  symbolmap_init(&s->map, 0);
  s->builder = new DIBuilder(*unwrap(module));
  s->release = optimised;
}

void symbols_package(symbols_t* symbols, const char* path, const char* name)
{
  symbols->unit = symbols->builder->createCompileUnit(DW_LANG_Pony, name, path,
    DW_TAG_Producer, symbols->release, StringRef(), 0, StringRef(),
    llvm::DIBuilder::FullDebug, true);
}

void symbols_basic(symbols_t* symbols, dwarf_meta_t* meta)
{
  symbol_t* basic = get_anchor(symbols, meta->name);

  if(basic->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = basic->anchor;
    uint16_t tag = dwarf::DW_ATE_unsigned;

    switch(meta->flags)
    {
      case DWARF_SIGNED:
        tag = dwarf::DW_ATE_signed;
        break;
      case DWARF_FLOAT:
        tag = dwarf::DW_ATE_float;
        break;
      case DWARF_BOOLEAN:
        tag = dwarf::DW_ATE_boolean;
        break;
      default: {};
    }

    DIType type = symbols->builder->createBasicType(meta->name, meta->size,
      meta->align, tag);

    // Eventually, basic builtin types may be used as const, e.g. let field or
    // local, method/behaviour parameter.
    DIType qualified = symbols->builder->createQualifiedType(DW_TAG_constant,
      type);

    anchor->type = type;
    anchor->qualified = qualified;
  }
}

void symbols_pointer(symbols_t* symbols, dwarf_meta_t* meta)
{
  symbol_t* pointer = get_anchor(symbols, meta->name);
  symbol_t* typearg = get_anchor(symbols, meta->typearg);

  anchor_t* symbol = pointer->anchor;
  anchor_t* target = typearg->anchor;

  // We must have seen the pointers target before. Also the target
  // symbol is not preliminary, because the pointer itself is not
  // preliminary.
  assert(pointer->kind & SYMBOL_NEW);
  assert((typearg->kind & SYMBOL_NEW) == 0);

  symbol->type = symbols->builder->createPointerType(target->type, meta->size,
    meta->align);
}

void symbols_trait(symbols_t* symbols, dwarf_meta_t* meta)
{
  symbol_t* trait = get_anchor(symbols, meta->name);

  if(trait->kind & SYMBOL_NEW)
  {
    anchor_t* anchor = trait->anchor;

    DIFile file = get_file(symbols, meta->file);

    DIType type = symbols->builder->createClassType(symbols->unit, meta->name,
      file, (int)meta->line, meta->size, meta->align, meta->offset, 0,
      DIType(), DIArray());

    anchor->type = symbols->builder->createPointerType(type, meta->size,
      meta->align);

    anchor->qualified = symbols->builder->createQualifiedType(
      DW_TAG_const_type, anchor->type);
  }
}

void symbols_declare(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta)
{
  symbol_t* symbol = get_anchor(symbols, meta->name);

  anchor_t* anchor = symbol->anchor;
  subnodes_t* nodes = (subnodes_t*)POOL_ALLOC(subnodes_t);

  nodes->size = meta->size;
  nodes->offset = 0;
  nodes->children = (MDNode**)pool_alloc_size(meta->size*sizeof(MDNode*));
  memset(nodes->children, 0, meta->size*sizeof(MDNode*));

  DIFile file = get_file(symbols, meta->file);
  uint16_t tag = DW_TAG_class_type;

  if(meta->flags & DWARF_TUPLE)
    tag = DW_TAG_structure_type;

  // We do have to store the preliminary dwarf symbol seperately, because
  // of resursive types and the fact that nominal types are used as pointers.
  nodes->prelim = symbols->builder->createReplaceableForwardDecl(tag,
    meta->name, symbols->unit, file, (int)meta->line);

  if(meta->flags & DWARF_TUPLE)
  {
    // The actual use type is the structure itself.
    anchor->type = nodes->prelim;

    anchor->qualified = symbols->builder->createQualifiedType(DW_TAG_constant,
      nodes->prelim);
  } else {
    // The use type is a pointer to the structure.
    anchor->type = symbols->builder->createPointerType(nodes->prelim,
      meta->size, meta->align);

    // A let field or method parameter is equivalent to a
    // C <type>* const <identifier>.
    anchor->qualified = symbols->builder->createQualifiedType(
      DW_TAG_const_type, anchor->type);
  }

  frame->members = nodes;
}

void symbols_field(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta)
{
  subnodes_t* subnodes = frame->members;
  unsigned visibility = DW_ACCESS_public;

  if(meta->flags & DWARF_PRIVATE)
    visibility = DW_ACCESS_private;

  symbol_t* field_symbol = get_anchor(symbols, meta->name);

  DIType use_type = DIType();

  if(meta->flags & DWARF_CONSTANT)
    use_type = field_symbol->anchor->qualified;
  else
    use_type = field_symbol->anchor->type;

  DIFile file = get_file(symbols, meta->file);

  subnodes->children[frame->index] = symbols->builder->createMemberType(
    symbols->unit, meta->name, file, (int)meta->line, meta->size, meta->align,
    subnodes->offset, visibility, use_type);

  subnodes->offset += meta->size;
  frame->index += 1;
}

void symbols_composite(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta)
{
  // The composite was previously forward declared, and a preliminary
  // debug symbol exists.
  subnodes_t* subnodes = frame->members;

  DIFile file = get_file(symbols, meta->file);
  DIType actual = DIType();
  DIArray fields = DIArray();

  if(subnodes->size > 0)
  {
    Value** members = (Value**)subnodes->children;

    fields = symbols->builder->getOrCreateArray(ArrayRef<Value*>(members,
      subnodes->size));
  }

  if(meta->flags & DWARF_TUPLE)
  {
    actual = symbols->builder->createStructType(symbols->unit, meta->name,
      DIFile(), 0, meta->size, meta->align, 0, DIType(), fields);
  } else {
    actual = symbols->builder->createClassType(symbols->unit, meta->name, file,
      (int)meta->line, meta->size, meta->align, meta->offset, 0, DIType(),
      fields);
  }

  subnodes->prelim.replaceAllUsesWith(actual);

  pool_free_size(sizeof(MDNode*) * subnodes->size, subnodes->children);
  POOL_FREE(subnodes_t, subnodes);
}

void symbols_finalise(symbols_t* symbols)
{
  assert(symbols->unit.Verify());
  symbols->builder->finalize();

  symbolmap_destroy(&symbols->map);
  POOL_FREE(symbols_t, symbols);
}
