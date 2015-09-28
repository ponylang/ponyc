#ifndef actor_h
#define actor_h

#include "../gc/gc.h"
#include "../mem/heap.h"
#include <pony.h>
#include <stdint.h>
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

#define  ACTORMSG_ACQUIRE (UINT32_MAX - 3)
#define  ACTORMSG_RELEASE (UINT32_MAX - 2)
#define  ACTORMSG_CONF (UINT32_MAX - 1)

bool actor_run(pony_ctx_t* ctx, pony_actor_t* actor);

void actor_destroy(pony_actor_t* actor);

gc_t* actor_gc(pony_actor_t* actor);

heap_t* actor_heap(pony_actor_t* actor);

bool actor_pendingdestroy(pony_actor_t* actor);

void actor_setpendingdestroy(pony_actor_t* actor);

void actor_final(pony_actor_t* actor);

void actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor);

void actor_setsystem(pony_actor_t* actor);

pony_actor_t* actor_next(pony_actor_t* actor);

void actor_setnext(pony_actor_t* actor, pony_actor_t* next);

void pony_destroy(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
