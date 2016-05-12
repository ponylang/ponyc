#ifndef sched_scheduler_h
#define sched_scheduler_h

#include <pony.h>
#include <platform.h>
#include "actor/messageq.h"
#include "gc/gc.h"
#include "gc/serialise.h"
#include "mpmcq.h"

PONY_EXTERN_C_BEGIN

typedef void (*trace_object_fn)(pony_ctx_t* ctx, void* p, pony_type_t* t,
  bool immutable);

typedef void (*trace_actor_fn)(pony_ctx_t* ctx, pony_actor_t* actor);

typedef struct scheduler_t scheduler_t;

typedef struct pony_ctx_t
{
  scheduler_t* scheduler;
  pony_actor_t* current;
  trace_object_fn trace_object;
  trace_actor_fn trace_actor;
  gcstack_t* stack;
  actormap_t acquire;
  bool finalising;

  size_t serialise_size;
  ponyint_serialise_t serialise;

#ifdef USE_TELEMETRY
  size_t tsc;

  size_t count_gc_passes;
  size_t count_alloc;
  size_t count_alloc_size;
  size_t count_alloc_actors;

  size_t count_msg_app;
  size_t count_msg_block;
  size_t count_msg_unblock;
  size_t count_msg_acquire;
  size_t count_msg_release;
  size_t count_msg_conf;
  size_t count_msg_ack;

  size_t time_in_gc;
  size_t time_in_send_scan;
  size_t time_in_recv_scan;
#endif
} pony_ctx_t;

struct scheduler_t
{
  // These are rarely changed.
  pony_thread_id_t tid;
  uint32_t cpu;
  uint32_t node;
  bool terminate;
  bool asio_stopped;

  // These are changed primarily by the owning scheduler thread.
  __pony_spec_align__(struct scheduler_t* last_victim, 64);

  pony_ctx_t ctx;
  uint32_t block_count;
  int32_t ack_token;
  uint32_t ack_count;

  // These are accessed by other scheduler threads. The mpmcq_t is aligned.
  mpmcq_t q;
  messageq_t mq;
};

pony_ctx_t* ponyint_sched_init(uint32_t threads, bool noyield);

bool ponyint_sched_start(bool library);

void ponyint_sched_stop();

void ponyint_sched_add(pony_ctx_t* ctx, pony_actor_t* actor);

uint32_t ponyint_sched_cores();

PONY_EXTERN_C_END

#endif
