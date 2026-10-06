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
  size_t* appearances;
  size_t count;
  pony_actor_t* leader;
  uint64_t confirmed_bits;
  bool denied;
  pony_actor_t* denier;
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
  bool initial_trace_done;
  uint32_t cycles_generation;
  uint32_t cached_generation;
  pony_actor_t** cached_comp_members;
  size_t cached_comp_count;
  pony_actor_t* cached_leader;
  cycle_record_t* known_cycles;
  dedup_conn_t* trace_dedup;
  candidate_record_t* candidate;
  distcd_conf_state_t conf_state;
  cycle_record_t* delegated_cycles;
} distcd_t;

typedef struct trace_route_msg_t
{
  pony_msg_t msg;
  size_t count;
  trace_entry_t* entries;
} trace_route_msg_t;

// Chained confirmation message. Accumulates confirmations as it walks the
// cycle topology. Each hop is to an actor in the sender's foreign map.
typedef struct confirm_chain_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t* appearances;
  size_t count;
  pony_actor_t* leader;
  uint64_t confirmed_bits;
  bool denied;
  pony_actor_t* denier;
} confirm_chain_msg_t;

// Chained delegation message. Routes through the component until it reaches
// the denier, who becomes the new leader.
typedef struct delegate_chain_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t count;
  pony_actor_t* denier;
  uint64_t visited_bits;
  cycle_record_t* cycles;
} delegate_chain_msg_t;

// Chained release message. Each member re-verifies, marks released, and
// forwards to its outgoing connections in the member list.
typedef struct release_chain_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t* appearances;
  size_t count;
  pony_actor_t* leader;
} release_chain_msg_t;

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

// Process chained CONFIRM_BLOCKED. Checks self, accumulates result, forwards.
void ponyint_distcd_handle_confirm_blocked(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_chain_msg_t* m);

// Process chained DELEGATE. Forwards until reaching the denier.
void ponyint_distcd_handle_delegate(pony_ctx_t* ctx,
  pony_actor_t* actor, delegate_chain_msg_t* m);

// Process chained RELEASE. Re-verifies, marks released, forwards.
void ponyint_distcd_handle_release(pony_ctx_t* ctx,
  pony_actor_t* actor, release_chain_msg_t* m);

// Returns false if released. Otherwise frees stale cycle state and returns true.
bool ponyint_distcd_can_self_reap(distcd_t* distcd);

// RELEASE has been received.
bool ponyint_distcd_released(distcd_t* distcd);

// If this actor is the natural leader of a cycle component and not already
// in a confirmation round, initiate one.
void ponyint_distcd_try_confirm(pony_ctx_t* ctx, pony_actor_t* actor);

PONY_EXTERN_C_END

#endif
