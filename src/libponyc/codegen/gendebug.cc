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
#if PONY_LLVM < 500
  DEFINE_ISA_CONVERSION_FUNCTIONS(Metadata, LLVMMetadataRef)

  inline Metadata** unwrap(LLVMMetadataRef* md)
  {
    return reinterpret_cast<Metadata**>(md);
  }
  #endif
}

using namespace llvm;

#if PONY_LLVM < 500
DEFINE_SIMPLE_CONVERSION_FUNCTIONS(DIBuilder, LLVMDIBuilderRef);
#endif

#if PONY_LLVM < 700
void LLVMMetadataReplaceAllUsesWith(LLVMMetadataRef md_old,
  LLVMMetadataRef md_new)
{
  MDNode* node = unwrap<MDNode>(md_old);
  node->replaceAllUsesWith(unwrap<Metadata>(md_new));
  MDNode::deleteTemporary(node);
}
#endif

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

#if PONY_LLVM < 600
void LLVMDIBuilderFinalize(LLVMDIBuilderRef d)
{
  unwrap(d)->finalize();
}

LLVMMetadataRef LLVMDIBuilderCreateCompileUnit(LLVMDIBuilderRef d,
  unsigned lang, const char* file, const char* dir, const char* producer,
  int optimized)
{
  DIBuilder* pd = unwrap(d);
  const StringRef flags = "";
  const unsigned runtimever = 0;

#if PONY_LLVM >= 400
  DIFile* difile = pd->createFile(file, dir);
  return wrap(pd->createCompileUnit(lang, difile, producer,
    optimized ? true : false, flags, runtimever));
#else
  return wrap(pd->createCompileUnit(lang, file, dir, producer, optimized ? true : false,
    flags, runtimever, StringRef())); // use the defaults
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateFile(LLVMDIBuilderRef d, const char* file)
{
  DIBuilder* pd = unwrap(d);

  StringRef filename = sys::path::filename(file);
  StringRef dir = sys::path::parent_path(file);

  return wrap(pd->createFile(filename, dir));
}
#endif

LLVMMetadataRef LLVMDIBuilderCreateNamespace(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file, unsigned line)
{
  DIBuilder* pd = unwrap(d);
#if PONY_LLVM >= 400
#  if PONY_LLVM >= 500
  (void)file;
  (void)line;
  return wrap(pd->createNameSpace(unwrap<DIScope>(scope), name, false));
#  else
  return wrap(pd->createNameSpace(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, false));
#  endif
#else
  return wrap(pd->createNameSpace(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line));
#endif
}

#if PONY_LLVM < 700
LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line, unsigned col)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLexicalBlock(
    unwrap<DILocalScope>(scope), unwrap<DIFile>(file), line, col));
}
#endif

LLVMMetadataRef LLVMDIBuilderCreateMethod(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, const char* linkage,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type, LLVMValueRef func,
  int optimized)
{
  DIBuilder* pd = unwrap(d);
  Function* f = unwrap<Function>(func);

#if PONY_LLVM >= 800
  DISubprogram::DISPFlags sp_flags = DISubprogram::toSPFlags(false, true,
    optimized ? true : false);

  DISubprogram* di_method = pd->createMethod(unwrap<DIScope>(scope),
    name, linkage, unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    0, 0, nullptr, DINode::FlagZero, sp_flags, nullptr, nullptr);
#elif PONY_LLVM >= 400
  DISubprogram* di_method = pd->createMethod(unwrap<DIScope>(scope),
    name, linkage, unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    false, true, 0, 0, 0, nullptr, DINode::FlagZero, optimized ? true : false);
#else
  DISubprogram* di_method = pd->createMethod(unwrap<DIScope>(scope),
    name, linkage, unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    false, true, 0, 0, 0, 0, optimized);
#endif

  f->setSubprogram(di_method);
  return wrap(di_method);
}

LLVMMetadataRef LLVMDIBuilderCreateAutoVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

#if PONY_LLVM >= 400
  return wrap(pd->createAutoVariable(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, unwrap<DIType>(type), true, DINode::FlagZero));
#else
  return wrap(pd->createAutoVariable(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, unwrap<DIType>(type), true, 0));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateParameterVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type)
{
  DIBuilder* pd = unwrap(d);

#if PONY_LLVM >= 400
  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, DINode::FlagZero));
#else
  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0));
#endif
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

#if PONY_LLVM < 700
LLVMMetadataRef LLVMDIBuilderCreateBasicType(LLVMDIBuilderRef d,
  const char* name, uint64_t size_bits, uint64_t align_bits,
  unsigned encoding)
{
  DIBuilder* pd = unwrap(d);

#if PONY_LLVM >= 400
  (void)(align_bits);
  return wrap(pd->createBasicType(name, size_bits, encoding));
#else
  return wrap(pd->createBasicType(name, size_bits, align_bits, encoding));
#endif
}
#endif

#if PONY_LLVM < 700
LLVMMetadataRef LLVMDIBuilderCreatePointerType(LLVMDIBuilderRef d,
  LLVMMetadataRef elem_type, uint64_t size_bits, uint64_t align_bits)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createPointerType(unwrap<DIType>(elem_type), size_bits,
    static_cast<uint32_t>(align_bits)));
}
#endif

#if PONY_LLVM < 700
LLVMMetadataRef LLVMDIBuilderCreateSubroutineType(LLVMDIBuilderRef d,
  LLVMMetadataRef file, LLVMMetadataRef param_types)
{
  DIBuilder* pd = unwrap(d);

  (void)file;
  return wrap(pd->createSubroutineType(
    DITypeRefArray(unwrap<MDTuple>(param_types))));
}

LLVMMetadataRef LLVMDIBuilderCreateStructType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_types)
{
  DIBuilder* pd = unwrap(d);

#if PONY_LLVM >= 400
  return wrap(pd->createStructType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits,
    static_cast<uint32_t>(align_bits), DINode::FlagZero, nullptr,
    elem_types ? DINodeArray(unwrap<MDTuple>(elem_types)) : nullptr));
#else
  return wrap(pd->createStructType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, align_bits, 0, nullptr,
    elem_types ? DINodeArray(unwrap<MDTuple>(elem_types)) : nullptr));
#endif
}
#endif

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

#if PONY_LLVM >= 400
  return wrap(pd->createMemberType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, static_cast<uint32_t>(align_bits),
    offset_bits, flags ? DINode::FlagPrivate : DINode::FlagZero,
    unwrap<DIType>(type)));
#else
  return wrap(pd->createMemberType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, align_bits,
    offset_bits, flags, unwrap<DIType>(type)));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateArrayType(LLVMDIBuilderRef d,
  uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_type, LLVMMetadataRef subscripts)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createArrayType(size_bits, static_cast<uint32_t>(align_bits),
    unwrap<DIType>(elem_type), DINodeArray(unwrap<MDTuple>(subscripts))));
}

#if PONY_LLVM < 700
LLVMMetadataRef LLVMDIBuilderGetOrCreateArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length)
{
  DIBuilder* pd = unwrap(d);
  Metadata** md = unwrap(data);
  ArrayRef<Metadata*> elems(md, length);

  DINodeArray a = pd->getOrCreateArray(elems);
  return wrap(a.get());
}

LLVMMetadataRef LLVMDIBuilderGetOrCreateTypeArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length)
{
  DIBuilder* pd = unwrap(d);
  Metadata** md = unwrap(data);
  ArrayRef<Metadata*> elems(md, length);

  DITypeRefArray a = pd->getOrCreateTypeArray(elems);
  return wrap(a.get());
}

LLVMMetadataRef LLVMDIBuilderCreateExpression(LLVMDIBuilderRef d,
  int64_t* addr, size_t length)
{
  DIBuilder* pd = unwrap(d);
  return wrap(pd->createExpression(ArrayRef<int64_t>(addr, length)));
}
#endif

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block)
{
  DIBuilder* pd = unwrap(d);
  MDNode* sc = unwrap<MDNode>(scope);

#if PONY_LLVM >= 1200
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
#else
  return wrap(pd->insertDeclare(unwrap(value),
    unwrap<DILocalVariable>(info), unwrap<DIExpression>(expr),
    DebugLoc::get(line, col, scope ? unwrap<MDNode>(scope) : nullptr, nullptr),
    unwrap(block)));
#endif
}

void LLVMSetCurrentDebugLocation2(LLVMBuilderRef b,
  unsigned line, unsigned col, LLVMMetadataRef scope)
{
#if PONY_LLVM >= 1200
  MDNode* sc = unwrap<MDNode>(scope);
  if (sc)
  {
    DILocation* loc = DILocation::get(sc->getContext(), line, col, sc,
      nullptr);
    unwrap(b)->SetCurrentDebugLocation(loc);
  }
#else
  unwrap(b)->SetCurrentDebugLocation(DebugLoc::get(line, col,
      scope ? unwrap<MDNode>(scope) : nullptr, nullptr));
#endif
}
