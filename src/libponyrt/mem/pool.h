#ifndef mem_pool_h
#define mem_pool_h

#include <stdbool.h>
#include <stddef.h>

#include <platform.h>

PONY_EXTERN_C_BEGIN

#define POOL_MIN_BITS 6

__pony_spec_malloc__(void* pool_alloc(size_t index));
void pool_free(size_t index, void* p);

__pony_spec_malloc__(void* pool_alloc_size(size_t size));
void pool_free_size(size_t size, void* p);

size_t pool_size(size_t index);

bool pool_debug_appears_freed();

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
    EXPR_NONE \
    )))))))))))))))

#define POOL_ALLOC(TYPE) \
  (TYPE*) pool_alloc(POOL_INDEX(sizeof(TYPE)))

#define POOL_FREE(TYPE, VALUE) \
  pool_free(POOL_INDEX(sizeof(TYPE)), VALUE)

PONY_EXTERN_C_END

#endif
