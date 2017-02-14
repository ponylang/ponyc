#ifndef sched_mpmcq_h
#define sched_mpmcq_h

#include <stdint.h>
#include <platform.h>
#include <pony/detail/atomics.h>

PONY_EXTERN_C_BEGIN

typedef struct mpmcq_node_t mpmcq_node_t;

__pony_spec_align__(
  typedef struct mpmcq_t
  {
    PONY_ATOMIC(mpmcq_node_t*) head;
    PONY_ATOMIC(mpmcq_node_t*) tail;
  } mpmcq_t, 64
);

mpmcq_node_t* ponyint_mpmcq_node_alloc(void* data);

void ponyint_mpmcq_node_free(mpmcq_node_t* node);

mpmcq_node_t* ponyint_mpmcq_init(mpmcq_t* q);

void ponyint_mpmcq_destroy(mpmcq_t* q);

void ponyint_mpmcq_push(mpmcq_t* q, void* data);

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data);

void* ponyint_mpmcq_pop(mpmcq_t* q, mpmcq_node_t** old_item);

PONY_EXTERN_C_END

#endif
