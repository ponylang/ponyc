#include "messageq.h"
#include "../mem/pool.h"
#include <string.h>
#include <assert.h>

void messageq_init(messageq_t* q)
{
  pony_msg_t* stub = POOL_ALLOC(pony_msg_t);
  stub->size = POOL_INDEX(sizeof(pony_msg_t));
  stub->next = NULL;

  q->head = (pony_msg_t*)((uintptr_t)stub | 1);
  q->tail = stub;
}

void messageq_destroy(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  assert(((uintptr_t)q->head & ~(uintptr_t)1) == (uintptr_t)tail);

  pool_free(tail->size, tail);
  q->head = NULL;
  q->tail = NULL;
}

bool messageq_push(messageq_t* q, pony_msg_t* m)
{
  m->next = NULL;

  pony_msg_t* prev = (pony_msg_t*)_atomic_exch(&q->head, m, __ATOMIC_RELAXED);

  bool was_empty = ((uintptr_t)prev & 1) != 0;
  prev = (pony_msg_t*)((uintptr_t)prev & ~(uintptr_t)1);

  _atomic_store(&prev->next, m, __ATOMIC_RELEASE);

  return was_empty;
}

pony_msg_t* messageq_pop(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* next = _atomic_load(&tail->next, __ATOMIC_ACQUIRE);

  if(next != NULL)
  {
    q->tail = next;
    pool_free(tail->size, tail);
  }

  return next;
}

bool messageq_markempty(messageq_t* q)
{
  pony_msg_t* tail = q->tail;
  pony_msg_t* head = _atomic_load(&q->head, __ATOMIC_RELAXED);

  if(((uintptr_t)head & 1) != 0)
    return true;

  if(head != tail)
    return false;

  head = (pony_msg_t*)((uintptr_t)head | 1);

  return _atomic_cas(&q->head, &tail, head, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
