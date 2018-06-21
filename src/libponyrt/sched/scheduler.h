#ifndef sched_scheduler_h
#define sched_scheduler_h

#ifndef __cplusplus
#  include <stdalign.h>
#endif
#include "mpmcq.h"

typedef struct scheduler_t scheduler_t;

#include "../actor/messageq.h"
#include "../gc/gc.h"
#include "../gc/serialise.h"
#include "../pony.h"
#include <platform.h>
#include "mutemap.h"

#define SPECIAL_THREADID_KQUEUE   -10
#define SPECIAL_THREADID_IOCP     -11
#define SPECIAL_THREADID_EPOLL    -12

#if !defined(PLATFORM_IS_WINDOWS) && !defined(USE_SCHEDULER_SCALING_PTHREADS)
// Signal to use for suspending/resuming threads via `sigwait`/`pthread_kill`
// If you change this, remember to change `signals` package accordingly
#define PONY_SCHED_SLEEP_WAKE_SIGNAL SIGUSR2
#endif

PONY_EXTERN_C_BEGIN

typedef void (*trace_object_fn)(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

typedef void (*trace_actor_fn)(pony_ctx_t* ctx, pony_actor_t* actor);

typedef struct pony_ctx_t
{
  scheduler_t* scheduler;
  pony_actor_t* current;
  trace_object_fn trace_object;
  trace_actor_fn trace_actor;
  gcstack_t* stack;
  actormap_t acquire;

  void* serialise_buffer;
  size_t serialise_size;
  ponyint_serialise_t serialise;
  serialise_alloc_fn serialise_alloc;
  serialise_alloc_fn serialise_alloc_final;
  serialise_throw_fn serialise_throw;
} pony_ctx_t;

struct scheduler_t
{
  // These are rarely changed.
  pony_thread_id_t tid;
  int32_t index;
  uint32_t cpu;
  uint32_t node;
  bool terminate;
  bool asio_stoppable;
  bool asio_noisy;
  pony_signal_event_t sleep_object;

  // These are changed primarily by the owning scheduler thread.
  alignas(64) struct scheduler_t* last_victim;

  pony_ctx_t ctx;
  uint32_t block_count;
  int32_t ack_token;
  uint32_t ack_count;
  mutemap_t mute_mapping;

  // These are accessed by other scheduler threads. The mpmcq_t is aligned.
  mpmcq_t q;
  messageq_t mq;
};

pony_ctx_t* ponyint_sched_init(uint32_t threads, bool noyield, bool nopin,
  bool pinasio, uint32_t min_threads);

bool ponyint_sched_start(bool library);

void ponyint_sched_stop();

void ponyint_sched_add(pony_ctx_t* ctx, pony_actor_t* actor);

uint32_t ponyint_sched_cores();

void ponyint_sched_mute(pony_ctx_t* ctx, pony_actor_t* sender, pony_actor_t* recv);

void ponyint_sched_start_global_unmute(uint32_t from, pony_actor_t* actor);

bool ponyint_sched_unmute_senders(pony_ctx_t* ctx, pony_actor_t* actor);

/** Mark asio as being noisy
 */
void ponyint_sched_noisy_asio(int32_t from);

/** Mark asio as not being noisy
 */
void ponyint_sched_unnoisy_asio(int32_t from);

// Try and wake up a sleeping scheduler thread to help with load
void ponyint_sched_maybe_wakeup(int32_t current_scheduler_id);

// Try and wake up a sleeping scheduler thread only if all scheduler
// threads are asleep
void ponyint_sched_maybe_wakeup_if_all_asleep(int32_t current_scheduler_id);

PONY_EXTERN_C_END

#endif
