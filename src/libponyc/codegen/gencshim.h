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
#include "../reach/reach.h"
#include "codegen.h"
#include "../ast/printbuf.h"

// Append the cc1 target/ABI arguments (triple, CPU, target features, ABI, PIC)
// derived from the pass options to `args`. C++ linkage (not part of the C entry
// point above) so CShimTest can assert the exact argv — e.g. that an empty CPU
// omits -target-cpu and a comma-separated feature string round-trips to one
// -target-feature each. Defined in gencshim.cc.
void cshim_target_args(pass_opt_t* opt, std::vector<const char*>& args);

// Write C source for a by-value FFI thunk into buf. The thunk wraps f_name,
// accepting pointers for struct parameters and converting to/from by-value
// passing. Returns false and reports through errors on unsupported field types.
bool generate_thunk_source(printbuf_t* buf, const char* f_name,
  reach_type_t** param_types, size_t param_count,
  reach_type_t* ret_type, bool ret_is_struct,
  errors_t* errors, strtable_t* strtab);

// Compile C source to an LLVM module using the embedded clang, sharing the
// LLVMContext of c. Returns NULL on failure; errors are reported through
// opt->check.errors.
LLVMModuleRef compile_ffi_thunk(compile_t* c, pass_opt_t* opt,
  const char* source);
#endif

#endif
