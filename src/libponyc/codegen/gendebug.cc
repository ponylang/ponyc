#include "gendebug.h"
#include "codegen.h"

#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#  pragma warning(disable:4146)
#endif

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Path.h>

#define DW_TAG_auto_variable 0x100
#define DW_TAG_arg_variable 0x101

namespace llvm
{
  DEFINE_ISA_CONVERSION_FUNCTIONS(Metadata, LLVMMetadataRef)

  inline Metadata** unwrap(LLVMMetadataRef* md)
  {
    return reinterpret_cast<Metadata**>(md);
  }
}

using namespace llvm;

DEFINE_SIMPLE_CONVERSION_FUNCTIONS(DIBuilder, LLVMDIBuilderRef);

void LLVMMetadataReplaceAllUsesWith(LLVMMetadataRef md_old,
  LLVMMetadataRef md_new)
{
  MDNode* node = unwrap<MDNode>(md_old);
  node->replaceAllUsesWith(unwrap<Metadata>(md_new));
  MDNode::deleteTemporary(node);
}

LLVMDIBuilderRef LLVMNewDIBuilder(LLVMModuleRef m)
{
  Module* pm = unwrap(m);
  unsigned dwarf = dwarf::DWARF_VERSION;
  unsigned debug_info = DEBUG_METADATA_VERSION;

  pm->addModuleFlag(Module::Warning, "Dwarf Version", dwarf);
  pm->addModuleFlag(Module::Warning, "Debug Info Version", debug_info);

  return wrap(new DIBuilder(*pm));
}

void LLVMDIBuilderDestroy(LLVMDIBuilderRef d)
{
  DIBuilder* pd = unwrap(d);
  delete pd;
}

void LLVMDIBuilderFinalize(LLVMDIBuilderRef d)
{
  unwrap(d)->finalize();
}

LLVMMetadataRef LLVMDIBuilderCreateCompileUnit(LLVMDIBuilderRef d,
  unsigned lang, const char* file, const char* dir, const char* producer,
  int optimized)
{
#if PONY_LLVM >= 309
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createCompileUnit(lang, file, dir, producer, optimized,
    StringRef(), 0, StringRef())); // use the defaults
#else
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createCompileUnit(lang, file, dir, producer, optimized,
    StringRef(), 0, StringRef(), DIBuilder::FullDebug, 0, true));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateFile(LLVMDIBuilderRef d, const char* file)
{
  DIBuilder* pd = unwrap(d);

  StringRef filename = sys::path::filename(file);
  StringRef dir = sys::path::parent_path(file);

  return wrap(pd->createFile(filename, dir));
}

LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line, unsigned col)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLexicalBlock(
    unwrap<DILocalScope>(scope), unwrap<DIFile>(file), line, col));
}

LLVMMetadataRef LLVMDIBuilderCreateMethod(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, const char* linkage,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type, LLVMValueRef func,
  int optimized)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);
  Function* f = unwrap<Function>(func);

  DISubprogram* di_method = pd->createMethod(unwrap<DIScope>(scope),
    name, linkage, unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    false, true, 0, 0,
#  if PONY_LLVM >= 309
    0,
#  else
    nullptr,
#  endif
    0, optimized);

  f->setSubprogram(di_method);
  return wrap(di_method);
#else
  DIBuilder* pd = unwrap(d);
  Function* f = unwrap<Function>(func);

  return wrap(pd->createMethod(unwrap<DIScope>(scope), name, linkage,
    unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    false, true, 0, 0, nullptr, 0, optimized, f));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateAutoVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, LLVMMetadataRef type)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createAutoVariable(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, unwrap<DIType>(type), true, 0));
#else
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_auto_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateParameterVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0));
#else
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_arg_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0, arg));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateArtificialVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createParameterVariable(
    unwrap<DIScope>(scope), name, arg, unwrap<DIFile>(file), line,
    pd->createArtificialType(unwrap<DIType>(type)),
    true, DINode::FlagArtificial));
#else
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_arg_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    pd->createArtificialType(unwrap<DIType>(type)),
    true, DINode::FlagArtificial, arg));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateBasicType(LLVMDIBuilderRef d,
  const char* name, uint64_t size_bits, uint64_t align_bits,
  unsigned encoding)
{
  DIBuilder* pd = unwrap(d);
  return wrap(pd->createBasicType(name, size_bits, align_bits, encoding));
}

LLVMMetadataRef LLVMDIBuilderCreatePointerType(LLVMDIBuilderRef d,
  LLVMMetadataRef elem_type, uint64_t size_bits, uint64_t align_bits)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createPointerType(unwrap<DIType>(elem_type), size_bits,
    align_bits));
}

LLVMMetadataRef LLVMDIBuilderCreateSubroutineType(LLVMDIBuilderRef d,
  LLVMMetadataRef file, LLVMMetadataRef param_types)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);

  (void)file;
  return wrap(pd->createSubroutineType(
    DITypeRefArray(unwrap<MDTuple>(param_types))));
#else
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createSubroutineType(unwrap<DIFile>(file),
    DITypeRefArray(unwrap<MDTuple>(param_types))));
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateStructType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_types)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createStructType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, align_bits, 0, nullptr,
    elem_types ? DINodeArray(unwrap<MDTuple>(elem_types)) : nullptr));
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
    unwrap<DIFile>(file), line, size_bits, align_bits,
    offset_bits, flags, unwrap<DIType>(type)));
}

LLVMMetadataRef LLVMDIBuilderCreateArrayType(LLVMDIBuilderRef d,
  uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_type, LLVMMetadataRef subscripts)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createArrayType(size_bits, align_bits,
    unwrap<DIType>(elem_type), DINodeArray(unwrap<MDTuple>(subscripts))));
}

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

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block)
{
  DIBuilder* pd = unwrap(d);

  return wrap(pd->insertDeclare(unwrap(value),
    unwrap<DILocalVariable>(info), unwrap<DIExpression>(expr),
    DebugLoc::get(line, col, scope ? unwrap<MDNode>(scope) : nullptr, nullptr),
    unwrap(block)));
}

void LLVMSetCurrentDebugLocation2(LLVMBuilderRef b,
  unsigned line, unsigned col, LLVMMetadataRef scope)
{
  unwrap(b)->SetCurrentDebugLocation(
    DebugLoc::get(line, col,
      scope ? unwrap<MDNode>(scope) : nullptr, nullptr));
}
