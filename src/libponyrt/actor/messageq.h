#ifndef actor_messageq_h
#define actor_messageq_h

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
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor
#endif
  );

bool ponyint_actor_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched,
  pony_actor_t* from_actor, pony_actor_t* to_actor
#endif
  );

pony_msg_t* ponyint_actor_messageq_pop(messageq_t* q
#ifdef USE_DYNAMIC_TRACE
  , scheduler_t* sched, pony_actor_t* actor
#endif
  );

bool ponyint_thread_messageq_push(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t from_thread, uintptr_t to_thread
#endif
  );

bool ponyint_thread_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t from_thread, uintptr_t to_thread
#endif
  );

pony_msg_t* ponyint_thread_messageq_pop(messageq_t* q
#ifdef USE_DYNAMIC_TRACE
  , uintptr_t thr
#endif
  );

bool ponyint_messageq_markempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
