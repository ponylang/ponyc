#include "dwarf.h"
#include "../pkg/package.h"
#include "../ast/source.h"
#include <string.h>
#include <stdio.h>

#define DW_LANG_Pony 0x8002
#define PRODUCER "ponyc"

typedef struct
{
  void* builder;
} dwarf_t;

static void mdobject(compile_t* c, dwarf_t* dwarf, ast_t* actor)
{

}

static void dwarf_entity(compile_t* c, dwarf_t* dwarf, ast_t* entity)
{
  switch(ast_id(entity))
  {
    /*case TK_TYPE:
      mdtype(c, dwarf, entity);
      break;
    case TK_PRIMITIVE:
      mdprimitive(c, dwarf, entity);
      break;
    case TK_TRAIT:
      mdtrait(c, dwarf, entity);
      break;*/
    case TK_CLASS:
    case TK_ACTOR:
      mdobject(c, dwarf, entity);
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

  dwarf_t dwarf;
  dwarf.builder = LLVMGetDebugBuilder(c->module);

  LLVMCreateDebugCompileUnit(dwarf.builder, DW_LANG_Pony,PRODUCER, file,
    c->release);

  for(size_t i = 0; i < packages; i++)
  {
    package = ast_childidx(program, i);

    for(size_t j = 0; j < ast_childcount(package); j++)
    {
      module = ast_childidx(package, j);

      for(size_t k = 0; k < ast_childcount(module); k++)
      {
        dwarf_entity(c, &dwarf, ast_childidx(module, k));
      }
    }
  }

  LLVMDebugInfoFinalize(dwarf.builder);
}
