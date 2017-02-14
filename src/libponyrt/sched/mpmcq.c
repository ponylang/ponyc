#define PONY_WANT_ATOMIC_DEFS

#include "mpmcq.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

typedef struct mpmcq_node_t mpmcq_node_t;

struct mpmcq_node_t
{
  PONY_ATOMIC(mpmcq_node_t*) next;
  void* data;
};

mpmcq_node_t* ponyint_mpmcq_node_alloc(void* data)
{
  mpmcq_node_t* node = POOL_ALLOC(mpmcq_node_t);
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
  node->data = data;

  return node;
}

void ponyint_mpmcq_node_free(mpmcq_node_t* node)
{
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(node);
#endif
  POOL_FREE(mpmcq_node_t, node);
}

mpmcq_node_t* ponyint_mpmcq_init(mpmcq_t* q)
{
  mpmcq_node_t* node = ponyint_mpmcq_node_alloc(NULL);

  atomic_store_explicit(&q->head, node, memory_order_relaxed);
  atomic_store_explicit(&q->tail, node, memory_order_relaxed);

  return node;
}

void ponyint_mpmcq_destroy(mpmcq_t* q)
{
  // The stub node is freed by one of the schedulers.
  atomic_store_explicit(&q->head, NULL, memory_order_relaxed);
  atomic_store_explicit(&q->tail, NULL, memory_order_relaxed);
}

void ponyint_mpmcq_push(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = ponyint_mpmcq_node_alloc(data);

  mpmcq_node_t* prev = atomic_exchange_explicit(&q->head, node,
    memory_order_relaxed);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
#endif
  atomic_store_explicit(&prev->next, node, memory_order_release);
}

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = ponyint_mpmcq_node_alloc(data);

  // If we have a single producer, the swap of the head need not be atomic RMW.
  mpmcq_node_t* prev = atomic_load_explicit(&q->head, memory_order_relaxed);
  atomic_store_explicit(&q->head, node, memory_order_relaxed);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
#endif
  atomic_store_explicit(&prev->next, node, memory_order_release);
}

void* ponyint_mpmcq_pop(mpmcq_t* q, mpmcq_node_t** old_item)
{
  mpmcq_node_t* tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
  mpmcq_node_t* next;

  do
  {
    // Get the next node rather than the tail. The tail is either a stub or has
    // already been consumed.
    next = atomic_load_explicit(&tail->next, memory_order_relaxed);

    if(next == NULL)
      return NULL;

  } while(!atomic_compare_exchange_weak_explicit(&q->tail, &tail, next,
    memory_order_relaxed, memory_order_relaxed));

  // Synchronise on tail->next.
  atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_AFTER(&tail->next);
#endif

  // Free the previous item popped from the queue by this thread. This mitigates
  // the ABA problem on the compare_exchange by making sure the delay between
  // popping a node and freeing that node is high compared to the CAS loop
  // delay.
  ponyint_mpmcq_node_free(*old_item);
  *old_item = next;

  return next->data;
}
