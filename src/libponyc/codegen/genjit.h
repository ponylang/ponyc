#ifndef CODEGEN_GENJIT_H
#define CODEGEN_GENJIT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

typedef struct
{
  const char* name;
  void** address;
} jit_symbol_t;

// JIT a program and start the Pony runtime.
// Should only be used for compiler tests.
// For each element in the `symbols` array, the symbol address will be fetched
// from the generated program and be assigned to the value pointed by the
// `address` field.
bool gen_jit_and_run(compile_t* c, int* exit_code, jit_symbol_t* symbols,
  size_t symbol_count);

PONY_EXTERN_C_END

#endif
