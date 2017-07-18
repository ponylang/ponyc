#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct chunk_t chunk_t;

chunk_t* ponyint_pagemap_get(const void* addr);

void ponyint_pagemap_set(const void* addr, chunk_t* chunk);

void ponyint_pagemap_set_bulk(const void* addr, chunk_t* chunk, size_t size);

PONY_EXTERN_C_END

#endif
