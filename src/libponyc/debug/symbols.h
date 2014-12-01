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
typedef struct symbols_t symbols_t;

struct symbol_t
{
  const char* name;

  llvm::MDNode* node;
  llvm::MDNode* pointer;
  llvm::MDNode* qualified;
};

symbols_t* symbols_init(size_t size);

void symbols_destroy(symbols_t** symbols);

symbol_t* symbols_get(symbols_t* symbols, const char* name);

#endif
