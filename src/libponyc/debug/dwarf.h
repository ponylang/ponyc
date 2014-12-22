#ifndef DEBUG_DWARF_H
#define DEBUG_DWARF_H

#include <platform.h>
#include "../ast/ast.h"
#include "../codegen/codegen.h"
#include "../codegen/gentype.h"

PONY_EXTERN_C_BEGIN

void dwarf_compileunit(dwarf_t* dwarf, ast_t* program);

void dwarf_forward(dwarf_t* dwarf, ast_t* ast, gentype_t* g);

void dwarf_basic(dwarf_t* dwarf, gentype_t* g);

void dwarf_pointer(dwarf_t* dwarf, gentype_t* g);

/**
 * Traits are dwarfed as composite types with no members, the actual debug
 * symbols for the underlying type are parsed at debug time. Symbols for traits
 * are only created once per type (not type instance). There is no need to
 * forward declare a trait dwarf symbol ever.
 */
void dwarf_trait(dwarf_t* dwarf, ast_t* def, gentype_t* g);

void dwarf_composite(dwarf_t* dwarf, ast_t* def, gentype_t* g);

void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field);

void dwarf_tuple(dwarf_t* dwarf, ast_t* ast, gentype_t* g);

void dwarf_init(compile_t* c);

bool dwarf_finalise(dwarf_t* d);

void dwarf_cleanup(dwarf_t** d);

PONY_EXTERN_C_END

#endif
