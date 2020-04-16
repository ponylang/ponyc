#ifndef gc_cycle_h
#define gc_cycle_h

#include "gc.h"
#include "../pony.h"
#include <platform.h>
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

void ponyint_cycle_create(pony_ctx_t* ctx, uint32_t detect_interval);

bool ponyint_cycle_check_blocked(uint64_t tsc, uint64_t tsc2);

void ponyint_cycle_actor_created(pony_actor_t* actor);

void ponyint_cycle_actor_destroyed(pony_actor_t* actor);

void ponyint_cycle_block(pony_actor_t* actor, gc_t* gc);

void ponyint_cycle_unblock(pony_actor_t* actor);

void ponyint_cycle_ack(size_t token);

void ponyint_cycle_terminate();

bool ponyint_is_cycle(pony_actor_t* actor);

#ifdef USE_MEMTRACK
size_t ponyint_cycle_mem_size();

size_t ponyint_cycle_alloc_size();
#endif

PONY_EXTERN_C_END

#endif
