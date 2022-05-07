#ifndef CODEGEN_GENDEBUG_H
#define CODEGEN_GENDEBUG_H

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>

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

LLVMMetadataRef LLVMDIBuilderCreateNamespace(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, LLVMMetadataRef file,
  unsigned line);

LLVMMetadataRef LLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, LLVMMetadataRef file, unsigned line, unsigned col);

LLVMMetadataRef LLVMDIBuilderCreateMethod(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, const char* linkage,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type, LLVMValueRef func,
  int optimized);

LLVMMetadataRef LLVMDIBuilderCreateArtificialVariable(LLVMDIBuilderRef d,
  LLVMMetadataRef scope, const char* name, unsigned arg,
  LLVMMetadataRef file, unsigned line, LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateArtificialType(LLVMDIBuilderRef d,
  LLVMMetadataRef type);

LLVMMetadataRef LLVMDIBuilderCreateReplaceableStruct(LLVMDIBuilderRef d,
  const char* name, LLVMMetadataRef scope, LLVMMetadataRef file,
  unsigned line);

LLVMMetadataRef LLVMDIBuilderGetOrCreateArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length);

LLVMMetadataRef LLVMDIBuilderGetOrCreateTypeArray(LLVMDIBuilderRef d,
  LLVMMetadataRef* data, size_t length);

LLVMValueRef LLVMDIBuilderInsertDeclare(LLVMDIBuilderRef d,
  LLVMValueRef value, LLVMMetadataRef info, LLVMMetadataRef expr,
  unsigned line, unsigned col, LLVMMetadataRef scope, LLVMBasicBlockRef block);

PONY_EXTERN_C_END

#endif
