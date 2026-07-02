#ifndef STRINGTAB_H
#define STRINGTAB_H

#include <platform.h>
#include "../../libponyrt/ds/list.h"
#include "../../libponyrt/ds/hash.h"

PONY_EXTERN_C_BEGIN

DECLARE_LIST(strlist, strlist_t, const char);

typedef struct pony_ctx_t pony_ctx_t;

typedef struct stringtab_entry_t stringtab_entry_t;

DECLARE_HASHMAP(strtable, strtable_t, stringtab_entry_t);

// Create a new, empty table of interned strings. Owned by the caller; free it
// with stringtab_free when the strings it holds are no longer referenced.
strtable_t* stringtab_new();

// Destroy a table created with stringtab_new, freeing every interned string in
// it. Any pointer previously returned by an interning function for this table
// becomes dangling.
void stringtab_free(strtable_t* table);

const char* stringtab(strtable_t* table, const char* string);
const char* stringtab_len(strtable_t* table, const char* string, size_t len);

// Must be called with a string allocated by ponyint_pool_alloc_size, which the
// function takes control of. The function is responsible for freeing this
// string and it must not be accessed again after the call returns.
const char* stringtab_consume(strtable_t* table, const char* string,
  size_t buf_size);

PONY_EXTERN_C_END

#endif
