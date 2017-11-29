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

bool ponyint_actor_messageq_push(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last, scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor);

bool ponyint_actor_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last, scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor);

pony_msg_t* ponyint_actor_messageq_pop(messageq_t* q,
  scheduler_t* sched, pony_actor_t* actor);

bool ponyint_thread_messageq_push(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last,
  uintptr_t from_thread, uintptr_t to_thread);

bool ponyint_thread_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last,
  uintptr_t from_thread, uintptr_t to_thread);

pony_msg_t* ponyint_thread_messageq_pop(messageq_t* q,
  uintptr_t thr);

bool ponyint_messageq_markempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
