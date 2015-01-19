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

struct dwarf_t
{
  symbols_t* symbols;
  bool emit;
};

typedef struct dwarf_type_t
{
  DIFile file;
  size_t line;
  size_t pos;
} dwarf_type_t;

struct dwarf_composite_t
{
  size_t size;
  MDNode** members;
};

/**
 * Collect type information such as file and line scope. Tuple types are 
 * unscoped, because their declaration may be ambiguous.
 */
static void setup_dwarf(dwarf_t* dwarf, ast_t* ast, dwarf_type_t* type)
{
  memset(type, 0, sizeof(dwarf_type_t));

  if(g->underlying != TK_TUPLETYPE)
  {
    ast_t* module = ast_nearest(ast, TK_MODULE);
    assert(module != NULL);

    type->file = symbols_file(dwarf->symbols, module);
    type->line = ast_line(ast);
    type->pos = ast_pos(ast);
  }
}

void dwarf_compileunit(dwarf_t* dwarf, ast_t* ast)
{
  if(!dwarf->emit) 
    return;

  assert(ast_id(ast) == TK_PROGRAM);

  symbols_compileunit(dwarf->symbols, ast_child(ast), dwarf->optimized);
}

void dwarf_forward(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  /*symbol_t* s = symbols_get(dwarf->symbols, g->type_name, false);
  dbg_symbol_t* d = s->dbg;

  if(d->type == NULL)
  {
    DIBuilder* builder = dwarf->builder;

    g->dwarf = POOL_ALLOC(dwarf_composite_t);
    memset(g->dwarf, 0, sizeof(dwarf_composite_t));

    size_t n = g->field_count;

    if(g->underlying != TK_TUPLETYPE)
      n = ast_childcount(ast_childidx(ast, 4));

    if(n > 0)
    {
      g->dwarf->size = n;
      g->dwarf->members = (MDNode**)calloc(n, sizeof(MDNode*));
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
  }*/
}

void dwarf_basic(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  dwarf_type_t type;
  setup_dwarf(dwarf, ast, &type);

  symbols_basic(dwarf->symbols, g, &type);
  /*DIType basic = dwarf->builder->createBasicType(g->type_name, info.size,
    info.align, type);

  dwarf->builder->createQualifiedType(DW_TAG_constant, basic);*/
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

void dwarf_trait(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  const char* name = ast_name(ast_child(ast));
  
  dwarf_type_t type;
  setup_dwarf(dwarf, ast, &type);

  symbols_trait(dwarf->symbols, g, &type)
  /*dwarf->builder->createClassType(dwarf->unit, name, info.file,
    (int)info.line, info.size, info.align, 0, 0, DIType(), DIArray());*/
}

void dwarf_composite(dwarf_t* dwarf, ast_t* def, gentype_t* g)
{
  /*if(!dwarf->emit)
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
  //d->qualified = builder->createQualifiedType(DW_TAG_const_type, d->type);*/
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
  if(c->opt->symbols)
    printf("Emitting debug symbols\n");

  c->dwarf = POOL_ALLOC(dwarf_t);
  memset(c->dwarf, 0, sizeof(dwarf_t));

  c->dwarf->symbols = symbols_init(c);
  c->dwarf->emit = c->opt->symbols;
}

void dwarf_finalise(dwarf_t* dwarf)
{
  symbols_finalise(dwarf->symbols, dwarf->emit);
  POOL_FREE(dwarf_t, dwarf);
}
