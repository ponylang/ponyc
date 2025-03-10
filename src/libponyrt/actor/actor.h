#ifndef actor_actor_h
#define actor_actor_h

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

// Reminder: When adding new types, please also update
// examples/dtrace/telemetry.d

#define ACTORMSG_APPLICATION_START (UINT32_MAX - 9)
#define ACTORMSG_CHECKBLOCKED (UINT32_MAX - 8)
#define ACTORMSG_ISBLOCKED (UINT32_MAX - 7)
#define ACTORMSG_BLOCK (UINT32_MAX - 6)
#define ACTORMSG_UNBLOCK (UINT32_MAX - 5)
#define ACTORMSG_ACQUIRE (UINT32_MAX - 4)
#define ACTORMSG_RELEASE (UINT32_MAX - 3)
#define ACTORMSG_CONF (UINT32_MAX - 2)
#define ACTORMSG_ACK (UINT32_MAX - 1)

enum
{
  ACTOR_FLAG_BLOCKED = 1 << 0,
  ACTOR_FLAG_BLOCKED_SENT = 1 << 1,
  ACTOR_FLAG_SYSTEM = 1 << 2,
  ACTOR_FLAG_UNSCHEDULED = 1 << 3,
  ACTOR_FLAG_CD_CONTACTED = 1 << 4,
  ACTOR_FLAG_RC_OVER_ZERO_SEEN = 1 << 5,
  ACTOR_FLAG_PINNED = 1 << 6,
#ifdef USE_RUNTIME_TRACING
  ACTOR_FLAG_TRACE = 1 << 7,
#endif
};

enum
{
  ACTOR_SYNC_FLAG_PENDINGDESTROY = 1 << 0,
  ACTOR_SYNC_FLAG_OVERLOADED = 1 << 1,
  ACTOR_SYNC_FLAG_UNDER_PRESSURE = 1 << 2,
  ACTOR_SYNC_FLAG_MUTED = 1 << 3,
};

typedef struct actorstats_t
{
  size_t heap_mem_allocated;
  size_t heap_mem_used; // actual mem used without "fake used" to trigger GC
  size_t heap_num_allocated;
  size_t heap_realloc_counter;
  size_t heap_alloc_counter;
  size_t heap_free_counter;
  size_t heap_gc_counter;
  size_t system_cpu;
  size_t app_cpu;
  size_t gc_mark_cpu;
  size_t gc_sweep_cpu;
  size_t messages_sent_counter;
  size_t system_messages_processed_counter;
  size_t app_messages_processed_counter;
  // actormap size is hashmap mem + (entry mem * num entries)
  // + size of each objectmap in each entry
  size_t foreign_actormap_objectmap_mem_used;
  size_t foreign_actormap_objectmap_mem_allocated;
} actorstats_t;

typedef struct pony_actor_t
{
  pony_type_t* type;
  messageq_t q;
  // sync flags are access from multiple scheduler threads concurrently
  PONY_ATOMIC(uint8_t) sync_flags;

  // keep things accessed by other actors on a separate cache line
  alignas(64) heap_t heap; // 52/104 bytes
  size_t muted; // 4/8 bytes
  // internal flags are only ever accessed from a single scheduler thread
  uint8_t internal_flags; // 4/8 bytes (after alignment)
  uint32_t live_asio_events;
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats; // 64/128 bytes
#endif
  gc_t gc; // 48/88 bytes
  // if you add more members here, you need to update the PONY_ACTOR_PAD_SIZE
  // calculation below and the pony_static_assert at the top of actor.c, to
  // reference the final member, its offset, and size
} pony_actor_t;

/** Padding for actor types.
 *
 * Size of pony_actor_t minus the padding at the end and the pony_type_t* at the beginning.
 *
 */
#define PONY_ACTOR_PAD_SIZE (offsetof(pony_actor_t, gc) + sizeof(gc_t) - sizeof(pony_type_t*))

typedef struct pony_actor_pad_t
{
  pony_type_t* type;
  char pad[PONY_ACTOR_PAD_SIZE];
} pony_actor_pad_t;

/**
 * Call this to "become" an actor on a non-scheduler context. It is used by
 * the compiler code gen to start the `Main` actor and to run finalizers.
 * It is also used by the `cycle detector` termination function.
 *
 * This can be called with NULL to make no actor the "current" actor for a
 * thread.
 */
void ponyint_become(pony_ctx_t* ctx, pony_actor_t* actor);

/** Send a message to an actor and add to the inject queue if it is not currently
 * scheduled.
 */
void ponyint_sendv_inject(pony_actor_t* to, pony_msg_t* msg);

/** Convenience function to send a message with no arguments and add the actor
 * to the inject queue if it is not currently scheduled.
 *
 * The dispatch function receives a pony_msg_t.
 */
void ponyint_send_inject(pony_actor_t* to, uint32_t id);

/** Convenience function to send a pointer argument in a message and add the actor
 * to the inject queue if it is not currently scheduled.
 *
 * The dispatch function receives a pony_msgp_t.
 */
void ponyint_sendp_inject(pony_actor_t* to, uint32_t id, void* p);

/** Convenience function to send an integer argument in a message and add the actor
 * to the inject queue if it is not currently scheduled.
 *
 * The dispatch function receives a pony_msgi_t.
 */
void ponyint_sendi_inject(pony_actor_t* to, uint32_t id, intptr_t i);

bool ponyint_actor_run(pony_ctx_t* ctx, pony_actor_t* actor, bool polling);

void ponyint_actor_destroy(pony_actor_t* actor);

gc_t* ponyint_actor_gc(pony_actor_t* actor);

heap_t* ponyint_actor_heap(pony_actor_t* actor);

bool ponyint_actor_is_pinned(pony_actor_t* actor);

PONY_API void pony_actor_set_pinned();

PONY_API void pony_actor_unset_pinned();

bool ponyint_actor_pendingdestroy(pony_actor_t* actor);

void ponyint_actor_setpendingdestroy(pony_actor_t* actor);

void ponyint_actor_final(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_actor_sendrelease(pony_ctx_t* ctx, pony_actor_t* actor);

void ponyint_actor_setsystem(pony_actor_t* actor);

void ponyint_actor_setnoblock(bool state);

bool ponyint_actor_getnoblock();

PONY_API void pony_apply_backpressure();

PONY_API void pony_release_backpressure();

PONY_API actorstats_t* pony_actor_stats();

void ponyint_unmute_actor(pony_actor_t* actor);

PONY_API void ponyint_destroy(pony_actor_t* actor);

#ifdef USE_RUNTIME_TRACING
void ponyint_cycle_detector_enable_tracing(pony_actor_t* actor);

bool ponyint_actor_tracing_enabled(pony_actor_t* actor);

void ponyint_actor_enable_tracing();

void ponyint_actor_disable_tracing();
#endif

#ifdef USE_RUNTIMESTATS
size_t ponyint_actor_mem_size(pony_actor_t* actor);

size_t ponyint_actor_alloc_size(pony_actor_t* actor);

size_t ponyint_actor_total_mem_size(pony_actor_t* actor);

size_t ponyint_actor_total_alloc_size(pony_actor_t* actor);
#endif

bool ponyint_acquire_cycle_detector_critical(pony_actor_t* actor);

void ponyint_release_cycle_detector_critical(pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
