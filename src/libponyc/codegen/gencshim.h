#ifndef CODEGEN_GENCSHIM_H
#define CODEGEN_GENCSHIM_H

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
 * `use "cdefine:..."` and `use "cincludedir:..."` directives become -D and -I
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
bool gencshim(ast_t* program, pass_opt_t* opt);

PONY_EXTERN_C_END

#ifdef __cplusplus
#include <vector>

// Append the cc1 target/ABI arguments (triple, CPU, target features, ABI, PIC)
// derived from the pass options to `args`. C++ linkage (not part of the C entry
// point above) so CShimTest can assert the exact argv — e.g. that an empty CPU
// omits -target-cpu and a comma-separated feature string round-trips to one
// -target-feature each. Defined in gencshim.cc.
void cshim_target_args(pass_opt_t* opt, std::vector<const char*>& args);
#endif

#endif
