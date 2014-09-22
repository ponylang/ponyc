#ifndef sched_scheduler_h
#define sched_scheduler_h

#include <pony.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

void scheduler_init(uint32_t threads, bool forcecd);

bool scheduler_start(pony_termination_t termination);

void scheduler_stop();

pony_actor_t* scheduler_worksteal();

void scheduler_add(pony_actor_t* actor);

uint32_t scheduler_cores();

void scheduler_terminate();

PONY_EXTERN_C_END

#endif
