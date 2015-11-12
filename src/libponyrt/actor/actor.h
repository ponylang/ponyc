#ifndef actor_h
#define actor_h

#include "../gc/gc.h"
#include "../mem/heap.h"
#include "messageq.h"
#include <pony.h>
#include <stdint.h>
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

#define ACTORMSG_BLOCK (UINT32_MAX - 6)
#define ACTORMSG_UNBLOCK (UINT32_MAX - 5)
#define ACTORMSG_ACQUIRE (UINT32_MAX - 4)
#define ACTORMSG_RELEASE (UINT32_MAX - 3)
#define ACTORMSG_CONF (UINT32_MAX - 2)
#define ACTORMSG_ACK (UINT32_MAX - 1)

typedef struct pony_actor_t
{
  pony_type_t* type;
  messageq_t q;
  pony_msg_t* continuation;
  uint8_t flags;

  // keep things accessed by other actors on a separate cache line
  __pony_spec_align__(heap_t heap, 64); // 104 bytes
  gc_t gc; // 80 bytes
} pony_actor_t;

bool actor_run(pony_ctx_t* ctx, pony_actor_t* actor);

void actor_destroy(pony_actor_t* actor);

gc_t* actor_gc(pony_actor_t* actor);

heap_t* actor_heap(pony_actor_t* actor);

bool actor_pendingdestroy(pony_actor_t* actor);

void actor_setpendingdestroy(pony_actor_t* actor);

void actor_final(pony_ctx_t* ctx, pony_actor_t* actor);

void actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor);

void actor_setsystem(pony_actor_t* actor);

pony_actor_t* actor_next(pony_actor_t* actor);

void actor_setnext(pony_actor_t* actor, pony_actor_t* next);

void pony_destroy(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
