#ifndef sched_scheduler_h
#define sched_scheduler_h

#ifndef __cplusplus
#  include <stdalign.h>
#endif
#include "mpmcq.h"

typedef struct scheduler_t scheduler_t;

#include "../actor/messageq.h"
#include "../gc/gc.h"
#include "../pony.h"
#include <platform.h>
#include "mutemap.h"

#define PONY_KQUEUE_SCHEDULER_INDEX        -10
#define PONY_SOCK_NOTIFY_SCHEDULER_INDEX   -11
#define PONY_EPOLL_SCHEDULER_INDEX         -12
#define PONY_UNKNOWN_SCHEDULER_INDEX -1

// the `-999` constant is the same value that is hardcoded in `actor_pinning.pony`
// in the `actor_pinning` package
#define PONY_PINNED_ACTOR_THREAD_INDEX -999

PONY_EXTERN_C_BEGIN

typedef void (*trace_object_fn)(pony_ctx_t* ctx, void* p, pony_type_t* t,
  int mutability);

typedef void (*trace_actor_fn)(pony_ctx_t* ctx, pony_actor_t* actor);

typedef enum
{
  SCHED_BLOCK = 20,
  SCHED_UNBLOCK = 21,
  SCHED_CNF = 30,
  SCHED_ACK,
  SCHED_TERMINATE = 40,
  SCHED_UNMUTE_ACTOR = 50,
  SCHED_NOISY_ASIO = 51,
  SCHED_UNNOISY_ASIO = 52,
  SCHED_ACTIVATE = 53
} sched_msg_t;

typedef struct schedulerstats_t
{
  // includes memory for mutemap stuff only
  int64_t mem_used;
  int64_t mem_allocated;
  // includes memory for actor structs (`pony_actor_t`)
  // and memory for acquire/release message contents (i.e. `actormap`s)
  int64_t mem_used_actors;
  int64_t mem_allocated_actors;
  size_t created_actors_counter;
  size_t destroyed_actors_counter;
  size_t actor_app_cpu;
  size_t actor_gc_mark_cpu;
  size_t actor_gc_sweep_cpu;
  size_t actor_system_cpu;
  size_t msg_cpu;
  size_t misc_cpu;
#ifdef USE_RUNTIMESTATS_MESSAGES
  // includes memory for messages only
  int64_t mem_used_inflight_messages;
  int64_t mem_allocated_inflight_messages;
  int64_t num_inflight_messages;
#endif
} schedulerstats_t;

typedef struct pony_ctx_t
{
  scheduler_t* scheduler;
  pony_actor_t* current;
  trace_object_fn trace_object;
  trace_actor_fn trace_actor;
  // Temporary stack for GC tracing; empty when GC not running
  gcstack_t* stack;
  // Temporary storage for acquiring actors/objects for GC;
  // empty when GC not running
  actormap_t acquire;
  // Destination of the message currently being traced for send. Set from the
  // destination argument of pony_gc_send (and pony_send_next, per message, when
  // the message-merge optimiser folds several sends into one trace round) and
  // cleared by pony_send_done. Lets the send-trace recognise a self-send
  // (msg_target == current) and pin the traced objects against the local GC
  // sweep while they sit in this actor's own queue, so weighted reference
  // counting amortises the owner acquire instead of re-borrowing every
  // round-trip.
  pony_actor_t* msg_target;
#ifdef USE_RUNTIMESTATS
  uint64_t last_tsc;
  schedulerstats_t schedulerstats;
#endif
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
  int32_t asio_noisy;

  // These are changed primarily by the owning scheduler thread.
  alignas(64) struct scheduler_t* last_victim;

  /// This thread has broadcast SCHED_BLOCK and has not taken work in
  /// hand since. The latch persists across activations that find
  /// nothing: only work in hand sends SCHED_UNBLOCK and clears it.
  bool blocked;

  /// A SCHED_ACTIVATE arrived. Consumed by the passive visit; cleared
  /// unread while active, so a stale request cannot ride into a later
  /// passive stretch.
  bool activate_requested;

  /// Which schedulers are blocked, one bit per scheduler, maintained
  /// by this thread alone from the block/unblock broadcasts it reads.
  uint64_t* blocked_map;

  /// Which blocked schedulers this thread has already sent
  /// SCHED_ACTIVATE, one bit per scheduler. Debounces repeat requests:
  /// a bit set here blocks another send until that scheduler's next
  /// unblock broadcast clears it.
  uint64_t* poked_map;

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
  bool pinasio, bool pinpat, uint32_t min_threads, uint32_t thread_suspend_threshold,
  uint32_t stats_interval, bool pin_tracing_thread
#if defined(USE_SYSTEMATIC_TESTING)
  , uint64_t systematic_testing_seed);
#else
  );
#endif

// Convert a runtime-stats print interval in seconds to ponyint_cpu_tick()
// cycles. Exposed (rather than inlined into ponyint_sched_init) so the
// width-sensitive conversion can be unit-tested.
uint64_t ponyint_sched_stats_interval_cycles(uint32_t interval_seconds);

bool ponyint_sched_start();

void ponyint_sched_add(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_sched_add_inject(pony_actor_t* actor);

void ponyint_sched_mute(pony_ctx_t* ctx, pony_actor_t* sender, pony_actor_t* recv);

void ponyint_sched_start_global_unmute(uint32_t from, pony_actor_t* actor);

bool ponyint_sched_unmute_senders(pony_ctx_t* ctx, pony_actor_t* actor);

PONY_API uint32_t pony_active_schedulers();

PONY_API int32_t pony_scheduler_index();

PONY_API schedulerstats_t* pony_scheduler_stats();

/** Create a pony_ctx_t for a thread that the runtime uses but that is not a
 * normal scheduler thread, such as the ASIO thread or the thread that
 * bootstraps the runtime. Do this before the thread calls pony_ctx() or runs
 * any Pony code. It is safe, but unnecessary, to call it more than once on the
 * same thread.
 */
void ponyint_register_thread();

/** Release the pony_ctx_t that ponyint_register_thread() created for a thread, as
 * that thread shuts down. Only call this on a thread that was registered with
 * ponyint_register_thread(); never on a scheduler thread.
 */
void ponyint_unregister_thread();

void ponyint_register_asio_thread();

/** Mark asio as being noisy
 */
void ponyint_sched_noisy_asio(int32_t from);

/** Mark asio as not being noisy
 */
void ponyint_sched_unnoisy_asio(int32_t from);

// Ask scheduler 0 to go active if every scheduler is passive. Advisory:
// a stale gauge read in either direction is caught by the passive
// visits' inject pops.
void ponyint_sched_activate_if_all_passive(int32_t from);

#ifdef USE_RUNTIMESTATS
bool ponyint_sched_print_stats();

uint64_t ponyint_sched_cpu_used(pony_ctx_t* ctx);

/** Get the static memory used by the scheduler subsystem.
 */
size_t ponyint_sched_static_mem_size();

/** Get the static memory allocated by the scheduler subsystem.
 */
size_t ponyint_sched_static_alloc_size();

/** Get the total memory used by a scheduler thread.
 */
size_t ponyint_sched_total_mem_size(pony_ctx_t* ctx);

/** Get the total memory allocated by a scheduler thread.
 */
size_t ponyint_sched_total_alloc_size(pony_ctx_t* ctx);
#endif

PONY_EXTERN_C_END

#endif
