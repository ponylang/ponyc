#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/Path.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include "dwarf.h"
#include "symbols.h"
#include "../ast/error.h"
#include "../pkg/package.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/pony.h"

#define DW_LANG_Pony 0x8002
#define PRODUCER "ponyc"
#define SKIP_CLASS_DESC (sizeof(void*) << 3)
#define SKIP_ACTOR_DESC SKIP_CLASS_DESC + (sizeof(pony_actor_pad_t) << 3)

using namespace llvm;
using namespace llvm::dwarf;

struct dwarf_info_t
{
  size_t size;
  MDNode** members;
};

struct dwarf_t
{
  symbols_t* symbols;

  bool optimized;
  bool emit;

  Module* module;
  DataLayout* layout;
  DIBuilder* builder;
  DICompileUnit unit;
};

typedef struct typeinfo_t
{
  DIFile file;
  size_t line;
  size_t pos;
  size_t size;
  size_t align;
} typeeinfo_t;

static DIFile mdfile(dwarf_t* dwarf, ast_t* module)
{
  source_t* source = (source_t*)ast_data(module);
  symbol_t* s = symbols_get(dwarf->symbols, source->file, true);

  if(s->file == NULL)
  {
    StringRef name = sys::path::filename(source->file);
    StringRef path = sys::path::parent_path(source->file);

    s->file = dwarf->builder->createFile(name, path);
  }

  return (DIFile)s->file;
}

static void get_typeinfo(dwarf_t* dwarf, ast_t* ast, LLVMTypeRef typeref,
  typeinfo_t* info)
{
  memset(info, 0, sizeof(typeinfo_t));

  // Tuple types are unscoped, as their declaration may be ambiguous.
  if((ast != NULL) && ast_id(ast) != TK_TUPLETYPE)
  {
    info->file = mdfile(dwarf, ast_nearest(ast, TK_MODULE));
    info->line = ast_line(ast);
    info->pos = ast_pos(ast);
  }

  if(typeref != NULL)
  {
    Type* type = unwrap(typeref);

    info->size = dwarf->layout->getTypeSizeInBits(type);
    info->align = dwarf->layout->getABITypeAlignment(type) << 3;
  }
}

void dwarf_compileunit(dwarf_t* dwarf, ast_t* program)
{
  if(!dwarf->emit)
    return;

  ast_t* package = ast_child(program);

  StringRef file = StringRef(package_filename(package));
  StringRef path = sys::path::parent_path(package_path(package));

  dwarf->unit = dwarf->builder->createCompileUnit(DW_LANG_Pony, file, path,
    PRODUCER, dwarf->optimized, StringRef(), 0);
}

void dwarf_forward(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  symbol_t* s = symbols_get(dwarf->symbols, g->type_name, false);
  dbg_symbol_t* d = s->dbg;

  if(d->type == NULL)
  {
    DIBuilder* builder = dwarf->builder;

    g->debug_info = POOL_ALLOC(dwarf_info_t);
    memset(g->debug_info, 0, sizeof(dwarf_info_t));

    size_t n = g->field_count;

    if(g->underlying != TK_TUPLETYPE)
      n += ast_childcount(ast_childidx(ast, 4)) - n;

    if(n > 0)
    {
      g->debug_info->size = n;
      g->debug_info->members = (MDNode**)calloc(n, sizeof(MDNode*));
    }

    typeinfo_t info;
    get_typeinfo(dwarf, ast, NULL, &info);

    uint16_t tag = 0;

    if(g->underlying == TK_TUPLETYPE)
      tag = DW_TAG_structure_type;
    else
      tag = DW_TAG_class_type;

    d->type = builder->createReplaceableForwardDecl(tag, g->type_name,
      dwarf->unit, info.file, (int)info.line);
  }
}

void dwarf_basic(dwarf_t* dwarf, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  symbol_t* s = symbols_get(dwarf->symbols, g->type_name, false);
  dbg_symbol_t* d = s->dbg;

  if(d->type == NULL)
  {
    typeinfo_t info;
    get_typeinfo(dwarf, NULL, g->primitive, &info);

    uint16_t basic_type = 0;

    switch(g->type_name[0])
    {
      case 'U': basic_type = dwarf::DW_ATE_unsigned; break;
      case 'I': basic_type = dwarf::DW_ATE_signed; break;
      case 'F': basic_type = dwarf::DW_ATE_float; break;
      case 'B': basic_type = dwarf::DW_ATE_boolean; break;
      default: assert(0);
    }

    DIType basic = dwarf->builder->createBasicType(g->type_name, info.size,
      info.align, basic_type);

    DIType constant = dwarf->builder->createQualifiedType(DW_TAG_constant,
      basic);

    d->type = basic;
    d->qualified = constant;
  }
}

void dwarf_pointer(dwarf_t* dwarf, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  ast_t* type = g->ast;

  if(is_pointer(type))
  {
    ast_t* typeargs = ast_childidx(type, 2);
    ast_t* target = ast_child(typeargs);

    (void)target;

  }
}

void dwarf_trait(dwarf_t* dwarf, ast_t* def, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  const char* name = ast_name(ast_child(def));
  symbol_t* s = symbols_get(dwarf->symbols, name, false);
  dbg_symbol_t* d = s->dbg;

  if(d->type == NULL)
  {
    typeinfo_t info;
    get_typeinfo(dwarf, def, g->use_type, &info);

    d->type = dwarf->builder->createClassType(dwarf->unit, name, info.file,
      (int)info.line, info.size, info.align, 0, 0, DIType(), DIArray());
  }
}

void dwarf_composite(dwarf_t* dwarf, ast_t* def, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  symbol_t* s = symbols_get(dwarf->symbols, g->type_name, false);
  dbg_symbol_t* d = s->dbg;

  assert(d->type != NULL);

  typeinfo_t info;
  get_typeinfo(dwarf, def, g->structure, &info);

  size_t n = g->debug_info->size;
  DIArray m = DIArray();
  DICompositeType forward = DICompositeType(d->type);
  DIBuilder* builder = dwarf->builder;

  if(n > 0)
  {
    Value** members = (Value**)g->debug_info->members;
    m = builder->getOrCreateArray(ArrayRef<Value*>(members, n));
  }

  size_t offset = 0;

  switch(g->underlying)
  {
    case TK_PRIMITIVE:
    case TK_CLASS: offset = SKIP_CLASS_DESC; break;
    case TK_ACTOR: offset = SKIP_ACTOR_DESC; break;
    default: assert(0);
  }

  MDNode* actual = builder->createClassType(dwarf->unit, g->type_name,
    info.file, (int)info.line, info.size, info.align, offset, 0, DIType(), m);

  forward.replaceAllUsesWith(actual);

  get_typeinfo(dwarf, NULL, g->use_type, &info);

  //d->type = builder->createPointerType(actual, info.size, info.align);
  //d->qualified = builder->createQualifiedType(DW_TAG_const_type, d->type);
}

void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field)
{
  if(!dwarf->emit)
    return;

  (void)composite;
  (void)field;
}

void dwarf_tuple(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  (void)dwarf;
  (void)ast;
  (void)g;
}

void dwarf_init(compile_t* c)
{
  dwarf_t* self;

  if(c->opt->symbols)
    printf("Emitting debug symbols\n");

  self = POOL_ALLOC(dwarf_t);
  memset(self, 0, sizeof(dwarf_t));

  self->symbols = symbols_init(0);
  self->optimized = c->opt->release;
  self->emit = c->opt->symbols;
  self->module = unwrap(c->module);
  self->layout = unwrap(c->target_data);
  self->builder = new DIBuilder(*unwrap(c->module));

  c->dwarf = self;
}

bool dwarf_finalise(dwarf_t* d)
{
  if(!d->emit)
    return true;

  d->builder->finalize();

  if(!d->unit.Verify())
  {
    errorf(NULL, "debug info verification failed!");
    return false;
  }

  return true;
}

void dwarf_cleanup(dwarf_t** d)
{
  dwarf_t* self = *d;

  if(!self->emit)
    return;

  symbols_destroy(&self->symbols);
  POOL_FREE(dwarf_t, self);

  *d = NULL;
}
