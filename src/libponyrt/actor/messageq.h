#ifndef messageq_h
#define messageq_h

#include <pony.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct messageq_t
{
  PONY_ATOMIC(pony_msg_t*) head;
  pony_msg_t* tail;
} messageq_t;

void ponyint_messageq_init(messageq_t* q);

void ponyint_messageq_destroy(messageq_t* q);

bool ponyint_messageq_push(messageq_t* q, pony_msg_t* m);

pony_msg_t* ponyint_messageq_pop(messageq_t* q);

bool ponyint_messageq_markempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
