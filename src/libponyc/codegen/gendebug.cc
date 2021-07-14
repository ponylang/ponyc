#include "codegen.h"
#include "gendebug.h"

#include "llvm_config_begin.h"

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Path.h>

#include "llvm_config_end.h"

#define DW_TAG_auto_variable 0x100
#define DW_TAG_arg_variable 0x101

namespace llvm
{
}

using namespace llvm;

LLVMDIBuilderRef LLVMNewDIBuilder(LLVMModuleRef m)
{
  Module* pm = unwrap(m);

#ifdef _MSC_VER
  pm->addModuleFlag(Module::Warning, "CodeView", 1);
#else
  unsigned dwarf = dwarf::DWARF_VERSION;
  unsigned debug_info = DEBUG_METADATA_VERSION;

  pm->addModuleFlag(Module::Warning, "Dwarf Version", dwarf);
  pm->addModuleFlag(Module::Warning, "Debug Info Version", debug_info);
#endif

  return wrap(new DIBuilder(*pm));
}

void LLVMDIBuilderDestroy(LLVMDIBuilderRef d)
{
  DIBuilder* pd = unwrap(d);
  delete pd;
}

LLVMMetadataRef LLVMDIBuilderCreateNamespace(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file, unsigned line)
{
  DIBuilder* pd = unwrap(d);
  (void)file;
  (void)line;
  return wrap(pd->createNameSpace(unwrap<DIScope>(scope), name, false));
}

LLVMMetadataRef LLVMDIBuilderCreateMethod(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, const char* linkage,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type, LLVMValueRef func,
  int optimized)
{
  DIBuilder* pd = unwrap(d);
  Function* f = unwrap<Function>(func);

  DISubprogram::DISPFlags sp_flags = DISubprogram::toSPFlags(false, true,
    optimized ? true : false);

  DISubprogram* di_method = pd->createMethod(unwrap<DIScope>(scope),
    name, linkage, unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    0, 0, nullptr, DINode::FlagZero, sp_flags, nullptr, nullptr);

  f->setSubprogram(di_method);
  return wrap(di_method);
}

LLVMMetadataRef LLVMDIBuilderCreateAutoVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createAutoVariable(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, unwrap<DIType>(type), true, DINode::FlagZero));
}

LLVMMetadataRef LLVMDIBuilderCreateParameterVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, DINode::FlagZero));
}

LLVMMetadataRef LLVMDIBuilderCreateArtificialVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    pd->createArtificialType(unwrap<DIType>(type)),
    true, DINode::FlagArtificial));
}

LLVMMetadataRef LLVMDIBuilderCreateReplaceableStruct(LLVMDIBuilderRef d,
  const char* name, LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createReplaceableCompositeType(dwarf::DW_TAG_structure_type,
    name, unwrap<DIScope>(scope), unwrap<DIFile>(file), line));
}

LLVMMetadataRef LLVMDIBuilderCreateMemberType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  uint64_t offset_bits, unsigned flags, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createMemberType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, static_cast<uint32_t>(align_bits),
    offset_bits, flags ? DINode::FlagPrivate : DINode::FlagZero,
    unwrap<DIType>(type)));
}

LLVMMetadataRef LLVMDIBuilderCreateArrayType(LLVMDIBuilderRef d,
  uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_type, LLVMMetadataRef subscripts)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createArrayType(size_bits, static_cast<uint32_t>(align_bits),
    unwrap<DIType>(elem_type), DINodeArray(unwrap<MDTuple>(subscripts))));
}

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block)
{
  DIBuilder* pd = unwrap(d);
  MDNode* sc = unwrap<MDNode>(scope);

  if (sc)
  {
    DILocation* loc = DILocation::get(sc->getContext(), line, col, sc,
      nullptr);

    Instruction* inst = pd->insertDeclare(unwrap(value),
      unwrap <DILocalVariable>(info), unwrap<DIExpression>(expr), loc,
      unwrap(block));

    return wrap(inst);
  }
  else
  {
    return nullptr;
  }
}

void LLVMSetCurrentDebugLocation2(LLVMBuilderRef b,
  unsigned line, unsigned col, LLVMMetadataRef scope)
{
  MDNode* sc = unwrap<MDNode>(scope);
  if (sc)
  {
    DILocation* loc = DILocation::get(sc->getContext(), line, col, sc,
      nullptr);
    unwrap(b)->SetCurrentDebugLocation(loc);
  }
}
