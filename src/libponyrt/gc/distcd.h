#ifndef gc_distcd_h
#define gc_distcd_h

#include <platform.h>
#include "../pony.h"
#include <stdint.h>
#include <stdbool.h>

PONY_EXTERN_C_BEGIN

typedef struct cycle_record_t
{
  pony_actor_t** members;
  size_t count;
  struct cycle_record_t* next;
} cycle_record_t;

typedef struct candidate_record_t
{
  pony_actor_t** members;
  size_t count;
  pony_actor_t* leader;
  size_t confirmed_count;
} candidate_record_t;

typedef enum distcd_conf_state_t
{
  DISTCD_CONF_NONE = 0,
  DISTCD_CONF_LEADER_WAITING,
  DISTCD_CONF_MEMBER_PENDING,
} distcd_conf_state_t;

typedef struct dedup_chain_t
{
  pony_actor_t* originator;
  uint32_t epoch;
  struct dedup_chain_t* next;
} dedup_chain_t;

typedef struct dedup_conn_t
{
  pony_actor_t* target;
  dedup_chain_t* chains;
  struct dedup_conn_t* next;
} dedup_conn_t;

typedef struct trace_entry_t
{
  pony_actor_t* actor;
  uint32_t epoch;
} trace_entry_t;

typedef struct distcd_t
{
  uint32_t epoch;
  bool released;
  bool retrace_needed;
  uint32_t cycles_generation;
  uint32_t cached_generation;
  pony_actor_t** cached_comp_members;
  size_t cached_comp_count;
  pony_actor_t* cached_leader;
  cycle_record_t* known_cycles;
  dedup_conn_t* trace_dedup;
  candidate_record_t* candidate;
  distcd_conf_state_t conf_state;
} distcd_t;

typedef struct trace_route_msg_t
{
  pony_msg_t msg;
  size_t count;
  trace_entry_t* entries;
} trace_route_msg_t;

// Used for CONFIRM_BLOCKED, CONFIRMED, DENIED, DELEGATE, and RELEASE.
// The leader field is the reply destination in CONFIRM_BLOCKED, the
// delegation target in DELEGATE, the authorizing leader in RELEASE,
// and passed through unchanged in CONFIRMED and DENIED.
// The appearances field carries the recipient's expected rc from cycle
// membership, set by the leader in CONFIRM_BLOCKED and RELEASE.
// The sender field identifies the responding actor in CONFIRMED and DENIED
// so the leader can delegate to the actual denier.
typedef struct confirm_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t count;
  pony_actor_t* leader;
  size_t appearances;
  pony_actor_t* sender;
} confirm_msg_t;

// Create a distcd_t with default initial state.
distcd_t* ponyint_distcd_create();

// Free distcd and all owned state.
void ponyint_distcd_destroy(distcd_t* distcd);

// A foreign reference to target was removed during GC sweep. Increments
// the actor's epoch and prunes cycle records involving target.
void ponyint_distcd_connection_lost(pony_actor_t* actor, pony_actor_t* target);

// A new foreign reference to target was created. Send an initial trace.
// Returns true if the trace was sent, false if suppressed (rc==0 or released).
bool ponyint_distcd_on_acquire(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_actor_t* target);

// On block, re-trace surviving edges after topology changes.
void ponyint_distcd_on_block(pony_ctx_t* ctx, pony_actor_t* actor);

// Process a TRACE_ROUTE. Frees m->entries.
void ponyint_distcd_handle_trace_route(pony_ctx_t* ctx, pony_actor_t* actor,
  trace_route_msg_t* m);

// Process CONFIRM_BLOCKED. Responds CONFIRMED or DENIED. Frees m->members.
void ponyint_distcd_handle_confirm_blocked(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m);

// Process CONFIRMED from a cycle member. Frees m->members.
void ponyint_distcd_handle_confirmed(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m);

// Process DENIED from a cycle member. Delegates leadership. Frees m->members.
void ponyint_distcd_handle_denied(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m);

// Process DELEGATE. Takes ownership of m->members for the candidate.
void ponyint_distcd_handle_delegate(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m);

// Process RELEASE. Marks the actor as released and frees protocol state.
// Frees m->members.
void ponyint_distcd_handle_release(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m);

// Returns false if released. Otherwise frees stale cycle state and returns true.
bool ponyint_distcd_can_self_reap(distcd_t* distcd);

// RELEASE has been received.
bool ponyint_distcd_released(distcd_t* distcd);

// If this actor is the natural leader of a cycle component and not already
// in a confirmation round, initiate one.
void ponyint_distcd_try_confirm(pony_ctx_t* ctx, pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
