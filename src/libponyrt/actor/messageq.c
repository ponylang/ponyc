#define PONY_WANT_ATOMIC_DEFS

#include "messageq.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <string.h>
#include <dtrace.h>

#ifdef USE_VALGRIND
#include <valgrind/helgrind.h>
#endif

#ifndef PONY_NDEBUG

static size_t messageq_size_debug(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  size_t count = 0;

  while(atomic_load_explicit(&tail->next, memory_order_relaxed) != NULL)
  {
    count++;
    tail = atomic_load_explicit(&tail->next, memory_order_relaxed);
  }

  return count;
}

#endif

static bool messageq_push(messageq_t* q, pony_msg_t* first, pony_msg_t* last)
{
  atomic_store_explicit(&last->next, NULL, memory_order_relaxed);

  // Without that fence, the store to last->next above could be reordered after
  // the exchange on the head and after the store to prev->next done by the
  // next push, which would result in the pop incorrectly seeing the queue as
  // empty.
  // Also synchronise with the pop on prev->next.
  atomic_thread_fence(memory_order_release);

  pony_msg_t* prev = atomic_exchange_explicit(&q->head, last,
    memory_order_relaxed);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

#ifdef USE_VALGRIND
  // Double fence with Valgrind since we need to have prev in scope for the
  // synchronisation annotation.
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
  atomic_thread_fence(memory_order_release);
#endif
  atomic_store_explicit(&prev->next, first, memory_order_relaxed);

  return was_empty;
}

static bool messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last)
{
  atomic_store_explicit(&last->next, NULL, memory_order_relaxed);

  // If we have a single producer, the swap of the head need not be atomic RMW.
  pony_msg_t* prev = atomic_load_explicit(&q->head, memory_order_relaxed);
  atomic_store_explicit(&q->head, last, memory_order_relaxed);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

  // If we have a single producer, the fence can be replaced with a store
  // release on prev->next.
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
#endif
  atomic_store_explicit(&prev->next, first, memory_order_release);

  return was_empty;
}

void ponyint_messageq_init(messageq_t* q)
{
  pony_msg_t* stub = POOL_ALLOC(pony_msg_t);
  stub->index = POOL_INDEX(sizeof(pony_msg_t));
  atomic_store_explicit(&stub->next, NULL, memory_order_relaxed);

  atomic_store_explicit(&q->head, (pony_msg_t*)((uintptr_t)stub | 1),
    memory_order_relaxed);
  q->tail = stub;

#ifndef PONY_NDEBUG
  messageq_size_debug(q);
#endif
}

void ponyint_messageq_destroy(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_assert((((uintptr_t)atomic_load_explicit(&q->head, memory_order_relaxed) &
    ~(uintptr_t)1)) == (uintptr_t)tail);
#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(tail);
#endif

  ponyint_pool_free(tail->index, tail);
  atomic_store_explicit(&q->head, NULL, memory_order_relaxed);
  q->tail = NULL;
}

bool ponyint_actor_messageq_push(messageq_t* q, pony_msg_t* first,
  pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor
#endif
  )
{
#ifdef USE_DYNAMIC_TRACE
  if(DTRACE_ENABLED(ACTOR_MSG_PUSH))
  {
    pony_msg_t* m = first;
    int32_t index = (sched == NULL) ? UNKNOWN_SCHEDULER : sched->index;

    while(m != last)
    {
      DTRACE4(ACTOR_MSG_PUSH, index, m->id,
        (uintptr_t)from_actor, (uintptr_t)to_actor);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE4(ACTOR_MSG_PUSH, index, last->id,
      (uintptr_t)from_actor, (uintptr_t)to_actor);
    /* Hush compiler warnings when DTrace isn't available */
    (void)index;
    (void)from_actor;
    (void)to_actor;
  }
#endif
  return messageq_push(q, first, last);
}

bool ponyint_thread_messageq_push(messageq_t* q, pony_msg_t* first,
  pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t from_thr, uintptr_t to_thr
#endif
  )
{
#ifdef USE_DYNAMIC_TRACE
  if(DTRACE_ENABLED(THREAD_MSG_PUSH))
  {
    pony_msg_t* m = first;

    while(m != last)
    {
      DTRACE3(THREAD_MSG_PUSH, m->id, from_thr, to_thr);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE3(THREAD_MSG_PUSH, last->id, from_thr, to_thr);
    /* Hush compiler warnings when DTrace isn't available */
    (void)from_thr;
    (void)to_thr;
  }
#endif
  return messageq_push(q, first, last);
}

bool ponyint_actor_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched, pony_actor_t* from_actor, pony_actor_t* to_actor
#endif
  )
{
#ifdef USE_DYNAMIC_TRACE
  if(DTRACE_ENABLED(ACTOR_MSG_PUSH))
  {
    pony_msg_t* m = first;
    int32_t index = (sched == NULL) ? UNKNOWN_SCHEDULER : sched->index;

    while(m != last)
    {
      DTRACE4(ACTOR_MSG_PUSH, index, m->id,
        (uintptr_t)from_actor, (uintptr_t)to_actor);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE4(ACTOR_MSG_PUSH, index, m->id,
      (uintptr_t)from_actor, (uintptr_t)to_actor);
    /* Hush compiler warnings when DTrace isn't available */
    (void)index;
    (void)from_actor;
    (void)to_actor;
  }
#endif
  return messageq_push_single(q, first, last);
}

bool ponyint_thread_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t from_thr, uintptr_t to_thr
#endif
  )
{
#ifdef USE_DYNAMIC_TRACE
  if(DTRACE_ENABLED(THREAD_MSG_PUSH))
  {
    pony_msg_t* m = first;

    while(m != last)
    {
      DTRACE3(THREAD_MSG_PUSH, m->id, from_thr, to_thr);
      m = atomic_load_explicit(&m->next, memory_order_relaxed);
    }

    DTRACE3(THREAD_MSG_PUSH, m->id, from_thr, to_thr);
    /* Hush compiler warnings when DTrace isn't available */
    (void)from_thr;
    (void)to_thr;
  }
#endif
  return messageq_push_single(q, first, last);
}

pony_msg_t* ponyint_actor_messageq_pop(messageq_t* q
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched, pony_actor_t* actor
#endif
  )
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* next = atomic_load_explicit(&tail->next, memory_order_relaxed);

  if(next != NULL)
  {
    DTRACE3(ACTOR_MSG_POP, sched->index, (uint32_t) next->id, (uintptr_t) actor);
    q->tail = next;
    atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
    ANNOTATE_HAPPENS_AFTER(&tail->next);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(tail);
#endif
    ponyint_pool_free(tail->index, tail);
  }

  return next;
}

pony_msg_t* ponyint_thread_messageq_pop(messageq_t* q
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t thr
#endif
  )
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* next = atomic_load_explicit(&tail->next, memory_order_relaxed);

  if(next != NULL)
  {
    DTRACE2(THREAD_MSG_POP, (uint32_t) next->id, (uintptr_t) thr);
    q->tail = next;
    atomic_thread_fence(memory_order_acquire);
#ifdef USE_VALGRIND
    ANNOTATE_HAPPENS_AFTER(&tail->next);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(tail);
#endif
    ponyint_pool_free(tail->index, tail);
  }

  return next;
}

bool ponyint_messageq_markempty(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* head = atomic_load_explicit(&q->head, memory_order_relaxed);

  if(((uintptr_t)head & 1) != 0)
    return true;

  if(head != tail)
    return false;

  head = (pony_msg_t*)((uintptr_t)head | 1);

#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&q->head);
#endif
  return atomic_compare_exchange_strong_explicit(&q->head, &tail, head,
    memory_order_release, memory_order_relaxed);
}
