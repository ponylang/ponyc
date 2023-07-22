#ifndef mem_pagemap_h
#define mem_pagemap_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct chunk_t chunk_t;
typedef struct pony_actor_t pony_actor_t;

chunk_t* ponyint_pagemap_get_chunk(const void* addr);

chunk_t* ponyint_pagemap_get(const void* addr, pony_actor_t** actor);

void ponyint_pagemap_set(const void* addr, chunk_t* chunk, pony_actor_t* actor);

void ponyint_pagemap_set_bulk(const void* addr, chunk_t* chunk, pony_actor_t* actor, size_t size);

#ifdef USE_RUNTIMESTATS
/** Get the memory used by the pagemap.
 */
size_t ponyint_pagemap_mem_size();

/** Get the memory allocated by the pagemap.
 */
size_t ponyint_pagemap_alloc_size();
#endif

PONY_EXTERN_C_END

#endif
