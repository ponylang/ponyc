#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct chunk_t chunk_t;

chunk_t* ponyint_pagemap_get(const void* addr);

void ponyint_pagemap_set(const void* addr, chunk_t* chunk);

void ponyint_pagemap_set_bulk(const void* addr, chunk_t* chunk, size_t size);

#ifdef USE_MEMTRACK
/** Get the memory used by the pagemap.
 */
size_t ponyint_pagemap_mem_size();

/** Get the memory allocated by the pagemap.
 */
size_t ponyint_pagemap_alloc_size();
#endif

PONY_EXTERN_C_END

#endif
