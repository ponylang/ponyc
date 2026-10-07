#ifndef gc_distcd_h
#define gc_distcd_h

#include "../pony.h"
#include "../ds/hash.h"
#include <stdint.h>
#include <stdbool.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef enum conf_state_t
{
  CONF_NONE = 0,
  CONF_COMPLETENESS_WAITING,
  CONF_ROUND1,
  CONF_ROUND2,
} conf_state_t;

typedef struct cycle_record_t
{
  pony_actor_t** members;
  size_t count;
} cycle_record_t;

typedef struct trace_entry_t
{
  pony_actor_t* actor;
  uint32_t epoch;
} trace_entry_t;

typedef struct trace_dedup_key_t
{
  pony_actor_t* target;
  pony_actor_t* originator;
  uint32_t epoch;
} trace_dedup_key_t;

DECLARE_HASHMAP(ponyint_trace_dedup, trace_dedup_map_t, trace_dedup_key_t);

typedef struct pending_trace_t
{
  trace_entry_t* entries;
  size_t count;
  struct pending_trace_t* next;
} pending_trace_t;

typedef struct dcd_outgoing_credit_t
{
  size_t target_index;
  size_t rc;
} dcd_outgoing_credit_t;

typedef struct member_credit_info_t
{
  size_t gc_rc;
  dcd_outgoing_credit_t* outgoing;
  size_t num_outgoing;
} member_credit_info_t;

typedef struct candidate_record_t
{
  pony_actor_t** members;
  size_t count;
  cycle_record_t* records;
  size_t num_records;
  uint64_t ack_bitmask;
  member_credit_info_t* credit_info;
} candidate_record_t;

typedef struct distcd_t
{
  uint32_t epoch;
  cycle_record_t* known_cycles;
  size_t num_known_cycles;
  size_t cap_known_cycles;
  bool retrace_needed;
  trace_dedup_map_t trace_dedup;
  candidate_record_t* candidate;
  conf_state_t conf_state;
  bool destroy_ready;
  bool confirmed_idle;
  pending_trace_t* pending_traces;
} distcd_t;

// DCD message IDs
#define ACTORMSG_DCD_TRACE_ROUTE     (UINT32_MAX - 14)
#define ACTORMSG_DCD_COMPLETENESS    (UINT32_MAX - 13)
#define ACTORMSG_DCD_COMP_RESPONSE   (UINT32_MAX - 12)
#define ACTORMSG_DCD_CONF            (UINT32_MAX - 11)
#define ACTORMSG_DCD_CONF_RESPONSE   (UINT32_MAX - 10)

// DCD message structs

typedef struct dcd_trace_route_msg_t
{
  pony_msg_t msg;
  trace_entry_t* entries;
  size_t count;
} dcd_trace_route_msg_t;

typedef struct dcd_completeness_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t count;
  cycle_record_t* cycle_records;
  size_t num_records;
  pony_actor_t* leader;
  size_t member_index;
} dcd_completeness_msg_t;

typedef struct dcd_comp_response_msg_t
{
  pony_msg_t msg;
  pony_actor_t* leader;
  size_t member_index;
  size_t gc_rc;
  dcd_outgoing_credit_t* outgoing;
  size_t num_outgoing;
} dcd_comp_response_msg_t;

typedef struct dcd_conf_msg_t
{
  pony_msg_t msg;
  pony_actor_t** members;
  size_t count;
  pony_actor_t* leader;
  uint32_t round;
} dcd_conf_msg_t;

typedef struct dcd_conf_response_msg_t
{
  pony_msg_t msg;
  pony_actor_t* leader;
  size_t member_index;
  uint32_t round;
  bool ack;
} dcd_conf_response_msg_t;

distcd_t* ponyint_distcd_create(void);

void ponyint_distcd_destroy(distcd_t* dcd);

bool ponyint_distcd_on_block(pony_ctx_t* ctx, pony_actor_t* actor,
  distcd_t* dcd);

void ponyint_distcd_handle_trace_route(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_trace_route_msg_t* msg);

void ponyint_distcd_handle_completeness(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_completeness_msg_t* msg);

void ponyint_distcd_handle_comp_response(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_comp_response_msg_t* msg);

void ponyint_distcd_handle_conf(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_conf_msg_t* msg);

void ponyint_distcd_handle_conf_response(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_conf_response_msg_t* msg);

void ponyint_distcd_on_acquire(pony_actor_t* actor);

void ponyint_distcd_on_release(pony_actor_t* actor);

void ponyint_distcd_on_sweep(pony_actor_t* actor, pony_actor_t* lost_target);

PONY_EXTERN_C_END

#endif
