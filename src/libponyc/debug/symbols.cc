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
#include <llvm/Config/llvm-config.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace llvm;
using namespace llvm::dwarf;

typedef struct anchor_t anchor_t;
typedef struct debug_sym_t debug_sym_t;

enum
{
  SYMBOL_NEW    = 1 << 0,
  SYMBOL_ANCHOR = 1 << 1,
  SYMBOL_FILE   = 1 << 2
};

struct anchor_t
{
  DIType type;
  DIType qualified;
};

struct debug_sym_t
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
  size_t offset;
  DIType prelim;
  MDNode** children;
};

static uint64_t symbol_hash(debug_sym_t* symbol);
static bool symbol_cmp(debug_sym_t* a, debug_sym_t* b);
static void symbol_free(debug_sym_t* symbol);

DECLARE_HASHMAP(symbolmap, debug_sym_t);
DEFINE_HASHMAP(symbolmap, debug_sym_t, symbol_hash, symbol_cmp,
  pool_alloc_size, pool_free_size, symbol_free);

struct symbols_t
{
  symbolmap_t map;

  DIBuilder* builder;
  DICompileUnit unit;

  bool release;
};

static uint64_t symbol_hash(debug_sym_t* symbol)
{
  return hash_ptr(symbol->name);
}

static bool symbol_cmp(debug_sym_t* a, debug_sym_t* b)
{
  return a->name == b->name;
}

static void symbol_free(debug_sym_t* symbol)
{
  // None of the symbols can be new, because we are not generating debug info
  // for unused types.
  //assert((symbol->kind & SYMBOL_NEW) == 0);

  if(symbol->kind == SYMBOL_ANCHOR)
    POOL_FREE(anchor_t, symbol->anchor);

  POOL_FREE(debug_sym_t, symbol);
}

static debug_sym_t* get_entry(symbols_t* symbols, const char* name)
{
  debug_sym_t key;
  key.name = name;

  debug_sym_t* value = symbolmap_get(&symbols->map, &key);

  if(value == NULL)
  {
    value = POOL_ALLOC(debug_sym_t);
    memset(value, 0, sizeof(debug_sym_t));

    value->name = key.name;
    value->kind = SYMBOL_NEW;
    symbolmap_put(&symbols->map, value);

    return value;
  }

  value->kind &= (uint16_t)~SYMBOL_NEW;
  return value;
}

static DIFile get_file(symbols_t* symbols, const char* fullpath)
{
  debug_sym_t* symbol = get_entry(symbols, fullpath);

  if(symbol->kind & SYMBOL_NEW)
  {
    StringRef name = sys::path::filename(fullpath);
    StringRef path = sys::path::parent_path(fullpath);

    symbol->file = symbols->builder->createFile(name, path);
    symbol->kind |= SYMBOL_FILE;
  }

  return (DIFile)symbol->file;
}

static debug_sym_t* get_anchor(symbols_t* symbols, const char* name)
{
  debug_sym_t* symbol = get_entry(symbols, name);

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

  Module* m = unwrap(module);

  m->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 3);

  m->addModuleFlag(llvm::Module::Error, "Debug Info Version",
    llvm::DEBUG_METADATA_VERSION);

  s->builder = new DIBuilder(*m);
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
  debug_sym_t* basic = get_anchor(symbols, meta->name);

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
    DIType qualified = symbols->builder->createQualifiedType(DW_TAG_const_type,
      type);

    anchor->type = type;
    anchor->qualified = qualified;
  }
}

void symbols_pointer(symbols_t* symbols, dwarf_meta_t* meta)
{
  debug_sym_t* pointer = get_anchor(symbols, meta->name);
  debug_sym_t* typearg = get_anchor(symbols, meta->typearg);

  anchor_t* symbol = pointer->anchor;
  anchor_t* target = typearg->anchor;

  // We must have seen the pointers target before. Also the target
  // symbol is not preliminary, because the pointer itself is not
  // preliminary.
  assert((pointer->kind & SYMBOL_NEW) != 0);
  assert((typearg->kind & SYMBOL_NEW) == 0);

  symbol->type = symbols->builder->createPointerType(target->type, meta->size,
    meta->align);

  symbol->qualified = symbols->builder->createQualifiedType(DW_TAG_const_type,
      symbol->type);
}

void symbols_trait(symbols_t* symbols, dwarf_meta_t* meta)
{
  debug_sym_t* trait = get_anchor(symbols, meta->name);

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

void symbols_unspecified(symbols_t* symbols, const char* name)
{
  debug_sym_t* type = get_anchor(symbols, name);

  anchor_t* unspecified = type->anchor;
  unspecified->type = symbols->builder->createUnspecifiedType(name);
  unspecified->qualified = unspecified->type;
}

void symbols_declare(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta)
{
  debug_sym_t* symbol = get_anchor(symbols, meta->name);

  anchor_t* anchor = symbol->anchor;
  subnodes_t* nodes = POOL_ALLOC(subnodes_t);
  memset(nodes, 0, sizeof(subnodes_t));

  if(frame->size > 0)
  {
    nodes->children = (MDNode**)pool_alloc_size(frame->size*sizeof(MDNode*));
    memset(nodes->children, 0, frame->size*sizeof(MDNode*));
  }

  DIFile file = get_file(symbols, meta->file);
  uint16_t tag = DW_TAG_class_type;

  if(meta->flags & DWARF_TUPLE)
    tag = DW_TAG_structure_type;

  // We do have to store the preliminary dwarf symbol seperately, because
  // of resursive types and the fact that nominal types are used as pointers.
#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR > 6)
  nodes->prelim = symbols->builder->createReplaceableCompositeType(tag,
    meta->name, symbols->unit, file, (int)meta->line);
#else
  nodes->prelim = symbols->builder->createReplaceableForwardDecl(tag,
    meta->name, symbols->unit, file, (int)meta->line);
#endif

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

  debug_sym_t* field_symbol = get_anchor(symbols, meta->typearg);

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
  assert(frame->index <= frame->size);
}

void symbols_method(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta, LLVMValueRef ir)
{
  // Emit debug info for the subroutine type.
  VLA(MDNode*, params, meta->size);

  debug_sym_t* current = get_anchor(symbols, meta->params[0]);
  params[0] = current->anchor->type;

  for(size_t i = 1; i < meta->size; i++)
  {
    current = get_anchor(symbols, meta->params[i]);
    assert((current->kind & SYMBOL_NEW) == 0);

    params[i] = current->anchor->qualified;
  }

  DIFile file = get_file(symbols, meta->file);

#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR > 5)
  DITypeArray uses = symbols->builder->getOrCreateTypeArray(ArrayRef<Metadata*>(
    (Metadata**)params, meta->size));
#else
  DIArray uses = symbols->builder->getOrCreateArray(ArrayRef<Value*>(
    (Value**)params, meta->size));
#endif

  DICompositeType type = symbols->builder->createSubroutineType(file,
    uses, llvm::DIDescriptor::FlagLValueReference);

  // Emit debug info for the method itself.
  current = get_anchor(symbols, meta->mangled);
  assert(current->kind & SYMBOL_NEW);

  Function* f = dyn_cast_or_null<Function>(unwrap(ir));

  DISubprogram fun = symbols->builder->createFunction(symbols->unit,
    meta->name, meta->mangled, file, (int)meta->line, type, false, true,
    (int)meta->offset, 0, symbols->release, f);

  if(frame->members != NULL)
  {
    assert(frame->index <= frame->size);
    subnodes_t* subnodes = frame->members;
    subnodes->children[frame->index] = fun;
    frame->index += 1;
  }

  frame->scope = fun;
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

  if(frame->size > 0)
  {
#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR > 5)
    Metadata** members = (Metadata**)subnodes->children;

    fields = symbols->builder->getOrCreateArray(ArrayRef<Metadata*>(members,
      frame->size));
#else
    Value** members = (Value**)subnodes->children;

    fields = symbols->builder->getOrCreateArray(ArrayRef<Value*>(members,
      frame->size));
#endif
  }

  if(meta->flags & DWARF_TUPLE)
  {
    debug_sym_t* symbol = get_anchor(symbols, meta->name);
    anchor_t* tuple = symbol->anchor;

    tuple->type = actual = symbols->builder->createStructType(symbols->unit,
      meta->name, file, (int)meta->line, meta->size, meta->align, 0, DIType(),
      fields);
  } else {
    actual = symbols->builder->createClassType(symbols->unit, meta->name, file,
      (int)meta->line, meta->size, meta->align, meta->offset, 0, DIType(),
      fields);
  }

  subnodes->prelim.replaceAllUsesWith(actual);

  if(frame->size > 0)
  {
    pool_free_size(sizeof(MDNode*) * frame->size, subnodes->children);
  }

  POOL_FREE(subnodes_t, subnodes);
  frame->members = NULL;
}

void symbols_lexicalscope(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta)
{
  MDNode* parent = (MDNode*)frame->prev->scope;
  DIFile file = get_file(symbols, meta->file);

  frame->scope = symbols->builder->createLexicalBlock((DIDescriptor)parent, 
    file, (unsigned)meta->line, (unsigned)meta->pos);
}

void symbols_finalise(symbols_t* symbols)
{
  assert(symbols->unit.Verify());
  symbols->builder->finalize();
  delete symbols->builder;

  symbolmap_destroy(&symbols->map);
  POOL_FREE(symbols_t, symbols);
}
