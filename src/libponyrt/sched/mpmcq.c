#include "mpmcq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"

typedef struct mpmcq_node_t mpmcq_node_t;

struct mpmcq_node_t
{
  mpmcq_node_t* volatile next;
  void* volatile data;
};

void ponyint_mpmcq_init(mpmcq_t* q)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  node->data = NULL;
  node->next = NULL;

  q->head = node;
  q->tail.node = node;
}

void ponyint_mpmcq_destroy(mpmcq_t* q)
{
  POOL_FREE(mpmcq_node_t, q->tail.node);
  q->head = NULL;
  q->tail.node = NULL;
}

void ponyint_mpmcq_push(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  node->data = data;
  node->next = NULL;

  mpmcq_node_t* prev = (mpmcq_node_t*)_atomic_exchange(&q->head, node,
    __ATOMIC_RELAXED);
  _atomic_store(&prev->next, node, __ATOMIC_RELEASE);
}

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  node->data = data;
  node->next = NULL;

  // If we have a single producer, operations on the head need not be atomic.
  mpmcq_node_t* prev = q->head;
  q->head = node;
  _atomic_store(&prev->next, node, __ATOMIC_RELEASE);
}

void* ponyint_mpmcq_pop(mpmcq_t* q)
{
  mpmcq_dwcas_t cmp, xchg;
  mpmcq_node_t* next;

  cmp.aba = q->tail.aba;
  cmp.node = q->tail.node;

  do
  {
    // Get the next node rather than the tail. The tail is either a stub or has
    // already been consumed.
    next = _atomic_load(&cmp.node->next, __ATOMIC_ACQUIRE);

    // Bailout if we have no next node.
    if(next == NULL)
      return NULL;

    // Make the next node the tail, incrementing the aba counter. If this
    // fails, cmp becomes the new tail and we retry the loop.
    xchg.aba = cmp.aba + 1;
    xchg.node = next;
  } while(!_atomic_dwcas(&q->tail.dw, &cmp.dw, xchg.dw, __ATOMIC_ACQ_REL,
    __ATOMIC_ACQUIRE));

  // We'll return the data pointer from the next node.
  void* data = _atomic_load(&next->data, __ATOMIC_ACQUIRE);

  // Since we will be freeing the old tail, we need to be sure no other
  // consumer is still reading the old tail. To do this, we set the data
  // pointer of our new tail to NULL, and we wait until the data pointer of
  // the old tail is NULL.
  _atomic_store(&next->data, NULL, __ATOMIC_RELEASE);

  while(_atomic_load(&cmp.node->data, __ATOMIC_ACQUIRE) != NULL)
    ponyint_cpu_relax();

  // Free the old tail. The new tail is the next node.
  POOL_FREE(mpmcq_node_t, cmp.node);
  return data;
}
