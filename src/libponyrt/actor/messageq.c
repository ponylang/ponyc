#include "messageq.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

#ifndef NDEBUG

static size_t messageq_size_debug(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  size_t count = 0;

  while(tail->next != NULL)
  {
    count++;
    tail = tail->next;
  }

  return count;
}

#endif

void ponyint_messageq_init(messageq_t* q)
{
  pony_msg_t* stub = POOL_ALLOC(pony_msg_t);
  stub->index = POOL_INDEX(sizeof(pony_msg_t));
  stub->next = NULL;

  q->head = (pony_msg_t*)((uintptr_t)stub | 1);
  q->tail = stub;

#ifndef NDEBUG
  messageq_size_debug(q);
#endif
}

void ponyint_messageq_destroy(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  assert(((uintptr_t)q->head & ~(uintptr_t)1) == (uintptr_t)tail);

  ponyint_pool_free(tail->index, tail);
  q->head = NULL;
  q->tail = NULL;
}

bool ponyint_messageq_push(messageq_t* q, pony_msg_t* m)
{
  m->next = NULL;

  pony_msg_t* prev = (pony_msg_t*)_atomic_exchange(&q->head, m,
    __ATOMIC_RELAXED);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

  _atomic_store(&prev->next, m, __ATOMIC_RELEASE);

  return was_empty;
}

pony_msg_t* ponyint_messageq_pop(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* next = _atomic_load(&tail->next, __ATOMIC_ACQUIRE);

  if(next != NULL)
  {
    q->tail = next;
    ponyint_pool_free(tail->index, tail);
  }

  return next;
}

bool ponyint_messageq_markempty(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* head = _atomic_load(&q->head, __ATOMIC_ACQUIRE);

  if(((uintptr_t)head & 1) != 0)
    return true;

  if(head != tail)
    return false;

  head = (pony_msg_t*)((uintptr_t)head | 1);

  return _atomic_cas(&q->head, &tail, head, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
