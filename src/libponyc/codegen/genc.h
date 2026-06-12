#ifndef CODEGEN_GENC_H
#define CODEGEN_GENC_H

#include <platform.h>
#include "../ast/ast.h"
#include "../pass/pass.h"

PONY_EXTERN_C_BEGIN

/**
 * Compile every package's C shim sources (PASS_C).
 *
 * Walks the program's packages and, for each package with .c sources
 * discovered next to its Pony files, compiles each source to an object file
 * in the output directory using the embedded clang. The compile targets the
 * same triple/cpu/features the Pony codegen will use (from pass_opt_t), so
 * this needs nothing from compile_t and runs before codegen. Each package's
 * `use "cdefine:..."` and `use "cinclude:..."` directives become -D and -I
 * arguments for that package's sources only. Object paths are recorded on
 * the program (program_add_c_object) for the platform linkers to append.
 *
 * Clang diagnostics are routed into opt->check.errors, so a broken shim
 * fails the build like any other compile error. A clang crash or internal
 * error is contained per translation unit (LLVM CrashRecoveryContext) and
 * reported as an error attributed to the shim, instead of taking the ponyc
 * process down.
 *
 * Returns true when every shim compiled (or there were none).
 */
bool genc(ast_t* program, pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
