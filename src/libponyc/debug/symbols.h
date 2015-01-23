#ifndef DEBUG_SYMBOLS_H
#define DEBUG_SYMBOLS_H

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4003)
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/Path.h>
#include <llvm/IR/Metadata.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include "dwarf.h"

typedef struct symbols_t symbols_t;
typedef struct subnodes_t subnodes_t;

typedef struct symbol_scope_t
{
  llvm::DIFile file;
  size_t line;
  size_t pos;
} symbol_scope_t;

symbols_t* symbols_init(compile_t* c);

llvm::DIFile symbols_file(symbols_t* symbols, const char* fullpath);

bool symbols_known_type(symbols_t* symbols, const char* name);

void symbols_declare(symbols_t* symbols, gentype_t* g, subnodes_t** members, 
  size_t size, symbol_scope_t* scope);

void symbols_package(symbols_t* symbols, const char* path, const char* name);

void symbols_basic(symbols_t* symbols, gentype_t* g);

void symbols_pointer(symbols_t* symbols, gentype_t* ptr, gentype_t* g);

void symbols_trait(symbols_t* symbols, gentype_t* g, symbol_scope_t* scope);

void symbols_member(symbols_t* symbols, gentype_t* field, subnodes_t* subnodes,
  symbol_scope_t* scope, const char* name, bool priv, bool constant,
  size_t index);

void symbols_composite(symbols_t* symbols, gentype_t* g, size_t offset, 
  subnodes_t* subnodes, symbol_scope_t* scope);

void symbols_finalise(symbols_t* symbols);

#endif