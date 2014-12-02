#ifndef DEBUG_DWARF_H
#define DEBUG_DWARF_H

#include <platform.h>
#include "../codegen/codegen.h"
#include "../codegen/gentype.h"

PONY_EXTERN_C_BEGIN

void dwarf_compileunit(dwarf_t* dwarf, ast_t* package);
void dwarf_nominal(dwarf_t* dwarf, ast_t* ast, gentype_t* g);
void dwarf_pointer(dwarf_t* dwarf, ast_t* ast, gentype_t* g);

void dwarf_init(compile_t* c);
bool dwarf_finalise(dwarf_t* d);
void dwarf_cleanup(dwarf_t** d);

PONY_EXTERN_C_END

#endif
