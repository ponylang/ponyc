#ifndef CODEGEN_GENDEBUG_H
#define CODEGEN_GENDEBUG_H

#include <platform.h>
#include <llvm-c/Core.h>

PONY_EXTERN_C_BEGIN

enum
{
  DW_ATE_boolean = 0x02,
  DW_ATE_float = 0x04,
  DW_ATE_signed = 0x05,
  DW_ATE_unsigned = 0x07,
};

enum
{
  DW_FLAG_Private           = 1,
  DW_FLAG_Protected         = 2,
  DW_FLAG_Public            = 3,
  DW_FLAG_FwdDecl           = 1 << 2,
  DW_FLAG_AppleBlock        = 1 << 3,
  DW_FLAG_BlockByrefStruct  = 1 << 4,
  DW_FLAG_Virtual           = 1 << 5,
  DW_FLAG_Artificial        = 1 << 6,
  DW_FLAG_Explicit          = 1 << 7,
  DW_FLAG_Prototyped        = 1 << 8,
  DW_FLAG_ObjcClassComplete = 1 << 9,
  DW_FLAG_ObjectPointer     = 1 << 10,
  DW_FLAG_Vector            = 1 << 11,
  DW_FLAG_StaticMember      = 1 << 12,
  DW_FLAG_LValueReference   = 1 << 13,
  DW_FLAG_RValueReference   = 1 << 14,
  DW_FLAG_ExternalTypeRef   = 1 << 15,
};

typedef struct LLVMOpaqueMetadata* LLVMMetadataRef;

typedef struct LLVMOpaqueDIBuilder* LLVMDIBuilderRef;

void LLVMMetadataReplaceAllUsesWith(LLVMMetadataRef md_old,
  LLVMMetadataRef md_new);

LLVMDIBuilderRef LLVMNewDIBuilder(LLVMModuleRef m);

void LLVMDIBuilderDestroy(LLVMDIBuilderRef d);

void LLVMDIBuilderFinalize(LLVMDIBuilderRef d);

LLVMMetadataRef LLVMDIBuilderCreateCompileUnit(LLVMDIBuilderRef d,
  unsigned lang, const char* file, const char* dir, const char* producer,
  int optimized);

LLVMMetadataRef LLVMDIBuilderCreateFile(LLVMDIBuilderRef d, const char* file);

LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line, unsigned col);

LLVMMetadataRef LLVMDIBuilderCreateMethod(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, const char* linkage,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type, LLVMValueRef func,
  int optimized);

LLVMMetadataRef LLVMDIBuilderCreateAutoVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateParameterVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateArtificialVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateBasicType(LLVMDIBuilderRef d,
  const char* name, uint64_t size_bits, uint64_t align_bits,
  unsigned encoding);

LLVMMetadataRef LLVMDIBuilderCreatePointerType(LLVMDIBuilderRef d,
  LLVMMetadataRef elem_type, uint64_t size_bits, uint64_t align_bits);

LLVMMetadataRef LLVMDIBuilderCreateArtificialType(LLVMDIBuilderRef d,
  LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateSubroutineType(LLVMDIBuilderRef d,
  LLVMMetadataRef file, LLVMMetadataRef param_types);

LLVMMetadataRef LLVMDIBuilderCreateStructType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_types);

LLVMMetadataRef LLVMDIBuilderCreateReplaceableStruct(LLVMDIBuilderRef d,
  const char* name, LLVMMetadataRef scope, LLVMMetadataRef file,
  unsigned line);

LLVMMetadataRef LLVMDIBuilderCreateMemberType(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line, uint64_t size_bits, uint64_t align_bits,
  uint64_t offset_bits, unsigned flags, LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateArrayType(LLVMDIBuilderRef d,
  uint64_t size_bits, uint64_t align_bits,
  LLVMMetadataRef elem_type, LLVMMetadataRef subscripts);

LLVMMetadataRef LLVMDIBuilderGetOrCreateArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length);

LLVMMetadataRef LLVMDIBuilderGetOrCreateTypeArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length);

LLVMMetadataRef LLVMDIBuilderCreateExpression(LLVMDIBuilderRef d,
  int64_t* addr, size_t length);

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block);

void LLVMSetCurrentDebugLocation2(LLVMBuilderRef b,
  unsigned line, unsigned col, LLVMMetadataRef scope);

PONY_EXTERN_C_END

#endif
