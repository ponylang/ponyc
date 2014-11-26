#ifndef DEBUG_DWARF_H
#define DEBUG_DWARF_H

#include <platform.h>
#include "../codegen/codegen.h"

PONY_EXTERN_C_BEGIN

void dwarf_init(compile_t* c);
bool dwarf_finalise(dwarf_t* d);
void dwarf_cleanup(dwarf_t** d);

PONY_EXTERN_C_END

#endif
