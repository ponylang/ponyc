#ifndef CODEGEN_GENEXE_H
#define CODEGEN_GENEXE_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

LLVMValueRef gen_main(compile_t* c, reach_type_t* t_main, reach_type_t* t_env);

bool genexe(compile_t* c, ast_t* program);

#ifdef PLATFORM_IS_POSIX_BASED
/** The cross-toolchain sysroot the ELF linker auto-detects for a cross
 * build given no --sysroot (the four standard /usr/<triple> locations,
 * each validated by an arch-matching libc crt). Shared with the C shim
 * compiler so a cross build resolves the same sysroot whether it is
 * linking or compiling a shim. Returns the sysroot path, or NULL after
 * emitting the "requires --sysroot" error when none is found.
 */
const char* find_cross_toolchain_sysroot(pass_opt_t* opt, errors_t* errors);

/** Whether the target C library is musl.
 *
 * Musl exists only on Linux; the result is false for every other target. An
 * explicit --triple is authoritative and is never overridden. Otherwise the
 * libc that will actually be linked is identified directly: glibc ships
 * libc.so.6 alongside its crt startup objects and musl never does, so the
 * dynamic linker stamped into the executable stays consistent with the libc
 * it is linked against. Shared with the C shim compiler (gencshim) so a
 * build's interpreter choice and its shim header ordering agree. Not
 * memoized -- a handful of stat() calls, recomputed at each call site.
 */
bool target_libc_is_musl(pass_opt_t* opt);
#endif

PONY_EXTERN_C_END

#endif
