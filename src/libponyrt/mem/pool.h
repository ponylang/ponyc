#ifndef mem_pool_h
#define mem_pool_h

#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

// TODO: cleanup, inline all the things!
extern uint32_t ponyint_size_to_sizeclass(size_t size);
extern size_t ponyint_sizeclass_to_size(uint32_t sc);
extern size_t ponyint_round_alloc_size(size_t size);
extern void* ponyint_alloc(size_t size);
extern void ponyint_dealloc(void* ptr, size_t size);
extern void* ponyint_realloc(void* ptr, size_t old_size, size_t new_size);

extern void* rust_alloc(size_t alignment, size_t size);
extern void rust_dealloc(void* ptr, size_t alignment, size_t size);
extern void* rust_realloc(void* ptr, size_t alignment, size_t old_size, size_t new_size);

/* Because of the way free memory is reused as its own linked list container,
 * the minimum allocation size is 32 bytes.
 */

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

void ponyint_pool_thread_cleanup();

size_t ponyint_pool_adjust_size(size_t size);

#define POOL_ALLOC(TYPE) \
  (TYPE*) ponyint_pool_alloc(sizeof(TYPE))

#define POOL_FREE(TYPE, VALUE) \
  ponyint_pool_free(sizeof(TYPE), VALUE)

PONY_EXTERN_C_END

#endif
