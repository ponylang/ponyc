#ifndef CODEGEN_GENOPT_H
#define CODEGEN_GENOPT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool genopt(compile_t* c, bool pony_specific);
bool target_is_linux(char* triple);
bool target_is_bsd(char* triple);
bool target_is_freebsd(char* triple);
bool target_is_dragonfly(char* triple);
bool target_is_openbsd(char* triple);
bool target_is_macosx(char* triple);
bool target_is_windows(char* triple);
bool target_is_haiku(char* triple);
bool target_is_posix(char* triple);
bool target_is_x86(char* triple);
bool target_is_arm(char* triple);
bool target_is_arm32(char* triple);
bool target_is_ppc(char* triple);
bool target_is_riscv(char* triple);
bool target_is_lp64(char* triple);
bool target_is_llp64(char* triple);
bool target_is_ilp32(char* triple);
bool target_is_native128(char* triple);
bool target_is_bigendian(char* triple);
bool target_is_littleendian(char* triple);

// Target introspection shared by the executable linker (genexe) and the C
// shim compiler (gencshim). They take pass_opt_t rather than the target_is_*
// family's bare triple because system_triple also needs the interned-string
// table, and gencshim runs before a compile_t exists, so all three take opt.

// True if opt->triple names a target different from the host (different
// arch, OS, or environment). Native macOS is not cross even though the host
// triple says "darwin" and the target says "macosx".
bool is_cross_compiling(pass_opt_t* opt);

// True if the native host is actually musl. The vendored LLVM's default
// triple may claim "gnu" on musl systems, so for native builds this probes
// the filesystem for the musl loader instead of trusting the triple.
bool host_is_musl(pass_opt_t* opt);

// The arch-os-environment form distros use for multiarch directories
// (e.g. x86_64-linux-gnu), interned in opt->strtab.
const char* system_triple(pass_opt_t* opt);

PONY_EXTERN_C_END

#endif
