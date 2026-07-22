#ifndef mem_pool_h
#define mem_pool_h

#include <stdbool.h>
#include <stddef.h>

#include <platform.h>

#if defined(USE_POOL_MEMALIGN) && defined(USE_POOL_CLASSIC)
#  error "pool_memalign and pool_classic select different allocators; use one"
#endif

/* The arena allocator is the default on every platform. */
#if defined(USE_POOL_MEMALIGN)
#  define POOL_USE_MEMALIGN
#elif defined(USE_POOL_CLASSIC)
#  define POOL_USE_CLASSIC
#else
#  define POOL_USE_ARENA
#endif

#if defined(USE_POOL_RETAIN) && !defined(POOL_USE_CLASSIC)
#  error "pool_retain modifies the classic pool; add pool_classic"
#endif

/* The arena allocator's memory comes straight from mmap and carries no
 * AddressSanitizer, Valgrind, or pooltrack instrumentation, so those
 * builds would report clean runs while checking nothing of it. The
 * allocator's own release-build checks are its memory-safety net; a
 * quietly unprotected build is worse than a rejected one.
 */
#if defined(POOL_USE_ARENA) && defined(USE_ADDRESS_SANITIZER)
#  error "address_sanitizer cannot track the arena allocator; add pool_memalign or pool_classic"
#endif

#if defined(POOL_USE_ARENA) && defined(USE_VALGRIND)
#  error "valgrind has no annotations for the arena allocator; add pool_classic"
#endif

#if defined(POOL_USE_ARENA) && defined(USE_POOLTRACK)
#  error "pooltrack instruments only the classic pool; add pool_classic"
#endif

PONY_EXTERN_C_BEGIN

/* Because of the way free memory is reused as its own linked list container,
 * the minimum allocation size is 32 bytes for the classic and arena pool
 * implementations and 16 bytes for the memalign pool implementation.
 *
 * The interface guarantees a returned pointer is non-null, aligned (POOL_MIN
 * always, POOL_ALIGN once the granted size reaches it), overlaps no live
 * allocation, and keeps its content until freed. Whether a freed pointer
 * comes back at the same address is backend behavior, not a guarantee.
 */

#if defined(POOL_USE_CLASSIC) || defined(POOL_USE_ARENA)
#define POOL_MIN_BITS 5
#else
#define POOL_MIN_BITS 4
#endif

#define POOL_MAX_BITS 20
#define POOL_ALIGN_BITS 10

// POOL_MIN is the minimum (and guaranteed) alignment of every pool allocation.
// codegen declares it to LLVM as the `align` return attribute of the pool
// allocators (pony_create, pony_alloc_msg; init_runtime in codegen.c), so it
// must not be lowered below a target's maximum field alignment. See #5462.
#define POOL_MIN (1 << POOL_MIN_BITS)
#define POOL_MAX (1 << POOL_MAX_BITS)
#define POOL_ALIGN (1 << POOL_ALIGN_BITS)
#define POOL_MEMALIGN_MIN_ALIGN 64
#define POOL_COUNT (POOL_MAX_BITS - POOL_MIN_BITS + 1)

__pony_spec_malloc__(void* ponyint_pool_alloc(size_t index));
void ponyint_pool_free(size_t index, void* p);

__pony_spec_malloc__(void* ponyint_pool_alloc_size(size_t size));
void ponyint_pool_free_size(size_t size, void* p);

void* ponyint_pool_realloc_size(size_t old_size, size_t new_size, void* p);

void ponyint_pool_thread_cleanup();

/* The suspend-and-drain interface. suspend_flush delivers this thread's
 * pending foreign frees to their owners and drains its own inbox; a
 * passive thread calls it before it sleeps. drain only drains this
 * thread's own inbox, crediting what other threads delivered to it.
 * Nothing notifies an owner of a delivery, so an owner reclaims foreign
 * frees on its next drain. Only the arena allocator has inboxes; the
 * other backends define these as no-ops.
 */
void ponyint_pool_suspend_flush();

void ponyint_pool_drain();

/* return_idle hands this thread's held-but-free memory back to the operating
 * system: the arena backend flushes its per-thread cache to slabs and
 * decommits its arenas' dirty pages. The passive path calls it once a thread
 * has been idle long enough that holding the memory no longer pays; the thread
 * rewarms when it runs again. The other backends define it as a no-op.
 */
void ponyint_pool_return_idle();

/* The memory profile trades resident footprint against throughput by setting
 * how quickly the allocator returns freed memory to the operating system.
 * Selected once at startup by --ponymemoryprofile; balanced is the default and
 * leaves the shipped behavior unchanged. Only the arena backend acts on it.
 */
typedef enum
{
  POOL_MEMORY_BALANCED,
  POOL_MEMORY_LOW,
  POOL_MEMORY_THROUGHPUT
} pool_memory_profile_t;

void ponyint_pool_set_memory_profile(pool_memory_profile_t profile);

size_t ponyint_pool_index(size_t size);

size_t ponyint_pool_used_size(size_t size);

size_t ponyint_pool_adjust_size(size_t size);

#if defined(POOL_USE_CLASSIC) || defined(POOL_USE_ARENA)
#define POOL_INDEX(SIZE) \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 0)), 0, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 1)), 1, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 2)), 2, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 3)), 3, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 4)), 4, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 5)), 5, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 6)), 6, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 7)), 7, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 8)), 8, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 9)), 9, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 10)), 10, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 11)), 11, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 12)), 12, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 13)), 13, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 14)), 14, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 15)), 15, \
    EXPR_NONE \
    ))))))))))))))))
#else
#define POOL_INDEX(SIZE) \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 0)), 0, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 1)), 1, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 2)), 2, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 3)), 3, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 4)), 4, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 5)), 5, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 6)), 6, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 7)), 7, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 8)), 8, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 9)), 9, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 10)), 10, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 11)), 11, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 12)), 12, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 13)), 13, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 14)), 14, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 15)), 15, \
  __pony_choose_expr(SIZE <= (1 << (POOL_MIN_BITS + 16)), 16, \
    EXPR_NONE \
    )))))))))))))))))
#endif

#define POOL_ALLOC(TYPE) \
  (TYPE*) ponyint_pool_alloc(POOL_INDEX(sizeof(TYPE)))

#define POOL_FREE(TYPE, VALUE) \
  ponyint_pool_free(POOL_INDEX(sizeof(TYPE)), VALUE)

#define POOL_SIZE(INDEX) \
  ((size_t)1 << (POOL_MIN_BITS + INDEX))

#ifdef USE_RUNTIMESTATS
#define POOL_ALLOC_SIZE(TYPE) \
  POOL_SIZE(POOL_INDEX(sizeof(TYPE)))
#endif

PONY_EXTERN_C_END

#endif
