#ifndef CODEGEN_GENOPT_H
#define CODEGEN_GENOPT_H

#include <platform.h>
#include "codegen.h"

PONY_EXTERN_C_BEGIN

bool genopt(compile_t* c);
bool target_is_linux(char* triple);
bool target_is_freebsd(char* triple);
bool target_is_macosx(char* triple);
bool target_is_windows(char* triple);
bool target_is_posix(char* triple);
bool target_is_x86(char* triple);
bool target_is_arm(char* triple);
bool target_is_lp64(char* triple);
bool target_is_llp64(char* triple);
bool target_is_ilp32(char* triple);
bool target_is_native128(char* triple);


PONY_EXTERN_C_END

#endif
