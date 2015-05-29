#ifndef messageq_h
#define messageq_h

#include <pony.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct messageq_t
{
  pony_msg_t* volatile head;
  pony_msg_t* tail;
} messageq_t;

void messageq_init(messageq_t* q);

void messageq_destroy(messageq_t* q);

bool messageq_push(messageq_t* q, pony_msg_t* m);

pony_msg_t* messageq_pop(messageq_t* q);

bool messageq_markempty(messageq_t* q);

PONY_EXTERN_C_END

#endif
