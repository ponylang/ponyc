#ifndef CODEGEN_GENTYPE_H
#define CODEGEN_GENTYPE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

typedef struct tbaa_metadata_t
{
  const char* name;
  LLVMValueRef metadata;
} tbaa_metadata_t;

DECLARE_HASHMAP(tbaa_metadatas, tbaa_metadatas_t, tbaa_metadata_t);

tbaa_metadatas_t* tbaa_metadatas_new();

void tbaa_metadatas_free(tbaa_metadatas_t* tbaa_metadatas);

LLVMValueRef tbaa_metadata_for_type(compile_t* c, ast_t* type);

LLVMValueRef tbaa_metadata_for_box_type(compile_t* c, const char* box_name);

bool gentypes(compile_t* c);

PONY_EXTERN_C_END

#endif
