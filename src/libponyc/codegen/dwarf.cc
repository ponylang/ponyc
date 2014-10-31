#include "dwarf.h"
#include "../pkg/package.h"
#include "../ast/source.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Support/Path.h>

#define DW_LANG_Pony 0x8002
#define PRODUCER "ponyc"

using namespace llvm;

static void mdobject(compile_t* c, DIBuilder* builder, ast_t* actor)
{

}

static void dwarf_entity(compile_t* c, DIBuilder* builder, ast_t* entity)
{
  switch(ast_id(entity))
  {
    case TK_CLASS:
    case TK_ACTOR:
      mdobject(c, builder, entity);
      break;
    default:
      return;
  }
}

void dwarf_program(compile_t* c, ast_t* program)
{
  ast_t* package = NULL;
  ast_t* module = NULL;
  size_t packages = ast_childcount(program);

  ast_t* main = ast_child(program);
  const char* file = ((source_t*)ast_data(main))->file;

  DIBuilder* builder = new DIBuilder(*unwrap(c->module));

  builder->createCompileUnit(DW_LANG_Pony, sys::path::filename(file),
    sys::path::parent_path(file), PRODUCER, c->release, StringRef(), 0);

  for(size_t i = 0; i < packages; i++)
  {
    package = ast_childidx(program, i);

    for(size_t j = 0; j < ast_childcount(package); j++)
    {
      module = ast_childidx(package, j);

      for(size_t k = 0; k < ast_childcount(module); k++)
      {
        dwarf_entity(c, builder, ast_childidx(module, k));
      }
    }
  }

  builder->finalize();
}
