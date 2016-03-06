#ifndef cycle_h
#define cycle_h

#include "gc.h"
#include <pony.h>
#include <platform.h>
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

void ponyint_cycle_create(pony_ctx_t* ctx, uint32_t min_deferred,
  uint32_t max_deferred, uint32_t conf_group);

void ponyint_cycle_block(pony_ctx_t* ctx, pony_actor_t* actor, gc_t* gc);

void ponyint_cycle_unblock(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_cycle_ack(pony_ctx_t* ctx, size_t token);

void ponyint_cycle_terminate(pony_ctx_t* ctx);

bool ponyint_is_cycle(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
