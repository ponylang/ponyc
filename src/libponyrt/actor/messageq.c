#include "messageq.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

void messageq_init(messageq_t* q)
{
  pony_msg_t* stub = (pony_msg_t*)pool_alloc(0);
  stub->index = 0;
  stub->next = NULL;

  q->head = (pony_msg_t*)((uintptr_t)stub | 1);
  q->tail = stub;
}

void messageq_destroy(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  assert(((uintptr_t)q->head & ~(uintptr_t)1) == (uintptr_t)tail);

  pool_free(tail->index, tail);
  q->head = NULL;
  q->tail = NULL;
}

bool messageq_push(messageq_t* q, pony_msg_t* m)
{
  m->next = NULL;

  pony_msg_t* prev = (pony_msg_t*)__pony_atomic_exchange_n(&q->head, m,
    PONY_ATOMIC_RELAXED, intptr_t);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

  __pony_atomic_store_n(&prev->next, m, PONY_ATOMIC_RELEASE,
    PONY_ATOMIC_NO_TYPE);

  return was_empty;
}

pony_msg_t* messageq_pop(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* next = __pony_atomic_load_n(&tail->next, PONY_ATOMIC_ACQUIRE,
    PONY_ATOMIC_NO_TYPE);

  if(next != NULL)
  {
    q->tail = next;
    pool_free(tail->index, tail);
  }

  return next;
}

bool messageq_markempty(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* head = __pony_atomic_load_n(&q->head, PONY_ATOMIC_RELAXED,
    PONY_ATOMIC_NO_TYPE);

  if(((uintptr_t)head & 1) != 0)
    return true;

  if(head != tail)
    return false;

  head = (pony_msg_t*)((uintptr_t)head | 1);

  return __pony_atomic_compare_exchange_n(&q->head, &tail, head, false,
    PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t);
}
