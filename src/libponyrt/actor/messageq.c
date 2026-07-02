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

  // acq_rel on the exchange:
  //  - release: orders the store to last->next above before the exchange, and
  //    before any thread that later acquires q->head.
  //  - acquire: establishes happens-before with the previous owner's
  //    ponyint_messageq_markempty release-CAS on q->head (or with an earlier
  //    push's release on q->head), so any later consumer of this actor sees
  //    the previous owner's writes to q->tail without relying on C11 release-
  //    sequence extension through read-modify-writes.
  pony_msg_t* prev = atomic_exchange_explicit(&q->head, last,
    memory_order_acq_rel);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&prev->next);
#endif
  // Release so ponyint_actor_messageq_pop's acquire load of tail->next
  // synchronises with the stores above.
  atomic_store_explicit(&prev->next, first, memory_order_release);

  return was_empty;
}

static bool messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last)
{
  atomic_store_explicit(&last->next, NULL, memory_order_relaxed);

  // Use an acq_rel exchange rather than a relaxed load + relaxed store even
  // though the caller promises single-producer. A non-RMW store on q->head
  // would not extend a prior release sequence on q->head, which would break
  // the happens-before chain from the previous owner's markempty to the next
  // consumer. Today this function is only called for TK_NEW sends where the
  // receiver is freshly constructed and no prior release exists, but we don't
  // want correctness to depend on that invariant.
  pony_msg_t* prev = atomic_exchange_explicit(&q->head, last,
    memory_order_acq_rel);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

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

void ponyint_messageq_destroy(messageq_t* q, bool maybe_non_empty)
{
  pony_msg_t* tail = q->tail;

  // If maybe_non_empty is set to true, we take it to mean that the caller
  // doesn't care if the queue is non-empty (i.e. a memory leak), such as
  // when the ASIO thread is destroying its queue at program termination.
  // That is, the queue may sometimes be non-empty in those cases, but the
  // otherwise memory leak is not a problem because the program is terminating.
  (void)maybe_non_empty;
  pony_assert(maybe_non_empty || (
    (((uintptr_t)atomic_load_explicit(&q->head, memory_order_acquire) &
    ~(uintptr_t)1)) == (uintptr_t)tail));

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
    int32_t index = (sched == NULL) ? PONY_UNKNOWN_SCHEDULER_INDEX : sched->index;

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
    int32_t index = (sched == NULL) ? PONY_UNKNOWN_SCHEDULER_INDEX : sched->index;

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
  pony_msg_t* next = atomic_load_explicit(&tail->next, memory_order_acquire);

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
  pony_msg_t* head = atomic_load_explicit(&q->head, memory_order_acquire);

  if(((uintptr_t)head & 1) != 0)
    return true;

  if(head != tail)
    return false;

  head = (pony_msg_t*)((uintptr_t)head | 1);

#ifdef USE_VALGRIND
  ANNOTATE_HAPPENS_BEFORE(&q->head);
#endif
  return atomic_compare_exchange_strong_explicit(&q->head, &tail, head,
    memory_order_acq_rel, memory_order_acquire);
}

bool ponyint_messageq_isempty(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* head = atomic_load_explicit(&q->head, memory_order_acquire);

  if(((uintptr_t)head & 1) != 0)
    return true;

  return head == tail;
}
