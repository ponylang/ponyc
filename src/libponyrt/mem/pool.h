#ifndef mem_pool_h
#define mem_pool_h

#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

// TODO: inline all the things!
extern void* ponyint_alloc(size_t size);
extern void ponyint_dealloc(void* ptr, size_t size);
extern void* ponyint_realloc(void* ptr, size_t old_size, size_t new_size);

#define POOL_MIN_BITS 5
#define POOL_MAX_BITS 20
#define POOL_ALIGN_BITS 10

#define POOL_MIN (1 << POOL_MIN_BITS)
#define POOL_MAX (1 << POOL_MAX_BITS)
#define POOL_ALIGN (1 << POOL_ALIGN_BITS)
#define POOL_COUNT (POOL_MAX_BITS - POOL_MIN_BITS + 1)

__pony_spec_malloc__(void* ponyint_pool_alloc(size_t size));
void ponyint_pool_free(size_t size, void* p);
void* ponyint_pool_realloc(size_t old_size, size_t new_size, void* p);
size_t ponyint_pool_adjust_size(size_t size);

#define POOL_ALLOC(TYPE) \
  (TYPE*) ponyint_pool_alloc(sizeof(TYPE))

#define POOL_FREE(TYPE, VALUE) \
  ponyint_pool_free(sizeof(TYPE), VALUE)

PONY_EXTERN_C_END

#endif
