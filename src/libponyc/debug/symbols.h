#ifndef DEBUG_SYMBOLS_H
#define DEBUG_SYMBOLS_H

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable:4003)
#  pragma warning(disable:4267)
#endif

#include <llvm/IR/Metadata.h>

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

typedef struct symbols_t symbols_t;

symbols_t* symbols_init(compile_t* c);

DIFile symbols_file(symbols_t* symbols, ast_t* module);

void symbols_compileunit(symbols_t* symbols, ast_t* package);

void symbols_basic(symbols_t* symbols, gentype_t* g, dwarf_type_t* type);

void symbols_trait(symbols_t* symbols, gentype_t* g, dwarf_type_t* type);

void symbols_composite(symbols_t* symbols, gentype_t* g, dwarf_type_t* type);

void symbols_finalise(symbols_t* symbols, bool verify);

#endif

/*
switch(g->type_name[0])
  {
    case 'U': type->tag = dwarf::DW_ATE_unsigned; break;
    case 'I': type->tag = dwarf::DW_ATE_signed; break;
    case 'F': type->tag = dwarf::DW_ATE_float; break;
    case 'B': type->tag = dwarf::DW_ATE_boolean; break;
    default: assert(0);
  }
*/