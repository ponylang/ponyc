#ifndef sched_scheduler_h
#define sched_scheduler_h

#include <pony.h>
#include <platform.h>
#include "mpmcq.h"

PONY_EXTERN_C_BEGIN

typedef struct scheduler_t scheduler_t;

#ifdef USE_MPMCQ
__pony_spec_align__(
struct scheduler_t
{
  // These are rarely changed.
  pony_thread_id_t tid;
  uint32_t cpu;
  uint32_t node;
  bool finish;
  bool forcecd;

  // These are changed primarily by the owning scheduler thread.
  __pony_spec_align__(struct scheduler_t* last_victim, 64);

  mpmcq_t q;
}, 64
);
#else
__pony_spec_align__(
struct scheduler_t
{
  // These are rarely changed.
  pony_thread_id_t tid;
  uint32_t cpu;
  uint32_t node;
  bool finish;
  bool forcecd;

  // These are changed primarily by the owning scheduler thread.
  __pony_spec_align__(struct scheduler_t* last_victim, 64);

  pony_actor_t* head;
  pony_actor_t* tail;
  struct scheduler_t* victim;

  // These are accessed by other scheduler threads.
  __pony_spec_align__(scheduler_t* thief, 64);
  uint32_t waiting;
}, 64
);
#endif


void scheduler_init(uint32_t threads, bool forcecd);

bool scheduler_start(pony_termination_t termination);

void scheduler_stop();

pony_actor_t* scheduler_worksteal();

void scheduler_add(pony_actor_t* actor);

void scheduler_respond();

void scheduler_offload();

uint32_t scheduler_cores();

void scheduler_terminate();

PONY_EXTERN_C_END

#endif
