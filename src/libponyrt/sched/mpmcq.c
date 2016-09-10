#include "mpmcq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"

typedef struct mpmcq_node_t mpmcq_node_t;

struct mpmcq_node_t
{
  ATOMIC_TYPE(mpmcq_node_t*) next;
  ATOMIC_TYPE(void*) data;
};

void ponyint_mpmcq_init(mpmcq_t* q)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  atomic_store_explicit(&node->data, NULL, memory_order_relaxed);
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);

  mpmcq_dwcas_t tail;
  tail.node = node;

  atomic_store_explicit(&q->head, node, memory_order_relaxed);
  atomic_store_explicit(&q->tail, tail, memory_order_relaxed);
}

void ponyint_mpmcq_destroy(mpmcq_t* q)
{
  mpmcq_dwcas_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

  POOL_FREE(mpmcq_node_t, tail.node);
  tail.node = NULL;
  q->head = NULL;
  atomic_store_explicit(&q->tail, tail, memory_order_relaxed);
}

void ponyint_mpmcq_push(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  atomic_store_explicit(&node->data, data, memory_order_relaxed);
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);

  mpmcq_node_t* prev = atomic_exchange_explicit(&q->head, node,
    memory_order_relaxed);
  atomic_store_explicit(&prev->next, node, memory_order_release);
}

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  atomic_store_explicit(&node->data, data, memory_order_relaxed);
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);

  // If we have a single producer, the swap of the head need not be atomic RMW.
  mpmcq_node_t* prev = atomic_load_explicit(&q->head, memory_order_relaxed);
  atomic_store_explicit(&q->head, node, memory_order_relaxed);
  atomic_store_explicit(&prev->next, node, memory_order_release);
}

void* ponyint_mpmcq_pop(mpmcq_t* q)
{
  mpmcq_dwcas_t cmp, xchg;
  mpmcq_node_t* next;

  cmp = atomic_load_explicit(&q->tail, memory_order_acquire);

  do
  {
    // Get the next node rather than the tail. The tail is either a stub or has
    // already been consumed.
    next = atomic_load_explicit(&cmp.node->next, memory_order_acquire);

    // Bailout if we have no next node.
    if(next == NULL)
      return NULL;

    // Make the next node the tail, incrementing the aba counter. If this
    // fails, cmp becomes the new tail and we retry the loop.
    xchg.aba = cmp.aba + 1;
    xchg.node = next;
  } while(!atomic_compare_exchange_weak_explicit(&q->tail, &cmp, xchg,
    memory_order_acq_rel, memory_order_relaxed));

  // We'll return the data pointer from the next node.
  void* data = atomic_load_explicit(&next->data, memory_order_relaxed);

  // Since we will be freeing the old tail, we need to be sure no other
  // consumer is still reading the old tail. To do this, we set the data
  // pointer of our new tail to NULL, and we wait until the data pointer of
  // the old tail is NULL.
  atomic_store_explicit(&next->data, NULL, memory_order_release);

  while(atomic_load_explicit(&cmp.node->data, memory_order_acquire) != NULL)
    ponyint_cpu_relax();

  // Free the old tail. The new tail is the next node.
  POOL_FREE(mpmcq_node_t, cmp.node);
  return data;
}
