#ifndef DEBUG_SYMBOLS_H
#define DEBUG_SYMBOLS_H

#include <platform.h>
#include "dwarf.h"

PONY_EXTERN_C_BEGIN

void symbols_init(symbols_t** symbols, LLVMBuilderRef builder, 
  LLVMModuleRef module, bool optimised);

void symbols_push_frame(symbols_t* symbols, gentype_t* g);

void symbols_pop_frame(symbols_t* symbols);

void symbols_package(symbols_t* symbols, const char* path, const char* name);

void symbols_basic(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_pointer(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_trait(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_unspecified(symbols_t* symbols, const char* name);

void symbols_declare(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_field(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_method(symbols_t* symbols, dwarf_meta_t* meta, LLVMValueRef ir);

void symbols_composite(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_lexicalscope(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_local(symbols_t* symbols, dwarf_meta_t* meta, bool is_arg);

void symbols_location(symbols_t* symbols, size_t line, size_t pos);

void symbols_reset(symbols_t* symbols, bool disable);

void symbols_finalise(symbols_t* symbols);

PONY_EXTERN_C_END

#endif
