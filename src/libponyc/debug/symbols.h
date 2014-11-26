#ifndef DEBUG_SYMBOLS_H
#define DEBUG_SYMBOLS_H

#include "../../libponyrt/ds/hash.h"

#include <llvm/IR/Metadata.h>

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
