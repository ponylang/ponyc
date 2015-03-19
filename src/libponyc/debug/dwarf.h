#ifndef DEBUG_DWARF_H
#define DEBUG_DWARF_H

#include "../ast/ast.h"
#include "../reach/reach.h"

#include <platform.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

#define DW_LANG_Pony 0x8002
#define DW_TAG_Producer "ponyc"

PONY_EXTERN_C_BEGIN

typedef struct dwarf_t dwarf_t;
typedef struct dwarf_frame_t dwarf_frame_t;
typedef struct dwarf_meta_t dwarf_meta_t;

typedef struct gentype_t gentype_t;
typedef struct symbols_t symbols_t;
typedef struct subnodes_t subnodes_t;

enum DWARF_FLAGS
{
  DWARF_FLOAT    = 1 << 0,
  DWARF_SIGNED   = 1 << 1,
  DWARF_BOOLEAN  = 1 << 2,
  DWARF_TUPLE    = 1 << 3,
  DWARF_PRIVATE  = 1 << 4,
  DWARF_CONSTANT = 1 << 5
};

struct dwarf_t
{
  symbols_t* symbols;
  pass_opt_t* opt;
  LLVMTargetDataRef target_data;
  dwarf_frame_t* frame;
};

struct dwarf_frame_t
{
  const char* type_name;
  
  size_t size;
  size_t index;
  
  void* scope;
  
  subnodes_t* members;
  dwarf_frame_t* prev;
};

struct dwarf_meta_t
{
  const char* file;
  const char* name;
  const char* mangled;
  const char* typearg;
  const char** params;

  LLVMBasicBlockRef entry;
  LLVMValueRef inst;
  LLVMValueRef storage;

  size_t line;
  size_t pos;
  size_t offset;
  size_t size;
  size_t align;

  uint8_t flags;
};

/**
 * Initialise the debug symbol emitter.
 */
void dwarf_init(dwarf_t* dwarf, pass_opt_t* opt, LLVMTargetDataRef layout,
  LLVMModuleRef module);

/**
 * Emit debug symbols for a compilation unit. For Pony, there is always a
 * single compilation unit. The file scope for a pony program compilation
 * unit is the package and module where the main actor resides. For Pony
 * libraries the file scope is undefined.
 */
void dwarf_compileunit(dwarf_t* dwarf, ast_t* program);

/**
 * Emit debug symbols for a Pony basic type: U8, ..., U128, I8, ..., I128,
 * Bool, F32, F64. Basic types are builtin, hence have no compilation
 * unit scope and their size and ABI alignment depends on the primitive
 * structure.
 */
void dwarf_basic(dwarf_t* dwarf, gentype_t* g);

/**
 * Emit debug symbols for a pointer to some Pony type. Note that this can be
 * Pointer[A], which would be dwarfed as A**. Pointer[U64]
 * instead is dwarfed as U64*.
 */
void dwarf_pointer(dwarf_t* dwarf, gentype_t* g, const char* typearg);

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
void dwarf_forward(dwarf_t* dwarf, gentype_t* g, size_t methods);

/**
 * Emit debug symbols for a field of some composite type. This requires to
 * have called dwarf_forward beforehand. If the owning type is not forward
 * declared, behaviour is undefined.
 */
void dwarf_field(dwarf_t* dwarf, gentype_t* composite, gentype_t* field);

/**
 * Emit debug symbols for a method. A method itself is implicitly a new lexical
 * scope. However, a method might have arbitrarily many nested lexical scopes
 * which need to be introduced with a call to dwarf_lexical_scope.
 */
void dwarf_method(dwarf_t* dwarf, ast_t* fun, const char* name,
  const char* mangled, const char** params, size_t count, LLVMValueRef ir);

/**
 * Introduce a new lexical scope.
 */
void dwarf_lexicalscope(dwarf_t* dwarf, ast_t* ast);

/**
 * Emit debug symbols for a local variable.
 */
void dwarf_local(dwarf_t* dwarf, ast_t* ast, const char* type,
  LLVMBasicBlockRef entry, LLVMValueRef inst, LLVMValueRef storage,
  bool is_arg);

/**
 * Finalise emitting debug symbols for a lexical scope.
 */
void dwarf_lexicalscope_finish(dwarf_t* dwarf);

/**
 * Finalise emitting the debug symbols for a composite type. By the nature
 * of how Pony type generation works, a composite type's components will
 * be final, before the type itself becomes final.
 */
void dwarf_finish(dwarf_t* dwarf, gentype_t* g);

/**
 * Dump collected debug information into the object file. Debug symols
 * will not be included in the final binary without calling this
 * finaliser. Frees all memory allocated during dwarfing.
 */
void dwarf_finalise(dwarf_t* dwarf);

PONY_EXTERN_C_END

#endif
