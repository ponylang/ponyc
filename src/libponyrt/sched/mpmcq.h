#ifndef sched_mpmcq_h
#define sched_mpmcq_h

#include <stdint.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct mpmcq_node_t mpmcq_node_t;

typedef struct mpmcq_dwcas_t
{
  uintptr_t aba;
  mpmcq_node_t* node;
} mpmcq_dwcas_t;

__pony_spec_align__(
  typedef struct mpmcq_t
  {
    ATOMIC_TYPE(mpmcq_node_t*) head;
    ATOMIC_TYPE(mpmcq_dwcas_t) tail;
  } mpmcq_t, 64
);

void ponyint_mpmcq_init(mpmcq_t* q);

void ponyint_mpmcq_destroy(mpmcq_t* q);

void ponyint_mpmcq_push(mpmcq_t* q, void* data);

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data);

void* ponyint_mpmcq_pop(mpmcq_t* q);

PONY_EXTERN_C_END

#endif
