#ifndef DEBUG_SYMBOLS_H
#define DEBUG_SYMBOLS_H

#include <platform.h>
#include "dwarf.h"

PONY_EXTERN_C_BEGIN

void symbols_init(symbols_t** symbols, LLVMModuleRef module, bool optimised);

void symbols_package(symbols_t* symbols, const char* path, const char* name);

void symbols_basic(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_pointer(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_trait(symbols_t* symbols, dwarf_meta_t* meta);

void symbols_declare(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta);

void symbols_field(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta);

void symbols_composite(symbols_t* symbols, dwarf_frame_t* frame,
  dwarf_meta_t* meta);

void symbols_finalise(symbols_t* symbols);

PONY_EXTERN_C_END

#endif
