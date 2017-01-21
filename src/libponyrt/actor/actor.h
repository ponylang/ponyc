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
  size_t batch;

  // keep things accessed by other actors on a separate cache line
  __pony_spec_align__(heap_t heap, 64); // 52/104 bytes
  gc_t gc; // 44/80 bytes
} pony_actor_t;

bool ponyint_actor_run(pony_ctx_t* ctx, pony_actor_t* actor, size_t batch);

void ponyint_actor_destroy(pony_actor_t* actor);

gc_t* ponyint_actor_gc(pony_actor_t* actor);

heap_t* ponyint_actor_heap(pony_actor_t* actor);

bool ponyint_actor_pendingdestroy(pony_actor_t* actor);

void ponyint_actor_setpendingdestroy(pony_actor_t* actor);

void ponyint_actor_final(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_actor_setsystem(pony_actor_t* actor);

void ponyint_actor_setnoblock(bool state);

void ponyint_destroy(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
