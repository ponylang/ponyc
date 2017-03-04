#ifndef sched_mpmcq_h
#define sched_mpmcq_h

#include <stdint.h>
#ifndef __cplusplus
#  include <stdalign.h>
#endif
#include <platform.h>
#include <pony/detail/atomics.h>

PONY_EXTERN_C_BEGIN

typedef struct mpmcq_node_t mpmcq_node_t;

PONY_ABA_PROTECTED_PTR_DECLARE(mpmcq_node_t)

typedef struct mpmcq_t
{
  alignas(64) PONY_ATOMIC(mpmcq_node_t*) head;
#ifdef PLATFORM_IS_X86
  PONY_ATOMIC_ABA_PROTECTED_PTR(mpmcq_node_t) tail;
#else
  // On ARM, the ABA problem is dealt with by the hardware with
  // LoadLinked/StoreConditional instructions.
  PONY_ATOMIC(mpmcq_node_t*) tail;
#endif
} mpmcq_t;

void ponyint_mpmcq_init(mpmcq_t* q);

void ponyint_mpmcq_destroy(mpmcq_t* q);

void ponyint_mpmcq_push(mpmcq_t* q, void* data);

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data);

void* ponyint_mpmcq_pop(mpmcq_t* q);

PONY_EXTERN_C_END

#endif
