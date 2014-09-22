#ifndef cycle_h
#define cycle_h

#include "gc.h"
#include <pony.h>
#include <platform.h>
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

void cycle_create();

void cycle_block(pony_actor_t* actor, gc_t* gc);

void cycle_unblock(pony_actor_t* actor);

void cycle_ack(size_t token);

void cycle_terminate(bool forcecd);

PONY_EXTERN_C_END

#endif
