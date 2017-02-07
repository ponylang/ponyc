#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

void* ponyint_pagemap_get(const void* m);

void ponyint_pagemap_set(const void* m, void* v);

void ponyint_pagemap_set_bulk(const void* m, void* v, size_t size);

PONY_EXTERN_C_END

#endif
