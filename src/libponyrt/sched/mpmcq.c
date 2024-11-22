#define PONY_WANT_ATOMIC_DEFS

#include "mpmcq.h"
#include "ponyassert.h"
#include "../mem/pool.h"
#include "../sched/cpu.h"

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

typedef struct mpmcq_node_t mpmcq_node_t;

struct mpmcq_node_t
{
  PONY_ATOMIC(mpmcq_node_t*) next;
  PONY_ATOMIC(void*) data;
};

static __pony_thread_local mpmcq_node_t* nodes_to_recycle;

static mpmcq_node_t* node_alloc(void* data)
{
  mpmcq_node_t* node = nodes_to_recycle;

  if (NULL != node)
  {
    nodes_to_recycle = atomic_load_explicit(&node->next, memory_order_relaxed);
  } else {
    node = POOL_ALLOC(mpmcq_node_t);
  }

  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
  atomic_store_explicit(&node->data, data, memory_order_relaxed);

  return node;
}

static void node_free(mpmcq_node_t* node)
{
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(node);
  ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(&node->data);
#endif
  POOL_FREE(mpmcq_node_t, node);
}

#ifndef PONY_NDEBUG

static size_t mpmcq_size_debug(mpmcq_t* q)
{
  size_t count = 0;

  mpmcq_node_t* tail = q->tail.object;

  while(atomic_load_explicit(&tail->next, memory_order_relaxed) != NULL)
  {
    count++;
    tail = atomic_load_explicit(&tail->next, memory_order_relaxed);
  }

  return count;
}

#endif

void ponyint_mpmcq_init(mpmcq_t* q)
{
  nodes_to_recycle = NULL;
  mpmcq_node_t* node = node_alloc(NULL);

  atomic_store_explicit(&q->head, node, memory_order_relaxed);
  q->tail.object = node;
  q->tail.counter = 0;

#ifndef PONY_NDEBUG
  mpmcq_size_debug(q);
#endif
}

void ponyint_mpmcq_cleanup()
{
  while(nodes_to_recycle != NULL)
  {
    mpmcq_node_t* node = nodes_to_recycle;
    nodes_to_recycle = atomic_load_explicit(&node->next, memory_order_relaxed);
    node_free(node);
  }
}

void ponyint_mpmcq_destroy(mpmcq_t* q)
{
  pony_assert(atomic_load_explicit(&q->head, memory_order_relaxed) == q->tail.object);

  atomic_store_explicit(&q->head, NULL, memory_order_relaxed);
  node_free(q->tail.object);
  q->tail.object = NULL;
}

void ponyint_mpmcq_push(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = node_alloc(data);

  // Without that fence, the store to node->next in node_alloc could be
  // reordered after the exchange on the head and after the store to prev->next
  // done by the next push, which would result in the pop incorrectly seeing
  // the queue as empty.
  // Also synchronise with the pop on prev->next.
  atomic_thread_fence(memory_order_release);

  mpmcq_node_t* prev = atomic_exchange_explicit(&q->head, node,
    memory_order_relaxed);

#ifdef USE_VALGRIND
  // Double fence with Valgrind since we need to have prev in scope for the
  // synchronisation annotation.
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
  atomic_thread_fence(memory_order_release);
#endif
  atomic_store_explicit(&prev->next, node, memory_order_relaxed);
}

void ponyint_mpmcq_push_single(mpmcq_t* q, void* data)
{
  mpmcq_node_t* node = node_alloc(data);

  // If we have a single producer, the swap of the head need not be atomic RMW.
  mpmcq_node_t* prev = atomic_load_explicit(&q->head, memory_order_relaxed);
  atomic_store_explicit(&q->head, node, memory_order_relaxed);

  // If we have a single producer, the fence can be replaced with a store
  // release on prev->next.
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
#endif
  atomic_store_explicit(&prev->next, node, memory_order_release);
}

void* ponyint_mpmcq_pop(mpmcq_t* q)
{
  PONY_ABA_PROTECTED_PTR(mpmcq_node_t) cmp;
  PONY_ABA_PROTECTED_PTR(mpmcq_node_t) xchg;
  mpmcq_node_t* tail;
  // Load the tail non-atomically. If object and counter are out of sync, we'll
  // do an additional CAS iteration which isn't less efficient than doing an
  // atomic initial load.
  cmp.object = q->tail.object;
  cmp.counter = q->tail.counter;

  mpmcq_node_t* next;

  do
  {
    tail = cmp.object;
    // Get the next node rather than the tail. The tail is either a stub or has
    // already been consumed.
    next = atomic_load_explicit(&tail->next, memory_order_relaxed);

    if(next == NULL)
      return NULL;

    xchg.object = next;
    xchg.counter = cmp.counter + 1;
  }
  while(!bigatomic_compare_exchange_strong_explicit(&q->tail, &cmp, xchg,
    memory_order_acq_rel, memory_order_acquire));

  // Synchronise on tail->next to ensure we see the write to next->data from
  // the push. Also synchronise on next->data (see comment below).
  // This is a standalone fence instead of a synchronised compare_exchange
  // operation because the latter would result in unnecessary synchronisation
  // on each loop iteration.
#ifdef USE_VALGRID
  atomic_thread_fence(memory_order_acquire);
  ANNOTATE_HAPPENS_AFTER(&tail->next);
  ANNOTATE_HAPPENS_BEFORE(&next->data);
  atomic_thread_fence(memory_order_release);
#else
  atomic_thread_fence(memory_order_acq_rel);
#endif

  void* data = atomic_load_explicit(&next->data, memory_order_acquire);

  // Since we will be freeing the old tail, we need to be sure no other
  // consumer is still reading the old tail. To do this, we set the data
  // pointer of our new tail to NULL, and we wait until the data pointer of
  // the old tail is NULL.
  // We synchronised on next->data to make sure all memory writes we've done
  // will be visible from the thread that will free our tail when it starts
  // freeing it.
  atomic_store_explicit(&next->data, NULL, memory_order_release);

  while(atomic_load_explicit(&tail->data, memory_order_acquire) != NULL)
    ponyint_cpu_relax();

  // Synchronise on tail->data to make sure we see every previous write to the
  // old tail before freeing it. This is a standalone fence to avoid
  // unnecessary synchronisation on each loop iteration.
  atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_AFTER(&tail->data);
#endif

  // if tail has already been consumed and freed by this thread, the read of
  // `tail->next` above in another thread would be a use-after-free error but is
  // technically safe in this context since we will end up looping due to the CAS
  // failing in the while condition due to the aba counter being out of sync
  // we cannot free the tail node if we want to ensure there are no use-after-free
  // concerns so we instead add it to the list of nodes to recycle
  atomic_store_explicit(&tail->next, nodes_to_recycle, memory_order_relaxed);
  nodes_to_recycle = tail;

  return data;
}
