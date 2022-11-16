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

#define ACTORMSG_APPLICATION_START (UINT32_MAX - 11)
#define ACTORMSG_CHECKBLOCKED (UINT32_MAX - 10)
#define ACTORMSG_DESTROYED (UINT32_MAX - 9)
#define ACTORMSG_ISBLOCKED (UINT32_MAX - 7)
#define ACTORMSG_BLOCK (UINT32_MAX - 6)
#define ACTORMSG_UNBLOCK (UINT32_MAX - 5)
#define ACTORMSG_ACQUIRE (UINT32_MAX - 4)
#define ACTORMSG_RELEASE (UINT32_MAX - 3)
#define ACTORMSG_CONF (UINT32_MAX - 2)
#define ACTORMSG_ACK (UINT32_MAX - 1)

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
  // accessed from the cycle detector and the actor
  PONY_ATOMIC(uint8_t) cycle_detector_critical;

  // keep things accessed by other actors on a separate cache line
  alignas(64) heap_t heap; // 52/104 bytes
  size_t muted; // 4/8 bytes
  // internal flags are only ever accessed from a single scheduler thread
  uint8_t internal_flags; // 4/8 bytes (after alignment)
#ifdef USE_RUNTIMESTATS
  actorstats_t actorstats; // 64/128 bytes
#endif
  gc_t gc; // 48/88 bytes
} pony_actor_t;

/** Padding for actor types.
 *
 * 56 bytes: initial header, not including the type descriptor
 * 52/104 bytes: heap
 * 4/8 bytes: muted counter
 * 4/8 bytes: internal flags (after alignment)
 * 64/128 bytes: actorstats (if enabled)
 * 48/88 bytes: gc
 * 24/56 bytes: padding to 64 bytes, ignored
 */
#if INTPTR_MAX == INT64_MAX
#ifdef USE_RUNTIMESTATS
#  define PONY_ACTOR_PAD_SIZE 384
#else
#  define PONY_ACTOR_PAD_SIZE 260
#endif
#elif INTPTR_MAX == INT32_MAX
#ifdef USE_RUNTIMESTATS
#  define PONY_ACTOR_PAD_SIZE 224
#else
#  define PONY_ACTOR_PAD_SIZE 164
#endif
#endif

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

bool ponyint_actor_run(pony_ctx_t* ctx, pony_actor_t* actor, bool polling);

void ponyint_actor_destroy(pony_actor_t* actor);

gc_t* ponyint_actor_gc(pony_actor_t* actor);

heap_t* ponyint_actor_heap(pony_actor_t* actor);

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

PONY_API void ponyint_destroy(pony_ctx_t* ctx, pony_actor_t* actor);

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
