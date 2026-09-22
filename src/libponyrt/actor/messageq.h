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

PONY_EXTERN_C_BEGIN

void ponyint_messageq_init(messageq_t* q);

void ponyint_messageq_destroy(messageq_t* q, bool maybe_non_empty);

bool ponyint_actor_messageq_push(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

bool ponyint_actor_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

pony_msg_t* ponyint_actor_messageq_pop(messageq_t* q);

bool ponyint_thread_messageq_push(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

bool ponyint_thread_messageq_push_single(messageq_t* q,
  pony_msg_t* first, pony_msg_t* last);

pony_msg_t* ponyint_thread_messageq_pop(messageq_t* q);

bool ponyint_messageq_markempty(messageq_t* q);

bool ponyint_messageq_isempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
