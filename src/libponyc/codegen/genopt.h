#ifndef CODEGEN_GENOPT_H
#define CODEGEN_GENOPT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool genopt(compile_t* c, bool pony_specific);
bool target_is_linux(char* triple);
bool target_is_musl(char* triple);
bool target_is_bsd(char* triple);
bool target_is_freebsd(char* triple);
bool target_is_dragonfly(char* triple);
bool target_is_openbsd(char* triple);
bool target_is_macosx(char* triple);
bool target_is_windows(char* triple);
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

PONY_EXTERN_C_END

#endif
