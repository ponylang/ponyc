#define PONY_WANT_ATOMIC_DEFS

#include "mpmcq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"

typedef struct mpmcq_node_t mpmcq_node_t;

struct mpmcq_node_t
{
  PONY_ATOMIC(mpmcq_node_t*) next;
  PONY_ATOMIC(void*) data;
};

void ponyint_mpmcq_init(mpmcq_t* q)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  atomic_store_explicit(&node->data, NULL, memory_order_relaxed);
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);

  atomic_store_explicit(&q->head, node, memory_order_relaxed);
  atomic_store_explicit(&q->tail, node, memory_order_relaxed);
  atomic_store_explicit(&q->ticket, 0, memory_order_relaxed);
  atomic_store_explicit(&q->waiting_for, 0, memory_order_relaxed);
}

void ponyint_mpmcq_destroy(mpmcq_t* q)
{
  mpmcq_node_t* tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

  POOL_FREE(mpmcq_node_t, tail);
  atomic_store_explicit(&q->head, NULL, memory_order_relaxed);
  atomic_store_explicit(&q->tail, NULL, memory_order_relaxed);
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
  size_t my_ticket = atomic_fetch_add_explicit(&q->ticket, 1,
    memory_order_relaxed);

  while(my_ticket != atomic_load_explicit(&q->waiting_for,
    memory_order_relaxed))
    ponyint_cpu_relax();

  atomic_thread_fence(memory_order_acquire);

  mpmcq_node_t* tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
  // Get the next node rather than the tail. The tail is either a stub or has
  // already been consumed.
  mpmcq_node_t* next = atomic_load_explicit(&tail->next, memory_order_relaxed);
  
  // Bailout if we have no next node.
  if(next == NULL)
  {
    atomic_store_explicit(&q->waiting_for, my_ticket + 1, memory_order_relaxed);
    return NULL;
  }

  atomic_store_explicit(&q->tail, next, memory_order_relaxed);
  atomic_store_explicit(&q->waiting_for, my_ticket + 1, memory_order_release);

  // Synchronise-with the push.
  atomic_thread_fence(memory_order_acquire);
  
  // We'll return the data pointer from the next node.
  void* data = atomic_load_explicit(&next->data, memory_order_relaxed);

  // Since we will be freeing the old tail, we need to be sure no other
  // consumer is still reading the old tail. To do this, we set the data
  // pointer of our new tail to NULL, and we wait until the data pointer of
  // the old tail is NULL.
  atomic_store_explicit(&next->data, NULL, memory_order_release);

  while(atomic_load_explicit(&tail->data, memory_order_relaxed) != NULL)
    ponyint_cpu_relax();

  atomic_thread_fence(memory_order_acquire);

  // Free the old tail. The new tail is the next node.
  POOL_FREE(mpmcq_node_t, tail);
  return data;
}
