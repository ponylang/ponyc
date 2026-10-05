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

// Set a read-only fallback table for the current thread. When set, all
// stringtab lookup functions check the fallback table first (read-only)
// before inserting into the table passed as the first argument.
void stringtab_set_fallback(strtable_t* fallback);
void stringtab_clear_fallback(void);

// Merge all entries from src into dst. Non-conflicting entries are moved
// (pointer preserved); conflicting entries keep their string buffer alive
// because AST nodes may still hold the pointer. After this call, use
// stringtab_free(src) to clean up the hashmap structure.
void stringtab_merge(strtable_t* dst, strtable_t* src);

PONY_EXTERN_C_END

#endif
