#ifndef STRINGTAB_H
#define STRINGTAB_H

#include <platform.h>
#include "../../libponyrt/ds/list.h"

PONY_EXTERN_C_BEGIN

DECLARE_LIST(strlist, strlist_t, const char);

void stringtab_init();
const char* stringtab(const char* string);
const char* stringtab_len(const char* string, size_t len);

// Must be called with a string allocated by ponyint_pool_alloc_size, which the
// function takes control of. The function is responsible for freeing this
// string and it must not be accessed again after the call returns.
const char* stringtab_consume(const char* string, size_t buf_size);

void stringtab_done();

PONY_EXTERN_C_END

#endif
