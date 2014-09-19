#ifndef sched_mpmcq_h
#define sched_mpmcq_h

#include <stdint.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct mpmcq_node_t mpmcq_node_t;

__pony_spec_align__(
  typedef struct mpmcq_dwcas_t
  {
    union
    {
      struct
      {
        uint64_t aba;
        mpmcq_node_t* node;
      };

      __int128_t dw;
    };
  } mpmcq_dwcas_t, 16
);

__pony_spec_align__(
  typedef struct mpmcq_t
  {
    mpmcq_node_t* head;
    mpmcq_dwcas_t tail;
  } mpmcq_t, 64
);

void mpmcq_init(mpmcq_t* q);

void mpmcq_destroy(mpmcq_t* q);

void mpmcq_push(mpmcq_t* q, void* data);

void* mpmcq_pop(mpmcq_t* q);

PONY_EXTERN_C_END

#endif
