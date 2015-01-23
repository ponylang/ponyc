#ifndef DEBUG_DWARF_H
#define DEBUG_DWARF_H

#include <platform.h>
#include "../ast/ast.h"
#include "../codegen/codegen.h"
#include "../codegen/gentype.h"

PONY_EXTERN_C_BEGIN

/**
 * Emit debug symbols for a compilation unit. For Pony, there is always a
 * single compilation unit. The file scope for a pony program compilation
 * unit is the package and module where the main actor resides. For Pony
 * libraries the file scope is undefined.
 */
void dwarf_compileunit(dwarf_t* dwarf, ast_t* program);

/**
 * Emit debug symbols for a Pony basic type: U8, ..., U128, I8, ..., I128,
 * Bool, F32, F64.
 */
void dwarf_basic(dwarf_t* dwarf, gentype_t* g);

/**
 * Emit debug symbols for a pointer to some Pony nominal type. Note that
 * this can be Pointer[A], which would be dwarfed as A**. Pointer[U64]
 * instead is dwarfed as U64*. Consequently, the usage of any nominal
 * type (or boxed instances of tuples or primitive builtins) is dwarfed
 * as a simple pointer to type.
 */
void dwarf_pointer(dwarf_t* dwarf, gentype_t* ptr, gentype_t* g);

/**
 * Traits are dwarfed as composite types with no members, the actual debug
 * symbols for the underlying type are parsed at debug time. Symbols for traits
 * are only created once per type (not type instance). There is no need to
 * forward declare a trait dwarf symbol ever.
 */
void dwarf_trait(dwarf_t* dwarf, gentype_t* g);

/**
 * Forward declare a debug symbol for some composite type (tuple or nominal).
 * Once the underlying gentype_t becomes final (i.e. gentype is called without
 * 'prelim' being true), the debug symbols need to be finalised with a call
 * to dwarf_composite. If debug symbols for composite types are not finalised,
 * the symbols will not be in the final binary. The same applies for any
 * symbols that depend on the forward declared dwarf entry. Forward declaring
 * a symbol is also necessary for Pony primitives. They have no fields, but
 * might have methods, which from the perspective of debug symbols are members,
 * and reside in the same DWARF meta structure as fields. Note that there are
 * also recursive type definitions.
 */
void dwarf_forward(dwarf_t* dwarf, gentype_t* g);

/**
 * Emit debug symbols for a field of some composite type. This requires to
 * have called dwarf_forward beforehand. If the owning type is not forward
 * declared, all fields remain unused and therefore will not be in the final
 * binary.
 */
void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field,
  size_t index);

/**
 * Finalise emitting the debug symbols for a composite type. By the nature
 * of how Pony type generation works, a composite type's components will
 * be final, before the type itself becomes final.
 */
void dwarf_composite(dwarf_t* dwarf, gentype_t* g);

/**
 * Initialise the debug symbol emitter. This always allocates a dwarf_t
 * structure, but symbols will only be emitted if compile_t.opt.symbols
 * is true.
 */
void dwarf_init(compile_t* c);

/**
 * Dump collected debug information into the object file. Debug symols
 * will not be included in the final binary without calling this 
 * finaliser. Frees all memory allocated during dwarfing.
 */
void dwarf_finalise(dwarf_t* d);

PONY_EXTERN_C_END

#endif
