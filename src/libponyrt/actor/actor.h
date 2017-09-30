#ifndef actor_h
#define actor_h

#include "messageq.h"
#include "../gc/gc.h"
#include "../mem/heap.h"
#include "../pony.h"
#include <stdint.h>
#include <stdbool.h>
#ifndef __cplusplus
#  include <stdalign.h>
#endif
#include <platform.h>

PONY_EXTERN_C_BEGIN

#define ACTORMSG_APPLICATION_START (UINT32_MAX - 7)
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
#ifdef USE_ACTOR_CONTINUATIONS
  pony_msg_t* continuation;
#endif
  PONY_ATOMIC(uint8_t) flags;
  size_t batch;
  size_t muted;

  // keep things accessed by other actors on a separate cache line
  alignas(64) heap_t heap; // 52/104 bytes
  gc_t gc; // 48/88 bytes
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

void ponyint_actor_setoverloaded(pony_actor_t* actor);

void ponyint_actor_unsetoverloaded(pony_actor_t* actor);

void maybe_mute(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* m);

PONY_API void ponyint_destroy(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
