#include "dwarf.h"
#include "symbols.h"
#include "../ast/error.h"
#include "../pkg/package.h"
#include "../type/subtype.h"
#include "../../libponyrt/mem/pool.h"
#include "../../libponyrt/pony.h"

#include <platform.h>

#define OFFSET_CLASS (sizeof(void*) * 8)
#define OFFSET_ACTOR (sizeof(pony_actor_pad_t) * 8)

using namespace llvm;
using namespace llvm::dwarf;

typedef struct frame_t frame_t;

struct frame_t
{
  size_t size;
  subnodes_t* members;
  frame_t* prev;
};

struct dwarf_t
{
  symbols_t* symbols;
  DataLayout* layout;
  frame_t* frame;
};

/**
 * Every call to dwarf_forward causes a frame_t to be pushed onto
 * the stack.
 */
static frame_t* push_frame(dwarf_t* dwarf)
{
  frame_t* frame = POOL_ALLOC(frame_t);
  memset(frame, 0, sizeof(frame_t));

  frame->prev = dwarf->frame;
  dwarf->frame = frame;

  return frame;
}

/**
 * Every call to dwarf_composite causes a frame_t to be popped from
 * the stack.
 */
static void pop_frame(dwarf_t* dwarf)
{
  frame_t* frame = dwarf->frame;
  dwarf->frame = frame->prev;

  POOL_FREE(frame_t, frame);
}

/**
 * Collect type information such as file and line scope. The definition
 * of tuple types is lexically unscoped, because tuple type names are
 * ambiguous.
 */
static void setup_dwarf(dwarf_t* dwarf, gentype_t* g, LLVMTypeRef typeref, 
  symbol_scope_t* scope, bool definition, bool prelim)
{
  ast_t* ast = g->ast;
  Type* type = unwrap(typeref);

  memset(scope, 0, sizeof(symbol_scope_t));
   
  if(definition && ast_id(ast) != TK_TUPLETYPE)
  {
    ast = (ast_t*)ast_data(ast);
    ast_t* module = ast_nearest(ast, TK_MODULE);
    source_t* source = (source_t*)ast_data(module);

    scope->file = symbols_file(dwarf->symbols, source->file); 
  } 

  if(!prelim)
  {
    g->size = dwarf->layout->getTypeSizeInBits(type);
    g->align = dwarf->layout->getABITypeAlignment(type) << 3;
  }

  scope->line = ast_line(ast);
  scope->pos = ast_pos(ast);
}

void dwarf_compileunit(dwarf_t* dwarf, ast_t* program)
{
  assert(ast_id(program) == TK_PROGRAM);
  ast_t* package = ast_child(program);

  const char* path = package_path(package);
  const char* name = package_filename(package);

  symbols_package(dwarf->symbols, path, name);
}

void dwarf_forward(dwarf_t* dwarf, gentype_t* g)
{
  if(!symbols_known_type(dwarf->symbols, g->type_name))
  {
    frame_t* frame = push_frame(dwarf);
    
    LLVMTypeRef typeref = g->structure;
    size_t size = g->field_count;
    
    // The field count for non-tuple types does not contain
    // the methods, which in the dwarf world are subnodes
    // just like fields.
    if(g->underlying != TK_TUPLETYPE)
    {
      ast_t* def = (ast_t*)ast_data(g->ast);
      size += ast_childcount(ast_childidx(def, 4)) - size;
    } else {
      typeref = g->primitive;
    }
      
    frame->size = size;
    
    symbol_scope_t scope;
    setup_dwarf(dwarf, g, typeref, &scope, true, true);

    symbols_declare(dwarf->symbols, g, &frame->members, size, &scope);
  }
}

void dwarf_basic(dwarf_t* dwarf, gentype_t* g)
{
  // Basic types are builtin, hence have no compilation
  // unit scope and their size and ABI alignment depends
  // on the primitive structure.
  symbol_scope_t scope;
  setup_dwarf(dwarf, g, g->primitive, &scope, false, false);

  symbols_basic(dwarf->symbols, g);
}

void dwarf_pointer(dwarf_t* dwarf, gentype_t* ptr, gentype_t* g)
{
  symbol_scope_t scope;
  setup_dwarf(dwarf, ptr, ptr->use_type, &scope, false, false);

  symbols_pointer(dwarf->symbols, ptr, g);
}

void dwarf_trait(dwarf_t* dwarf, gentype_t* g)
{
  // Trait definitions have a scope, but are modeled
  // as opaque classes from which other classes may
  // inherit. There is no need to set the size and
  // align to 0, because gentype_t was memset.
  symbol_scope_t scope;
  setup_dwarf(dwarf, g, g->use_type, &scope, true, false);

  symbols_trait(dwarf->symbols, g, &scope);
}

void dwarf_composite(dwarf_t* dwarf, gentype_t* g)
{
  size_t offset = 0;

  switch(g->underlying)
  {
    case TK_ACTOR: offset = OFFSET_ACTOR;
    case TK_PRIMITIVE:
    case TK_CLASS: offset += OFFSET_CLASS;
    default: {}
  }

  LLVMTypeRef typeref = g->structure;

  if(g->underlying == TK_TUPLETYPE)
    typeref = g->primitive;
  
  symbol_scope_t scope;
  setup_dwarf(dwarf, g, typeref, &scope, true, false);

  symbols_composite(dwarf->symbols, g, offset, dwarf->frame->members,
    &scope);

  pop_frame(dwarf);
}

void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field,
  size_t index)
{
  char buf[32];
  memset(buf, 0, sizeof(buf));

  const char* name = NULL;
  
  bool is_private = false;
  bool constant = false;

  if(composite->underlying != TK_TUPLETYPE)
  {
    ast_t* def = (ast_t*)ast_data(composite->ast);
    ast_t* members = ast_childidx(def, 4);
    ast_t* field = ast_childidx(members, index);

    assert(ast_id(field) == TK_FVAR || ast_id(field) == TK_FLET);

    if(ast_id(field) == TK_FLET)
      constant = true;

    name = ast_name(ast_child(field));
    is_private = name[0] == '_';
  } else {
    snprintf(buf, sizeof(buf), "_" __zu, index);
    name = buf;
  }

  symbol_scope_t scope;
  setup_dwarf(dwarf, field, field->use_type, &scope, false, false);

  symbols_member(dwarf->symbols, field, dwarf->frame->members, &scope,
    name, is_private, constant, index);
}

void dwarf_init(compile_t* c)
{
  c->dwarf = POOL_ALLOC(dwarf_t);
  c->dwarf->symbols = symbols_init(c);
  c->dwarf->layout = unwrap(c->target_data);
  c->dwarf->frame = NULL;
}

void dwarf_finalise(dwarf_t* dwarf)
{
  symbols_finalise(dwarf->symbols);
  POOL_FREE(dwarf_t, dwarf);
}
