#ifndef mem_pool_h
#define mem_pool_h

#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

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

__pony_spec_malloc__(void* ponyint_pool_alloc(uint32_t index));
void ponyint_pool_free(uint32_t index, void* p);

__pony_spec_malloc__(void* ponyint_pool_alloc_size(size_t size));
void ponyint_pool_free_size(size_t size, void* p);

void* ponyint_pool_realloc_size(size_t old_size, size_t new_size, void* p);

void ponyint_pool_thread_cleanup();

uint32_t ponyint_pool_index(size_t size);

size_t ponyint_pool_size(uint32_t index);

size_t ponyint_pool_used_size(size_t size);

size_t ponyint_pool_adjust_size(size_t size);

#define POOL_INDEX(SIZE) \
  ponyint_pool_index(SIZE)

#define POOL_ALLOC(TYPE) \
  (TYPE*) ponyint_pool_alloc(POOL_INDEX(sizeof(TYPE)))

#define POOL_FREE(TYPE, VALUE) \
  ponyint_pool_free(POOL_INDEX(sizeof(TYPE)), VALUE)

#define POOL_SIZE(INDEX) \
  ponyint_pool_size(INDEX)

#ifdef USE_MEMTRACK
#define POOL_ALLOC_SIZE(TYPE) \
  POOL_SIZE(POOL_INDEX(sizeof(TYPE)))
#endif

PONY_EXTERN_C_END

#endif
