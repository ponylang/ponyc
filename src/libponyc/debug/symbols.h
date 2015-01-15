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

typedef struct symbol_t symbol_t;
typedef struct dbg_symbol_t dbg_type_t;
typedef struct symbols_t symbols_t;

struct dbg_symbol_t
{
  llvm::MDNode* type;
  llvm::MDNode* qualified;
};

struct symbol_t
{
  const char* name;

  union
  {
    dbg_symbol_t* dbg;
    llvm::MDNode* file;
  };

  uint16_t kind;
};

symbols_t* symbols_init(size_t size);

void symbols_destroy(symbols_t** symbols);

symbol_t* symbols_get(symbols_t* symbols, const char* name, bool file);

#endif
