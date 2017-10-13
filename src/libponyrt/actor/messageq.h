#ifndef messageq_h
#define messageq_h

#include "../pony.h"

typedef struct messageq_t
{
  PONY_ATOMIC(pony_msg_t*) head;
  pony_msg_t* tail;
} messageq_t;

#include "../sched/scheduler.h"
#include <platform.h>

#define UNKNOWN_SCHEDULER -1

PONY_EXTERN_C_BEGIN

void ponyint_messageq_init(messageq_t* q);

void ponyint_messageq_destroy(messageq_t* q);

bool ponyint_actor_messageq_push(scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor, messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

bool ponyint_actor_messageq_push_single(scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor, messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

pony_msg_t* ponyint_actor_messageq_pop(scheduler_t* sched,
  pony_actor_t* actor, messageq_t* q);

bool ponyint_thread_messageq_push(
  uintptr_t from_thread, uintptr_t to_thread, messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

bool ponyint_thread_messageq_push_single(
  uintptr_t from_thread, uintptr_t to_thread, messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

pony_msg_t* ponyint_thread_messageq_pop(
  uintptr_t thr, messageq_t* q);

bool ponyint_messageq_markempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
