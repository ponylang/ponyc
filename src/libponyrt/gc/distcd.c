#define PONY_WANT_ATOMIC_DEFS

#include "distcd.h"
#include "gc.h"
#include "actormap.h"
#include "../actor/actor.h"
#include "../actor/messageq.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <string.h>
#include <stdlib.h>

// Trace dedup hashmap implementation
static size_t trace_dedup_hash(trace_dedup_key_t* key)
{
  size_t h = (size_t)key->target;
  h ^= (size_t)key->originator * 2654435761u;
  h ^= (size_t)key->epoch * 2246822519u;
  return h;
}

static bool trace_dedup_cmp(trace_dedup_key_t* a, trace_dedup_key_t* b)
{
  return (a->target == b->target) &&
         (a->originator == b->originator) &&
         (a->epoch == b->epoch);
}

static void trace_dedup_free(trace_dedup_key_t* key)
{
  POOL_FREE(trace_dedup_key_t, key);
}

DEFINE_HASHMAP(ponyint_trace_dedup, trace_dedup_map_t, trace_dedup_key_t,
  trace_dedup_hash, trace_dedup_cmp, trace_dedup_free);

// Compare actor pointers for qsort
static int actor_ptr_cmp(const void* a, const void* b)
{
  uintptr_t pa = (uintptr_t)(*(pony_actor_t* const*)a);
  uintptr_t pb = (uintptr_t)(*(pony_actor_t* const*)b);
  return (pa > pb) - (pa < pb);
}

// Check if sorted array a (count_a) is a subset of sorted array b (count_b)
static bool is_subset(pony_actor_t** a, size_t count_a,
  pony_actor_t** b, size_t count_b)
{
  if(count_a > count_b)
    return false;

  size_t j = 0;
  for(size_t i = 0; i < count_a; i++)
  {
    while(j < count_b && b[j] < a[i])
      j++;
    if(j >= count_b || b[j] != a[i])
      return false;
    j++;
  }
  return true;
}

// Check if two sorted arrays are equal
static bool records_equal(pony_actor_t** a, size_t count_a,
  pony_actor_t** b, size_t count_b)
{
  if(count_a != count_b)
    return false;
  return memcmp(a, b, count_a * sizeof(pony_actor_t*)) == 0;
}

static void free_cycle_record(cycle_record_t* rec)
{
  if(rec->members != NULL)
    ponyint_pool_free_size(rec->count * sizeof(pony_actor_t*), rec->members);
}

static void free_candidate(candidate_record_t* cand)
{
  if(cand == NULL)
    return;

  if(cand->members != NULL)
    ponyint_pool_free_size(cand->count * sizeof(pony_actor_t*), cand->members);

  for(size_t i = 0; i < cand->num_records; i++)
    free_cycle_record(&cand->records[i]);

  if(cand->records != NULL)
    ponyint_pool_free_size(cand->num_records * sizeof(cycle_record_t),
      cand->records);

  if(cand->credit_info != NULL)
  {
    for(size_t i = 0; i < cand->count; i++)
    {
      if(cand->credit_info[i].outgoing != NULL)
        ponyint_pool_free_size(
          cand->credit_info[i].num_outgoing * sizeof(dcd_outgoing_credit_t),
          cand->credit_info[i].outgoing);
    }
    ponyint_pool_free_size(cand->count * sizeof(member_credit_info_t),
      cand->credit_info);
  }

  POOL_FREE(candidate_record_t, cand);
}

static void free_pending_traces(pending_trace_t* pt)
{
  while(pt != NULL)
  {
    pending_trace_t* next = pt->next;
    if(pt->entries != NULL)
      ponyint_pool_free_size(pt->count * sizeof(trace_entry_t), pt->entries);
    POOL_FREE(pending_trace_t, pt);
    pt = next;
  }
}

// Add a cycle record to known_cycles if not already present or subset
static bool add_cycle_record(distcd_t* dcd, pony_actor_t** members,
  size_t count)
{
  // Check against existing records
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    cycle_record_t* existing = &dcd->known_cycles[i];

    // If new record equals an existing one, reject
    if(records_equal(members, count, existing->members, existing->count))
      return false;

    // If new record is a subset of an existing one, reject
    if(is_subset(members, count, existing->members, existing->count))
      return false;
  }

  // Grow capacity if needed
  if(dcd->num_known_cycles == dcd->cap_known_cycles)
  {
    size_t new_cap = (dcd->cap_known_cycles == 0) ? 4 :
      dcd->cap_known_cycles * 2;
    cycle_record_t* new_arr = (cycle_record_t*)ponyint_pool_alloc_size(
      new_cap * sizeof(cycle_record_t));

    if(dcd->known_cycles != NULL)
    {
      memcpy(new_arr, dcd->known_cycles,
        dcd->num_known_cycles * sizeof(cycle_record_t));
      ponyint_pool_free_size(
        dcd->cap_known_cycles * sizeof(cycle_record_t), dcd->known_cycles);
    }

    dcd->known_cycles = new_arr;
    dcd->cap_known_cycles = new_cap;
  }

  // Copy the members array
  pony_actor_t** copy = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(copy, members, count * sizeof(pony_actor_t*));

  dcd->known_cycles[dcd->num_known_cycles].members = copy;
  dcd->known_cycles[dcd->num_known_cycles].count = count;
  dcd->num_known_cycles++;

  return true;
}

// Remove cycle records that contain a given actor from known_cycles
static void prune_records_containing(distcd_t* dcd, pony_actor_t* target)
{
  size_t write = 0;
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    bool contains = false;
    for(size_t j = 0; j < dcd->known_cycles[i].count; j++)
    {
      if(dcd->known_cycles[i].members[j] == target)
      {
        contains = true;
        break;
      }
    }

    if(contains)
    {
      free_cycle_record(&dcd->known_cycles[i]);
    }
    else
    {
      if(write != i)
        dcd->known_cycles[write] = dcd->known_cycles[i];
      write++;
    }
  }
  dcd->num_known_cycles = write;
}

// Remove specific cycle records (by index set) from known_cycles
static void remove_component_records(distcd_t* dcd, bool* in_component)
{
  size_t write = 0;
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    if(in_component[i])
    {
      free_cycle_record(&dcd->known_cycles[i]);
    }
    else
    {
      if(write != i)
        dcd->known_cycles[write] = dcd->known_cycles[i];
      write++;
    }
  }
  dcd->num_known_cycles = write;
}

// Bump epoch without clearing known_cycles. Safe when the topology change
// can only make cycles MORE complete (e.g. RELEASE decreases rc).
static void bump_epoch_light(distcd_t* dcd)
{
  dcd->epoch++;
  dcd->retrace_needed = true;
  ponyint_trace_dedup_destroy(&dcd->trace_dedup);
  ponyint_trace_dedup_init(&dcd->trace_dedup, 0);
}

// Bump epoch and clear known_cycles. Required when the topology change
// could invalidate discovered cycles (e.g. ACQUIRE adds a new edge).
static void bump_epoch_full(distcd_t* dcd)
{
  bump_epoch_light(dcd);

  for(size_t i = 0; i < dcd->num_known_cycles; i++)
    free_cycle_record(&dcd->known_cycles[i]);
  dcd->num_known_cycles = 0;
}

// Send a trace route message to a target actor
static void send_trace_route(pony_actor_t* target,
  trace_entry_t* entries, size_t count)
{
  dcd_trace_route_msg_t* m = (dcd_trace_route_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(dcd_trace_route_msg_t)), ACTORMSG_DCD_TRACE_ROUTE);

  m->entries = entries;
  m->count = count;

  ponyint_sendv_inject(target, &m->msg);
}

// Phase 1: Send trace routes to all outgoing connections
static void do_trace(pony_ctx_t* ctx, pony_actor_t* actor, distcd_t* dcd)
{
  (void)ctx;

  actormap_t* foreign = &actor->gc.foreign;
  size_t map_size = ponyint_actormap_size(foreign);

  if(map_size == 0)
  {
    dcd->retrace_needed = false;
    return;
  }

  size_t i = HASHMAP_BEGIN;
  actorref_t* aref;

  while((aref = ponyint_actormap_next(foreign, &i)) != NULL)
  {
    trace_entry_t* entries = (trace_entry_t*)ponyint_pool_alloc_size(
      sizeof(trace_entry_t));
    entries[0].actor = actor;
    entries[0].epoch = dcd->epoch;

    send_trace_route(aref->actor, entries, 1);
  }

  dcd->retrace_needed = false;
}

// Count how many of the given cycle records contain a given actor
static size_t count_appearances(pony_actor_t* actor,
  cycle_record_t* records, size_t num_records)
{
  size_t count = 0;
  for(size_t i = 0; i < num_records; i++)
  {
    for(size_t j = 0; j < records[i].count; j++)
    {
      if(records[i].members[j] == actor)
      {
        count++;
        break;
      }
    }
  }
  return count;
}

// Compute component: transitive closure of overlapping cycle records
// Returns: member list (sorted), member count, list of record indices,
// record count. Caller frees.
static bool compute_component(distcd_t* dcd,
  pony_actor_t*** out_members, size_t* out_member_count,
  cycle_record_t** out_records, size_t* out_num_records,
  bool** out_in_component)
{
  if(dcd->num_known_cycles == 0)
    return false;

  // Mark which records are in the component
  bool* in_component = (bool*)ponyint_pool_alloc_size(
    dcd->num_known_cycles * sizeof(bool));
  memset(in_component, 0, dcd->num_known_cycles * sizeof(bool));

  // Start with the first record
  in_component[0] = true;
  bool changed = true;

  while(changed)
  {
    changed = false;
    for(size_t i = 0; i < dcd->num_known_cycles; i++)
    {
      if(in_component[i])
        continue;

      // Check if record i overlaps with any in-component record
      for(size_t j = 0; j < dcd->num_known_cycles; j++)
      {
        if(!in_component[j])
          continue;

        // Check for shared member between records i and j
        for(size_t mi = 0; mi < dcd->known_cycles[i].count; mi++)
        {
          for(size_t mj = 0; mj < dcd->known_cycles[j].count; mj++)
          {
            if(dcd->known_cycles[i].members[mi] ==
               dcd->known_cycles[j].members[mj])
            {
              in_component[i] = true;
              changed = true;
              goto next_record;
            }
          }
        }
      }
      next_record:;
    }
  }

  // Collect unique members from component records
  size_t total_entries = 0;
  size_t num_comp_records = 0;
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    if(in_component[i])
    {
      total_entries += dcd->known_cycles[i].count;
      num_comp_records++;
    }
  }

  if(num_comp_records == 0)
  {
    ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
    return false;
  }

  // Temporary array to collect all member pointers (may have duplicates)
  pony_actor_t** all_members = (pony_actor_t**)ponyint_pool_alloc_size(
    total_entries * sizeof(pony_actor_t*));
  size_t idx = 0;
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    if(in_component[i])
    {
      for(size_t j = 0; j < dcd->known_cycles[i].count; j++)
        all_members[idx++] = dcd->known_cycles[i].members[j];
    }
  }

  // Sort and deduplicate
  qsort(all_members, total_entries, sizeof(pony_actor_t*), actor_ptr_cmp);

  size_t unique = 0;
  for(size_t i = 0; i < total_entries; i++)
  {
    if(unique == 0 || all_members[i] != all_members[unique - 1])
      all_members[unique++] = all_members[i];
  }

  // Shrink to unique count
  pony_actor_t** members = (pony_actor_t**)ponyint_pool_alloc_size(
    unique * sizeof(pony_actor_t*));
  memcpy(members, all_members, unique * sizeof(pony_actor_t*));
  ponyint_pool_free_size(total_entries * sizeof(pony_actor_t*), all_members);

  // Copy the component records
  cycle_record_t* comp_records = (cycle_record_t*)ponyint_pool_alloc_size(
    num_comp_records * sizeof(cycle_record_t));
  size_t ri = 0;
  for(size_t i = 0; i < dcd->num_known_cycles; i++)
  {
    if(in_component[i])
    {
      comp_records[ri].count = dcd->known_cycles[i].count;
      comp_records[ri].members = (pony_actor_t**)ponyint_pool_alloc_size(
        dcd->known_cycles[i].count * sizeof(pony_actor_t*));
      memcpy(comp_records[ri].members, dcd->known_cycles[i].members,
        dcd->known_cycles[i].count * sizeof(pony_actor_t*));
      ri++;
    }
  }

  *out_members = members;
  *out_member_count = unique;
  *out_records = comp_records;
  *out_num_records = num_comp_records;
  *out_in_component = in_component;

  return true;
}

// Pick leader: actor with most appearances across component records.
// Ties broken by lowest pointer address.
static pony_actor_t* pick_leader(pony_actor_t** members, size_t member_count,
  cycle_record_t* records, size_t num_records)
{
  pony_actor_t* leader = NULL;
  size_t max_appearances = 0;

  for(size_t i = 0; i < member_count; i++)
  {
    size_t appearances = count_appearances(members[i], records, num_records);
    if(appearances > max_appearances ||
       (appearances == max_appearances &&
        (leader == NULL || (uintptr_t)members[i] < (uintptr_t)leader)))
    {
      leader = members[i];
      max_appearances = appearances;
    }
  }

  return leader;
}

// Find member index in sorted member array
static size_t find_member_index(pony_actor_t* actor,
  pony_actor_t** members, size_t count)
{
  for(size_t i = 0; i < count; i++)
  {
    if(members[i] == actor)
      return i;
  }
  // Should never happen if actor is a member
  pony_assert(false);
  return 0;
}

// Phase 4: Destruction
// Returns true if destruction completed, false if a member isn't ready yet.
static bool do_destruction(pony_ctx_t* ctx, pony_actor_t* leader,
  candidate_record_t* cand)
{
  // Check that all non-leader members have finished their run loops
  // and called markempty. The markempty bit (bit 0 of head) proves
  // the member is sleeping — no concurrent access.
  for(size_t i = 0; i < cand->count; i++)
  {
    if(cand->members[i] != leader)
    {
      messageq_t* q = &cand->members[i]->q;
      pony_msg_t* head = atomic_load_explicit(&q->head,
        memory_order_acquire);
      if(((uintptr_t)head & 1) == 0)
        return false;
    }
  }

  // Step 1: setpendingdestroy on all members including leader
  for(size_t i = 0; i < cand->count; i++)
    ponyint_actor_setpendingdestroy(cand->members[i]);

  // Step 2: final on all members including leader
  for(size_t i = 0; i < cand->count; i++)
    ponyint_actor_final(ctx, cand->members[i]);

  // Step 3: sendrelease on all members including leader
  for(size_t i = 0; i < cand->count; i++)
    ponyint_actor_sendrelease(ctx, cand->members[i]);

  // Step 4: destroy non-leader members
  for(size_t i = 0; i < cand->count; i++)
  {
    if(cand->members[i] != leader)
    {
      // Free non-leader's distcd_t before destroying the actor
      distcd_t* member_dcd = ponyint_actor_get_distcd(cand->members[i]);
      if(member_dcd != NULL)
        ponyint_distcd_destroy(member_dcd);
      ponyint_actor_set_distcd(cand->members[i], NULL);

      ponyint_actor_destroy(cand->members[i], ACTOR_DESTROYED_CD_NORMAL);
    }
  }

  return true;
}

// try_confirm: run from the blocking path, tries to confirm a cycle
static void try_confirm(pony_actor_t* actor, distcd_t* dcd)
{
  if(dcd->confirmed_idle)
    return;

  if(dcd->num_known_cycles == 0)
    return;

  pony_actor_t** members = NULL;
  size_t member_count = 0;
  cycle_record_t* comp_records = NULL;
  size_t num_comp_records = 0;
  bool* in_component = NULL;

  if(!compute_component(dcd, &members, &member_count, &comp_records,
    &num_comp_records, &in_component))
    return;

  pony_actor_t* leader = pick_leader(members, member_count, comp_records,
    num_comp_records);

  if(leader != actor)
  {
    // Not the leader, clean up and return
    ponyint_pool_free_size(member_count * sizeof(pony_actor_t*), members);
    for(size_t i = 0; i < num_comp_records; i++)
      free_cycle_record(&comp_records[i]);
    ponyint_pool_free_size(num_comp_records * sizeof(cycle_record_t),
      comp_records);
    ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
    return;
  }

  if(dcd->conf_state != CONF_NONE)
  {
    // Already in progress
    ponyint_pool_free_size(member_count * sizeof(pony_actor_t*), members);
    for(size_t i = 0; i < num_comp_records; i++)
      free_cycle_record(&comp_records[i]);
    ponyint_pool_free_size(num_comp_records * sizeof(cycle_record_t),
      comp_records);
    ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
    return;
  }

  // If the leader is the only member, skip to phase 3 directly
  if(member_count == 1)
  {
    // Single-member cycle: all foreign map entries must point to self
    // and all credits must come from self
    actormap_t* foreign = &actor->gc.foreign;

    size_t fi = HASHMAP_BEGIN;
    actorref_t* ar;
    bool all_internal = true;
    size_t self_credits = 0;
    while((ar = ponyint_actormap_next(foreign, &fi)) != NULL)
    {
      if(ar->actor != actor)
        all_internal = false;
      else
        self_credits = ar->rc;
    }

    if(!all_internal || self_credits != actor->gc.rc)
    {
      ponyint_pool_free_size(member_count * sizeof(pony_actor_t*), members);
      for(size_t ci = 0; ci < num_comp_records; ci++)
        free_cycle_record(&comp_records[ci]);
      ponyint_pool_free_size(num_comp_records * sizeof(cycle_record_t),
        comp_records);
      ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
      return;
    }

    // Build candidate
    candidate_record_t* cand = POOL_ALLOC(candidate_record_t);
    memset(cand, 0, sizeof(candidate_record_t));
    cand->members = members;
    cand->count = member_count;
    cand->records = comp_records;
    cand->num_records = num_comp_records;
    cand->ack_bitmask = 0;
    cand->credit_info = NULL;

    dcd->candidate = cand;

    // No non-leader members to check or confirm
    // Go straight to checking quiescence for just the leader
    // Leader self-check
    if(!ponyint_messageq_isempty(&actor->q) || actor->live_asio_events != 0)
    {
      dcd->conf_state = CONF_NONE;
      remove_component_records(dcd, in_component);
      ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool),
        in_component);
      free_candidate(cand);
      dcd->candidate = NULL;
      return;
    }

    // Single member, both rounds pass trivially
    dcd->destroy_ready = true;
    dcd->conf_state = CONF_ROUND2;
    ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
    return;
  }

  // Build candidate with credit info array
  candidate_record_t* cand = POOL_ALLOC(candidate_record_t);
  memset(cand, 0, sizeof(candidate_record_t));
  cand->members = members;
  cand->count = member_count;
  cand->records = comp_records;
  cand->num_records = num_comp_records;
  cand->ack_bitmask = 0;
  cand->credit_info = (member_credit_info_t*)ponyint_pool_alloc_size(
    member_count * sizeof(member_credit_info_t));
  memset(cand->credit_info, 0, member_count * sizeof(member_credit_info_t));

  // Populate leader's credit info
  size_t leader_idx = find_member_index(actor, members, member_count);
  {
    actormap_t* foreign = &actor->gc.foreign;
    size_t nout = 0;

    for(size_t mi = 0; mi < member_count; mi++)
    {
      if(members[mi] == actor)
        continue;

      size_t idx = HASHMAP_UNKNOWN;
      actorref_t* ar = ponyint_actormap_getactor(foreign, members[mi], &idx);
      if(ar != NULL)
        nout++;
    }

    dcd_outgoing_credit_t* out = NULL;
    if(nout > 0)
    {
      out = (dcd_outgoing_credit_t*)ponyint_pool_alloc_size(
        nout * sizeof(dcd_outgoing_credit_t));
      size_t k = 0;
      for(size_t mi = 0; mi < member_count; mi++)
      {
        if(members[mi] == actor)
          continue;

        size_t idx = HASHMAP_UNKNOWN;
        actorref_t* ar = ponyint_actormap_getactor(foreign, members[mi], &idx);
        if(ar != NULL)
        {
          out[k].target_index = mi;
          out[k].rc = ar->rc;
          k++;
        }
      }
    }

    cand->credit_info[leader_idx].gc_rc = actor->gc.rc;
    cand->credit_info[leader_idx].outgoing = out;
    cand->credit_info[leader_idx].num_outgoing = nout;
  }

  dcd->candidate = cand;
  dcd->conf_state = CONF_COMPLETENESS_WAITING;

  // Send COMPLETENESS_CHECK to each non-leader member
  for(size_t i = 0; i < member_count; i++)
  {
    if(members[i] == actor)
      continue;

    dcd_completeness_msg_t* m = (dcd_completeness_msg_t*)pony_alloc_msg(
      POOL_INDEX(sizeof(dcd_completeness_msg_t)),
      ACTORMSG_DCD_COMPLETENESS);

    m->members = members;
    m->count = member_count;
    m->cycle_records = comp_records;
    m->num_records = num_comp_records;
    m->leader = actor;
    m->member_index = i;

    ponyint_sendv_inject(members[i], &m->msg);
  }

  ponyint_pool_free_size(dcd->num_known_cycles * sizeof(bool), in_component);
}

// Send CONF round to non-leader members
static void send_conf_round(pony_actor_t* leader, candidate_record_t* cand,
  uint32_t round)
{
  for(size_t i = 0; i < cand->count; i++)
  {
    if(cand->members[i] == leader)
      continue;

    dcd_conf_msg_t* m = (dcd_conf_msg_t*)pony_alloc_msg(
      POOL_INDEX(sizeof(dcd_conf_msg_t)), ACTORMSG_DCD_CONF);

    m->members = cand->members;
    m->count = cand->count;
    m->leader = leader;
    m->round = round;

    ponyint_sendv_inject(cand->members[i], &m->msg);
  }
}

// ============================================================
// Public API
// ============================================================

distcd_t* ponyint_distcd_create(void)
{
  distcd_t* dcd = POOL_ALLOC(distcd_t);
  memset(dcd, 0, sizeof(distcd_t));
  dcd->retrace_needed = true;
  ponyint_trace_dedup_init(&dcd->trace_dedup, 0);
  return dcd;
}

void ponyint_distcd_destroy(distcd_t* dcd)
{
  if(dcd == NULL)
    return;

  for(size_t i = 0; i < dcd->num_known_cycles; i++)
    free_cycle_record(&dcd->known_cycles[i]);

  if(dcd->known_cycles != NULL)
    ponyint_pool_free_size(
      dcd->cap_known_cycles * sizeof(cycle_record_t), dcd->known_cycles);

  ponyint_trace_dedup_destroy(&dcd->trace_dedup);

  free_candidate(dcd->candidate);
  free_pending_traces(dcd->pending_traces);

  POOL_FREE(distcd_t, dcd);
}

bool ponyint_distcd_on_block(pony_ctx_t* ctx, pony_actor_t* actor,
  distcd_t* dcd)
{
  // If destruction is ready, perform it
  if(dcd->destroy_ready)
  {
    pony_assert(dcd->candidate != NULL);
    if(!do_destruction(ctx, actor, dcd->candidate))
      return false;

    // do_destruction completed. The caller (run loop) will:
    // 1. markempty the leader's queue
    // 2. free the leader's distcd_t
    // 3. destroy the leader
    // 4. return false
    return true;
  }

  // Phase 1: trace if needed
  if(dcd->retrace_needed)
  {
    do_trace(ctx, actor, dcd);
  }

  // Phase 2/3: try to confirm a cycle
  try_confirm(actor, dcd);

  return false;
}

void ponyint_distcd_handle_trace_route(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_trace_route_msg_t* msg)
{
  (void)ctx;

  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL)
  {
    // Actor doesn't have DCD state, free the entries
    ponyint_pool_free_size(msg->count * sizeof(trace_entry_t), msg->entries);
    return;
  }

  trace_entry_t* entries = msg->entries;
  size_t count = msg->count;

  // Check if self appears in entries
  for(size_t i = 0; i < count; i++)
  {
    if(entries[i].actor == actor)
    {
      if(entries[i].epoch == dcd->epoch)
      {
        // Cycle found! Members = actors from position i through end
        size_t cycle_len = count - i;
        pony_actor_t** cycle_members = (pony_actor_t**)ponyint_pool_alloc_size(
          cycle_len * sizeof(pony_actor_t*));

        for(size_t j = 0; j < cycle_len; j++)
          cycle_members[j] = entries[i + j].actor;

        // Sort by pointer
        qsort(cycle_members, cycle_len, sizeof(pony_actor_t*), actor_ptr_cmp);

        // Try to add
        add_cycle_record(dcd, cycle_members, cycle_len);
        ponyint_pool_free_size(cycle_len * sizeof(pony_actor_t*),
          cycle_members);
      }
      // else: stale, discard

      ponyint_pool_free_size(count * sizeof(trace_entry_t), entries);
      return;
    }
  }

  // Forward the trace. Cap length to prevent unbounded growth.
  if(count >= 64)
  {
    ponyint_pool_free_size(count * sizeof(trace_entry_t), entries);
    return;
  }

  actormap_t* foreign = &actor->gc.foreign;
  size_t map_size = ponyint_actormap_size(foreign);

  if(map_size == 0)
  {
    // No outgoing connections, buffer as pending trace
    pending_trace_t* pt = POOL_ALLOC(pending_trace_t);
    pt->entries = entries;
    pt->count = count;
    pt->next = dcd->pending_traces;
    dcd->pending_traces = pt;
    return;
  }

  // Hash the originator to pick one target from the foreign map
  pony_actor_t* originator = entries[0].actor;
  uint32_t originator_epoch = entries[0].epoch;
  size_t hash = (size_t)originator * 2654435761u;
  size_t target_idx = hash % map_size;

  size_t fi = HASHMAP_BEGIN;
  actorref_t* fwd_aref = NULL;
  for(size_t n = 0; n <= target_idx; n++)
    fwd_aref = ponyint_actormap_next(foreign, &fi);

  pony_assert(fwd_aref != NULL);
  pony_actor_t* target = fwd_aref->actor;

  // Check trace_dedup
  trace_dedup_key_t lookup_key;
  lookup_key.target = target;
  lookup_key.originator = originator;
  lookup_key.epoch = originator_epoch;

  size_t dedup_idx = HASHMAP_UNKNOWN;
  trace_dedup_key_t* existing = ponyint_trace_dedup_get(&dcd->trace_dedup,
    &lookup_key, &dedup_idx);

  if(existing != NULL)
  {
    if(existing->epoch == originator_epoch)
    {
      // Already forwarded this trace
      ponyint_pool_free_size(count * sizeof(trace_entry_t), entries);
      return;
    }
    existing->epoch = originator_epoch;
  }
  else
  {
    trace_dedup_key_t* key = POOL_ALLOC(trace_dedup_key_t);
    key->target = target;
    key->originator = originator;
    key->epoch = originator_epoch;
    ponyint_trace_dedup_put(&dcd->trace_dedup, key);
  }

  // Extend entries with self and forward
  trace_entry_t* new_entries = (trace_entry_t*)ponyint_pool_alloc_size(
    (count + 1) * sizeof(trace_entry_t));
  memcpy(new_entries, entries, count * sizeof(trace_entry_t));
  new_entries[count].actor = actor;
  new_entries[count].epoch = dcd->epoch;

  send_trace_route(target, new_entries, count + 1);

  ponyint_pool_free_size(count * sizeof(trace_entry_t), entries);
}

void ponyint_distcd_handle_completeness(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_completeness_msg_t* msg)
{
  (void)ctx;

  // Compute outgoing credits to cycle members from our foreign map
  actormap_t* foreign = &actor->gc.foreign;
  size_t num_outgoing = 0;

  // First pass: count
  for(size_t i = 0; i < msg->count; i++)
  {
    if(msg->members[i] == actor)
      continue;

    size_t idx = HASHMAP_UNKNOWN;
    actorref_t* aref = ponyint_actormap_getactor(foreign, msg->members[i],
      &idx);

    if(aref != NULL)
      num_outgoing++;
  }

  dcd_outgoing_credit_t* outgoing = NULL;
  if(num_outgoing > 0)
  {
    outgoing = (dcd_outgoing_credit_t*)ponyint_pool_alloc_size(
      num_outgoing * sizeof(dcd_outgoing_credit_t));

    size_t k = 0;
    for(size_t i = 0; i < msg->count; i++)
    {
      if(msg->members[i] == actor)
        continue;

      size_t idx = HASHMAP_UNKNOWN;
      actorref_t* aref = ponyint_actormap_getactor(foreign, msg->members[i],
        &idx);

      if(aref != NULL)
      {
        outgoing[k].target_index = i;
        outgoing[k].rc = aref->rc;
        k++;
      }
    }
  }

  // Collect extra cycle records the member knows about that aren't in the
  // leader's component records. These help the leader discover transitively
  // overlapping cycles.
  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  cycle_record_t* extra_records = NULL;
  size_t num_extra = 0;

  if(dcd != NULL && dcd->num_known_cycles > 0)
  {
    // Count records not already in the leader's set
    for(size_t r = 0; r < dcd->num_known_cycles; r++)
    {
      bool found_in_leader = false;
      for(size_t lr = 0; lr < msg->num_records; lr++)
      {
        if(records_equal(dcd->known_cycles[r].members,
           dcd->known_cycles[r].count,
           msg->cycle_records[lr].members,
           msg->cycle_records[lr].count))
        {
          found_in_leader = true;
          break;
        }
      }
      if(!found_in_leader)
        num_extra++;
    }

    if(num_extra > 0)
    {
      extra_records = (cycle_record_t*)ponyint_pool_alloc_size(
        num_extra * sizeof(cycle_record_t));
      size_t ei = 0;
      for(size_t r = 0; r < dcd->num_known_cycles; r++)
      {
        bool found_in_leader = false;
        for(size_t lr = 0; lr < msg->num_records; lr++)
        {
          if(records_equal(dcd->known_cycles[r].members,
             dcd->known_cycles[r].count,
             msg->cycle_records[lr].members,
             msg->cycle_records[lr].count))
          {
            found_in_leader = true;
            break;
          }
        }
        if(!found_in_leader)
        {
          extra_records[ei].count = dcd->known_cycles[r].count;
          extra_records[ei].members =
            (pony_actor_t**)ponyint_pool_alloc_size(
              dcd->known_cycles[r].count * sizeof(pony_actor_t*));
          memcpy(extra_records[ei].members, dcd->known_cycles[r].members,
            dcd->known_cycles[r].count * sizeof(pony_actor_t*));
          ei++;
        }
      }
    }
  }

  // Send response to leader with credit info and extra records
  dcd_comp_response_msg_t* resp = (dcd_comp_response_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(dcd_comp_response_msg_t)),
    ACTORMSG_DCD_COMP_RESPONSE);

  resp->leader = msg->leader;
  resp->member_index = msg->member_index;
  resp->gc_rc = actor->gc.rc;
  resp->outgoing = outgoing;
  resp->num_outgoing = num_outgoing;
  resp->extra_records = extra_records;
  resp->num_extra_records = num_extra;

  ponyint_sendv_inject(msg->leader, &resp->msg);
}

static void completeness_nack(distcd_t* dcd)
{
  dcd->conf_state = CONF_NONE;
  free_candidate(dcd->candidate);
  dcd->candidate = NULL;
}

void ponyint_distcd_handle_comp_response(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_comp_response_msg_t* msg)
{
  (void)ctx;

  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL || dcd->candidate == NULL ||
     dcd->conf_state != CONF_COMPLETENESS_WAITING)
  {
    if(msg->outgoing != NULL)
      ponyint_pool_free_size(
        msg->num_outgoing * sizeof(dcd_outgoing_credit_t), msg->outgoing);
    for(size_t i = 0; i < msg->num_extra_records; i++)
      free_cycle_record(&msg->extra_records[i]);
    if(msg->extra_records != NULL)
      ponyint_pool_free_size(
        msg->num_extra_records * sizeof(cycle_record_t), msg->extra_records);
    return;
  }

  // Merge extra records into our known_cycles
  bool merged_any = false;
  for(size_t i = 0; i < msg->num_extra_records; i++)
  {
    if(add_cycle_record(dcd, msg->extra_records[i].members,
       msg->extra_records[i].count))
      merged_any = true;
    free_cycle_record(&msg->extra_records[i]);
  }
  if(msg->extra_records != NULL)
    ponyint_pool_free_size(
      msg->num_extra_records * sizeof(cycle_record_t), msg->extra_records);

  // Store this member's credit info
  pony_assert(msg->member_index < dcd->candidate->count);
  member_credit_info_t* ci = &dcd->candidate->credit_info[msg->member_index];
  ci->gc_rc = msg->gc_rc;
  ci->outgoing = msg->outgoing;
  ci->num_outgoing = msg->num_outgoing;

  // Mark this member as responded
  pony_assert(msg->member_index < 64);
  dcd->candidate->ack_bitmask |= ((uint64_t)1 << msg->member_index);

  // Check if all non-leader members have ACKed
  size_t leader_idx = find_member_index(actor, dcd->candidate->members,
    dcd->candidate->count);

  uint64_t expected = 0;
  for(size_t i = 0; i < dcd->candidate->count; i++)
  {
    if(i != leader_idx)
      expected |= ((uint64_t)1 << i);
  }

  if((dcd->candidate->ack_bitmask & expected) != expected)
    return;

  // If we merged new records, the component may have grown.
  // Restart the completeness check with the expanded component.
  if(merged_any)
  {
    free_candidate(dcd->candidate);
    dcd->candidate = NULL;
    dcd->conf_state = CONF_NONE;
    // try_confirm will re-compute with the expanded known_cycles
    try_confirm(actor, dcd);
    return;
  }

  // All members responded. Cross-check: for each member M, the sum of
  // credits held by other cycle members must equal M's gc.rc.
  candidate_record_t* cand = dcd->candidate;
  bool complete = true;

  for(size_t m = 0; m < cand->count; m++)
  {
    size_t incoming_credits = 0;

    for(size_t src = 0; src < cand->count; src++)
    {
      if(src == m)
        continue;

      member_credit_info_t* sci = &cand->credit_info[src];
      for(size_t k = 0; k < sci->num_outgoing; k++)
      {
        if(sci->outgoing[k].target_index == m)
        {
          incoming_credits += sci->outgoing[k].rc;
          break;
        }
      }
    }

    if(incoming_credits != cand->credit_info[m].gc_rc)
    {
      complete = false;
      break;
    }
  }

  if(!complete)
  {
    completeness_nack(dcd);
    return;
  }

  // All members passed completeness, proceed to confirmation round 1
  dcd->conf_state = CONF_ROUND1;
  dcd->candidate->ack_bitmask = 0;

  // Leader self-check before round 1
  if(!ponyint_messageq_isempty(&actor->q) || actor->live_asio_events != 0)
  {
    dcd->conf_state = CONF_NONE;
    free_candidate(dcd->candidate);
    dcd->candidate = NULL;
    return;
  }

  send_conf_round(actor, dcd->candidate, 1);
}

void ponyint_distcd_handle_conf(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_conf_msg_t* msg)
{
  (void)ctx;

  distcd_t* dcd = ponyint_actor_get_distcd(actor);

  // Check queue is empty NOW (after consuming the CONF message)
  // and no live ASIO events
  bool queue_empty = ponyint_messageq_isempty(&actor->q);
  bool no_asio = (actor->live_asio_events == 0);
  bool ack = queue_empty && no_asio;

  // Find our member_index
  size_t member_index = find_member_index(actor, msg->members, msg->count);

  // If ACKing round 2, set confirmed_idle
  if(ack && msg->round == 2 && dcd != NULL)
    dcd->confirmed_idle = true;

  // Send response to leader
  dcd_conf_response_msg_t* resp = (dcd_conf_response_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(dcd_conf_response_msg_t)),
    ACTORMSG_DCD_CONF_RESPONSE);

  resp->leader = msg->leader;
  resp->member_index = member_index;
  resp->round = msg->round;
  resp->ack = ack;

  ponyint_sendv_inject(msg->leader, &resp->msg);
}

void ponyint_distcd_handle_conf_response(pony_ctx_t* ctx, pony_actor_t* actor,
  dcd_conf_response_msg_t* msg)
{
  (void)ctx;

  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL || dcd->candidate == NULL)
    return;

  if(msg->round == 1 && dcd->conf_state != CONF_ROUND1)
    return;
  if(msg->round == 2 && dcd->conf_state != CONF_ROUND2)
    return;

  if(!msg->ack)
  {
    dcd->conf_state = CONF_NONE;
    free_candidate(dcd->candidate);
    dcd->candidate = NULL;
    return;
  }

  // ACK: mark this member
  pony_assert(msg->member_index < 64);
  dcd->candidate->ack_bitmask |= ((uint64_t)1 << msg->member_index);

  // Check if all non-leader members have ACKed this round
  size_t leader_idx = find_member_index(actor, dcd->candidate->members,
    dcd->candidate->count);

  uint64_t expected = 0;
  for(size_t i = 0; i < dcd->candidate->count; i++)
  {
    if(i != leader_idx)
      expected |= ((uint64_t)1 << i);
  }

  if((dcd->candidate->ack_bitmask & expected) != expected)
    return;

  if(msg->round == 1)
  {
    // All ACKed round 1, proceed to round 2
    dcd->conf_state = CONF_ROUND2;
    dcd->candidate->ack_bitmask = 0;

    // Leader self-check before round 2
    if(!ponyint_messageq_isempty(&actor->q) || actor->live_asio_events != 0)
    {
      dcd->conf_state = CONF_NONE;
      free_candidate(dcd->candidate);
      dcd->candidate = NULL;
      return;
    }

    send_conf_round(actor, dcd->candidate, 2);
  }
  else
  {
    // All ACKed round 2: set destroy_ready, deferred to on_block
    dcd->destroy_ready = true;
  }
}

void ponyint_distcd_on_acquire(pony_actor_t* actor)
{
  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL)
    return;

  bump_epoch_full(dcd);

  // Process pending traces if we now have outgoing connections
  if(dcd->pending_traces != NULL &&
     ponyint_actormap_size(&actor->gc.foreign) > 0)
  {
    pending_trace_t* pt = dcd->pending_traces;
    dcd->pending_traces = NULL;

    while(pt != NULL)
    {
      pending_trace_t* next = pt->next;

      // Re-inject pending trace to one target (hash-selected)
      actormap_t* foreign = &actor->gc.foreign;
      size_t fmap_size = ponyint_actormap_size(foreign);
      pony_actor_t* pt_originator = pt->entries[0].actor;
      size_t pt_hash = (size_t)pt_originator * 2654435761u;
      size_t pt_target_idx = pt_hash % fmap_size;

      size_t fi = HASHMAP_BEGIN;
      actorref_t* fwd_aref = NULL;
      for(size_t n = 0; n <= pt_target_idx; n++)
        fwd_aref = ponyint_actormap_next(foreign, &fi);

      pony_assert(fwd_aref != NULL);

      trace_entry_t* new_entries = (trace_entry_t*)ponyint_pool_alloc_size(
        (pt->count + 1) * sizeof(trace_entry_t));
      memcpy(new_entries, pt->entries, pt->count * sizeof(trace_entry_t));
      new_entries[pt->count].actor = actor;
      new_entries[pt->count].epoch = dcd->epoch;

      send_trace_route(fwd_aref->actor, new_entries, pt->count + 1);

      ponyint_pool_free_size(pt->count * sizeof(trace_entry_t), pt->entries);
      POOL_FREE(pending_trace_t, pt);
      pt = next;
    }
  }
}

void ponyint_distcd_on_release(pony_actor_t* actor)
{
  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL)
    return;

  bump_epoch_light(dcd);
}

void ponyint_distcd_on_sweep(pony_actor_t* actor, pony_actor_t* lost_target)
{
  distcd_t* dcd = ponyint_actor_get_distcd(actor);
  if(dcd == NULL)
    return;

  // Prune cycle records containing the lost target
  prune_records_containing(dcd, lost_target);

  // Remove dedup entries for the lost target
  size_t i = HASHMAP_BEGIN;
  trace_dedup_key_t* key;
  while((key = ponyint_trace_dedup_next(&dcd->trace_dedup, &i)) != NULL)
  {
    if(key->target == lost_target)
    {
      ponyint_trace_dedup_removeindex(&dcd->trace_dedup, i);
      POOL_FREE(trace_dedup_key_t, key);
      i = HASHMAP_BEGIN;
    }
  }

  bump_epoch_full(dcd);
}
