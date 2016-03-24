#include "gendebug.h"
#include "codegen.h"

#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#endif

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Path.h>

#define DW_TAG_auto_variable 0x100
#define DW_TAG_arg_variable 0x101

#if PONY_LLVM >= 307
namespace llvm
{
  DEFINE_ISA_CONVERSION_FUNCTIONS(Metadata, LLVMMetadataRef)

  inline Metadata** unwrap(LLVMMetadataRef* md)
  {
    return reinterpret_cast<Metadata**>(md);
  }
}
#endif

using namespace llvm;

DEFINE_SIMPLE_CONVERSION_FUNCTIONS(DIBuilder, LLVMDIBuilderRef);

void LLVMMetadataReplaceAllUsesWith(LLVMMetadataRef md_old,
  LLVMMetadataRef md_new)
{
#if PONY_LLVM >= 307
  MDNode* node = unwrap<MDNode>(md_old);
  node->replaceAllUsesWith(unwrap<Metadata>(md_new));
  MDNode::deleteTemporary(node);
#else
  (void)md_old;
  (void)md_new;
#endif
}

LLVMDIBuilderRef LLVMNewDIBuilder(LLVMModuleRef m)
{
#if PONY_LLVM >= 307
  Module* pm = unwrap(m);
  unsigned version = DEBUG_METADATA_VERSION;

  pm->addModuleFlag(Module::Warning, "Dwarf Version", version);
  pm->addModuleFlag(Module::Error, "Debug Info Version", version);

  return wrap(new DIBuilder(*pm));
#else
  (void)m;
  return NULL;
#endif
}

void LLVMDIBuilderDestroy(LLVMDIBuilderRef d)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);
  delete pd;
#else
  (void)d;
#endif
}

void LLVMDIBuilderFinalize(LLVMDIBuilderRef d)
{
#if PONY_LLVM >= 307
  unwrap(d)->finalize();
#else
  (void)d;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateCompileUnit(LLVMDIBuilderRef d,
  unsigned lang, const char* file, const char* dir, const char* producer,
  int optimized)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createCompileUnit(lang, file, dir, producer, optimized,
    StringRef(), 0, StringRef(), DIBuilder::FullDebug, 0, true));
#else
  (void)d;
  (void)lang;
  (void)file;
  (void)dir;
  (void)producer;
  (void)optimized;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateFile(LLVMDIBuilderRef d, const char* file)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  StringRef filename = sys::path::filename(file);
  StringRef dir = sys::path::parent_path(file);

  return wrap(pd->createFile(filename, dir));
#else
  (void)d;
  (void)file;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line, unsigned col)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLexicalBlock(
    unwrap<DILocalScope>(scope), unwrap<DIFile>(file), line, col));
#else
  (void)d;
  (void)scope;
  (void)file;
  (void)line;
  (void)col;
  return NULL;
#endif
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
    false, true, 0, 0, nullptr, 0, optimized);

  f->setSubprogram(di_method);
  return wrap(di_method);
#elif PONY_LLVM == 307
  DIBuilder* pd = unwrap(d);
  Function* f = unwrap<Function>(func);

  return wrap(pd->createMethod(unwrap<DIScope>(scope), name, linkage,
    unwrap<DIFile>(file), line, unwrap<DISubroutineType>(type),
    false, true, 0, 0, nullptr, 0, optimized, f));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)linkage;
  (void)file;
  (void)line;
  (void)type;
  (void)func;
  (void)optimized;
  return NULL;
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
#elif PONY_LLVM == 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_auto_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)file;
  (void)line;
  (void)type;
  return NULL;
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
#elif PONY_LLVM == 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_arg_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    unwrap<DIType>(type), true, 0, arg));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)arg;
  (void)file;
  (void)line;
  (void)type;
  return NULL;
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
#elif PONY_LLVM == 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createLocalVariable(DW_TAG_arg_variable,
    unwrap<DIScope>(scope), name, unwrap<DIFile>(file), line,
    pd->createArtificialType(unwrap<DIType>(type)),
    true, DINode::FlagArtificial, arg));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)arg;
  (void)file;
  (void)line;
  (void)type;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateBasicType(LLVMDIBuilderRef d,
  const char* name, uint64_t size_bits, uint64_t align_bits,
  unsigned encoding)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);
  return wrap(pd->createBasicType(name, size_bits, align_bits, encoding));
#else
  (void)d;
  (void)name;
  (void)size_bits;
  (void)align_bits;
  (void)encoding;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreatePointerType(LLVMDIBuilderRef d,
  LLVMMetadataRef elem_type, uint64_t size_bits, uint64_t align_bits)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createPointerType(unwrap<DIType>(elem_type), size_bits,
    align_bits));
#else
  (void)d;
  (void)elem_type;
  (void)size_bits;
  (void)align_bits;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateSubroutineType(LLVMDIBuilderRef d,
  LLVMMetadataRef file, LLVMMetadataRef param_types)
{
#if PONY_LLVM >= 308
  DIBuilder* pd = unwrap(d);

  (void)file;
  return wrap(pd->createSubroutineType(
    DITypeRefArray(unwrap<MDTuple>(param_types))));
#elif PONY_LLVM == 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createSubroutineType(unwrap<DIFile>(file),
    DITypeRefArray(unwrap<MDTuple>(param_types))));
#else
  (void)d;
  (void)file;
  (void)param_types;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateStructType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_types)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createStructType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, align_bits, 0, nullptr,
    elem_types ? DINodeArray(unwrap<MDTuple>(elem_types)) : nullptr));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)file;
  (void)line;
  (void)size_bits;
  (void)align_bits;
  (void)elem_types;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateReplaceableStruct(LLVMDIBuilderRef d,
  const char* name, LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createReplaceableCompositeType(dwarf::DW_TAG_structure_type,
    name, unwrap<DIScope>(scope), unwrap<DIFile>(file), line));
#else
  (void)d;
  (void)name;
  (void)scope;
  (void)file;
  (void)line;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateMemberType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  uint64_t offset_bits, unsigned flags, LLVMMetadataRef type)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createMemberType(unwrap<DIScope>(scope), name,
    unwrap<DIFile>(file), line, size_bits, align_bits,
    offset_bits, flags, unwrap<DIType>(type)));
#else
  (void)d;
  (void)scope;
  (void)name;
  (void)file;
  (void)line;
  (void)size_bits;
  (void)align_bits;
  (void)offset_bits;
  (void)flags;
  (void)type;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateArrayType(LLVMDIBuilderRef d,
  uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_type, LLVMMetadataRef subscripts)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->createArrayType(size_bits, align_bits,
    unwrap<DIType>(elem_type), DINodeArray(unwrap<MDTuple>(subscripts))));
#else
  (void)d;
  (void)size_bits;
  (void)align_bits;
  (void)elem_type;
  (void)subscripts;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderGetOrCreateArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);
  Metadata** md = unwrap(data);
  ArrayRef<Metadata*> elems(md, length);

  DINodeArray a = pd->getOrCreateArray(elems);
  return wrap(a.get());
#else
  (void)d;
  (void)data;
  (void)length;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderGetOrCreateTypeArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);
  Metadata** md = unwrap(data);
  ArrayRef<Metadata*> elems(md, length);

  DITypeRefArray a = pd->getOrCreateTypeArray(elems);
  return wrap(a.get());
#else
  (void)d;
  (void)data;
  (void)length;
  return NULL;
#endif
}

LLVMMetadataRef LLVMDIBuilderCreateExpression(LLVMDIBuilderRef d,
  int64_t* addr, size_t length)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);
  return wrap(pd->createExpression(ArrayRef<int64_t>(addr, length)));
#else
  (void)d;
  (void)addr;
  (void)length;
  return NULL;
#endif
}

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block)
{
#if PONY_LLVM >= 307
  DIBuilder* pd = unwrap(d);

  return wrap(pd->insertDeclare(unwrap(value),
    unwrap<DILocalVariable>(info), unwrap<DIExpression>(expr),
    DebugLoc::get(line, col, scope ? unwrap<MDNode>(scope) : nullptr, nullptr),
    unwrap(block)));
#else
  (void)d;
  (void)value;
  (void)info;
  (void)expr;
  (void)line;
  (void)col;
  (void)scope;
  (void)block;
  return NULL;
#endif
}

void LLVMSetCurrentDebugLocation2(LLVMBuilderRef b,
  unsigned line, unsigned col, LLVMMetadataRef scope)
{
#if PONY_LLVM >= 307
  unwrap(b)->SetCurrentDebugLocation(
    DebugLoc::get(line, col,
      scope ? unwrap<MDNode>(scope) : nullptr, nullptr));
#else
  (void)b;
  (void)line;
  (void)col;
  (void)scope;
#endif
}
