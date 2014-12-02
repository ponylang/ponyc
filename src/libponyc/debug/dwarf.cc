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
#include "../../libponyrt/mem/pool.h"

#define DW_LANG_Pony 0x8002
#define PRODUCER "ponyc"
#define MEMBER_OFFSET sizeof(void*) << 3

using namespace llvm;

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

void dwarf_compileunit(dwarf_t* dwarf, ast_t* package)
{
  if(!dwarf->emit)
    return;

  const char* file = package_filename(package);
  const char* path = package_path(package);

  DIBuilder* builder = dwarf->builder;

  dwarf->unit = builder->createCompileUnit(DW_LANG_Pony, file, path, PRODUCER,
    dwarf->optimized, StringRef(), 0);
}

void dwarf_nominal(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  (void)ast;
  (void)g;
}

void dwarf_pointer(dwarf_t* dwarf, ast_t* ast, gentype_t* g)
{
  if(!dwarf->emit)
    return;

  (void)ast;
  (void)g;
}

void dwarf_init(compile_t* c)
{
  dwarf_t* self;

  if(c->symbols)
    printf("Emitting debug symbols\n");

  self = POOL_ALLOC(dwarf_t);
  memset(self, 0, sizeof(dwarf_t));

  self->symbols = symbols_init(0);
  self->optimized = c->release;
  self->emit = c->symbols;
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
