#ifndef CODEGEN_GENSPLIT_H
#define CODEGEN_GENSPLIT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool split_and_emit_bitcode(compile_t* c, const char*** out_files,
  size_t* out_count);

void cleanup_bc_files(const char** files, size_t count);

PONY_EXTERN_C_END

#endif
