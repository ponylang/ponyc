#include <llvm/IR/Module.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/Support/Path.h>

#include "dwarf.h"

using namespace llvm;

void* LLVMGetDebugBuilder(LLVMModuleRef m)
{
  Module* module = unwrap(m);
  DIBuilder* d = new DIBuilder(*module);
  return (void*)d;
}

void LLVMCreateDebugCompileUnit(void* builder, int lang, const char* producer,
  const char* path, bool optimized)
{
  DIBuilder* d = (DIBuilder*)builder;

  d->createCompileUnit(lang, sys::path::filename(path),
    sys::path::parent_path(path), producer, optimized, StringRef(), 0);
}

void LLVMDebugInfoFinalize(void* builder)
{
  ((DIBuilder*)builder)->finalize();
}
