#include "distcd.h"
#include "actormap.h"
#include "gc.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <string.h>
#include <stdlib.h>

PONY_EXTERN_C_BEGIN

// Push a DCD protocol message, bypassing pony_sendv's pendingdestroy
// assertion. A target actor can be concurrently destroyed by another
// scheduler thread between the caller's pendingdestroy check and the
// send — the message is drained harmlessly by the receiver's
// post-destruction drain loop.
static void distcd_send(pony_ctx_t* ctx, pony_actor_t* to, pony_msg_t* msg)
{
  if(ponyint_actor_messageq_push(&to->q, msg, msg))
    ponyint_sched_add(ctx, to);
}

static uint64_t fnv1a_init()
{
  return 14695981039346656037ULL;
}

static uint64_t fnv1a_update(uint64_t hash, const void* data, size_t len)
{
  const uint8_t* bytes = (const uint8_t*)data;
  for(size_t i = 0; i < len; i++)
  {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static uint64_t chain_hash(trace_entry_t* entries, size_t count)
{
  uint64_t h = fnv1a_init();
  for(size_t i = 0; i < count; i++)
  {
    h = fnv1a_update(h, &entries[i].actor, sizeof(pony_actor_t*));
    h = fnv1a_update(h, &entries[i].epoch, sizeof(uint32_t));
  }
  return h;
}

static int ptr_cmp(const void* a, const void* b)
{
  pony_actor_t* pa = *(pony_actor_t**)a;
  pony_actor_t* pb = *(pony_actor_t**)b;
  if(pa < pb) return -1;
  if(pa > pb) return 1;
  return 0;
}

static bool cycle_equals(cycle_record_t* a, cycle_record_t* b)
{
  if(a->count != b->count)
    return false;
  return memcmp(a->members, b->members,
    a->count * sizeof(pony_actor_t*)) == 0;
}

static bool cycle_is_subset(cycle_record_t* sub, cycle_record_t* super)
{
  if(sub->count > super->count)
    return false;

  size_t si = 0;
  for(size_t i = 0; i < sub->count; i++)
  {
    while(si < super->count && super->members[si] < sub->members[i])
      si++;
    if(si >= super->count || super->members[si] != sub->members[i])
      return false;
    si++;
  }
  return true;
}

static bool actor_in_cycle(pony_actor_t* actor, cycle_record_t* cycle)
{
  for(size_t i = 0; i < cycle->count; i++)
  {
    if(cycle->members[i] == actor)
      return true;
    if(cycle->members[i] > actor)
      return false;
  }
  return false;
}

static void cycle_record_free(cycle_record_t* rec)
{
  ponyint_pool_free_size(rec->count * sizeof(pony_actor_t*), rec->members);
  ponyint_pool_free_size(sizeof(cycle_record_t), rec);
}

static void free_cycle_list(cycle_record_t* list)
{
  while(list != NULL)
  {
    cycle_record_t* next = list->next;
    cycle_record_free(list);
    list = next;
  }
}

static void free_dedup(dedup_conn_t* list)
{
  while(list != NULL)
  {
    dedup_conn_t* next = list->next;
    dedup_chain_t* chain = list->chains;
    while(chain != NULL)
    {
      dedup_chain_t* cnext = chain->next;
      ponyint_pool_free_size(sizeof(dedup_chain_t), chain);
      chain = cnext;
    }
    ponyint_pool_free_size(sizeof(dedup_conn_t), list);
    list = next;
  }
}

static bool dedup_check_and_add(distcd_t* distcd, pony_actor_t* target,
  uint64_t hash)
{
  dedup_conn_t* conn = distcd->trace_dedup;
  while(conn != NULL)
  {
    if(conn->target == target)
    {
      dedup_chain_t* chain = conn->chains;
      while(chain != NULL)
      {
        if(chain->hash == hash)
          return true;
        chain = chain->next;
      }
      dedup_chain_t* new_chain = (dedup_chain_t*)ponyint_pool_alloc_size(
        sizeof(dedup_chain_t));
      new_chain->hash = hash;
      new_chain->next = conn->chains;
      conn->chains = new_chain;
      return false;
    }
    conn = conn->next;
  }

  dedup_conn_t* new_conn = (dedup_conn_t*)ponyint_pool_alloc_size(
    sizeof(dedup_conn_t));
  new_conn->target = target;
  dedup_chain_t* new_chain = (dedup_chain_t*)ponyint_pool_alloc_size(
    sizeof(dedup_chain_t));
  new_chain->hash = hash;
  new_chain->next = NULL;
  new_conn->chains = new_chain;
  new_conn->next = distcd->trace_dedup;
  distcd->trace_dedup = new_conn;
  return false;
}

// Returns true if known_cycles changed
static bool add_cycle_if_new(distcd_t* distcd, pony_actor_t** members,
  size_t count)
{
  cycle_record_t candidate;
  candidate.count = count;
  pony_actor_t** sorted = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(sorted, members, count * sizeof(pony_actor_t*));
  qsort(sorted, count, sizeof(pony_actor_t*), ptr_cmp);
  candidate.members = sorted;

  cycle_record_t* cur = distcd->known_cycles;
  while(cur != NULL)
  {
    if(cycle_equals(cur, &candidate) || cycle_is_subset(&candidate, cur))
    {
      ponyint_pool_free_size(count * sizeof(pony_actor_t*), sorted);
      return false;
    }
    cur = cur->next;
  }

  // Remove existing cycles that are subsets of the new one
  cycle_record_t** prev = &distcd->known_cycles;
  cur = distcd->known_cycles;
  while(cur != NULL)
  {
    if(cycle_is_subset(cur, &candidate))
    {
      *prev = cur->next;
      cycle_record_free(cur);
      cur = *prev;
    } else {
      prev = &cur->next;
      cur = cur->next;
    }
  }

  cycle_record_t* rec = (cycle_record_t*)ponyint_pool_alloc_size(
    sizeof(cycle_record_t));
  rec->count = count;
  rec->members = sorted;
  rec->next = distcd->known_cycles;
  distcd->known_cycles = rec;
  return true;
}

static size_t count_actor_appearances(pony_actor_t* actor,
  cycle_record_t* cycles)
{
  size_t count = 0;
  while(cycles != NULL)
  {
    if(actor_in_cycle(actor, cycles))
      count++;
    cycles = cycles->next;
  }
  return count;
}

static bool cycles_overlap(cycle_record_t* a, cycle_record_t* b)
{
  size_t ai = 0, bi = 0;
  while(ai < a->count && bi < b->count)
  {
    if(a->members[ai] == b->members[bi])
      return true;
    if(a->members[ai] < b->members[bi])
      ai++;
    else
      bi++;
  }
  return false;
}

// Compute connected component containing this actor.
// Returns a flat sorted array of unique member pointers and count.
// component_cycles is set to the list of cycles in the component.
static void compute_component(distcd_t* distcd, pony_actor_t* actor,
  pony_actor_t*** out_members, size_t* out_count,
  cycle_record_t** out_cycles, size_t* out_cycle_count)
{
  // Collect cycles reachable from any cycle containing actor
  size_t cap = 8;
  size_t n = 0;
  cycle_record_t** included = (cycle_record_t**)ponyint_pool_alloc_size(
    cap * sizeof(cycle_record_t*));

  // Seed: all cycles containing this actor
  cycle_record_t* cur = distcd->known_cycles;
  while(cur != NULL)
  {
    if(actor_in_cycle(actor, cur))
    {
      if(n >= cap)
      {
        size_t newcap = cap * 2;
        cycle_record_t** newarr = (cycle_record_t**)ponyint_pool_alloc_size(
          newcap * sizeof(cycle_record_t*));
        memcpy(newarr, included, n * sizeof(cycle_record_t*));
        ponyint_pool_free_size(cap * sizeof(cycle_record_t*), included);
        included = newarr;
        cap = newcap;
      }
      included[n++] = cur;
    }
    cur = cur->next;
  }

  // Transitive expansion
  bool changed = true;
  while(changed)
  {
    changed = false;
    cur = distcd->known_cycles;
    while(cur != NULL)
    {
      bool already = false;
      for(size_t i = 0; i < n; i++)
      {
        if(included[i] == cur)
        {
          already = true;
          break;
        }
      }
      if(!already)
      {
        for(size_t i = 0; i < n; i++)
        {
          if(cycles_overlap(cur, included[i]))
          {
            if(n >= cap)
            {
              size_t newcap = cap * 2;
              cycle_record_t** newarr =
                (cycle_record_t**)ponyint_pool_alloc_size(
                  newcap * sizeof(cycle_record_t*));
              memcpy(newarr, included, n * sizeof(cycle_record_t*));
              ponyint_pool_free_size(cap * sizeof(cycle_record_t*), included);
              included = newarr;
              cap = newcap;
            }
            included[n++] = cur;
            changed = true;
            break;
          }
        }
      }
      cur = cur->next;
    }
  }

  // Collect unique members
  size_t mcap = 8;
  size_t mn = 0;
  pony_actor_t** members = (pony_actor_t**)ponyint_pool_alloc_size(
    mcap * sizeof(pony_actor_t*));

  for(size_t i = 0; i < n; i++)
  {
    for(size_t j = 0; j < included[i]->count; j++)
    {
      pony_actor_t* m = included[i]->members[j];
      bool found = false;
      for(size_t k = 0; k < mn; k++)
      {
        if(members[k] == m)
        {
          found = true;
          break;
        }
      }
      if(!found)
      {
        if(mn >= mcap)
        {
          size_t newcap = mcap * 2;
          pony_actor_t** newarr = (pony_actor_t**)ponyint_pool_alloc_size(
            newcap * sizeof(pony_actor_t*));
          memcpy(newarr, members, mn * sizeof(pony_actor_t*));
          ponyint_pool_free_size(mcap * sizeof(pony_actor_t*), members);
          members = newarr;
          mcap = newcap;
        }
        members[mn++] = m;
      }
    }
  }

  qsort(members, mn, sizeof(pony_actor_t*), ptr_cmp);

  // Shrink to exact size so callers can free with mn * sizeof(...)
  if((mn > 0) && (mn < mcap))
  {
    pony_actor_t** exact = (pony_actor_t**)ponyint_pool_alloc_size(
      mn * sizeof(pony_actor_t*));
    memcpy(exact, members, mn * sizeof(pony_actor_t*));
    ponyint_pool_free_size(mcap * sizeof(pony_actor_t*), members);
    members = exact;
  } else if(mn == 0) {
    ponyint_pool_free_size(mcap * sizeof(pony_actor_t*), members);
    members = NULL;
  }

  *out_members = members;
  *out_count = mn;
  *out_cycles = (n > 0) ? included[0] : NULL;
  *out_cycle_count = n;

  ponyint_pool_free_size(cap * sizeof(cycle_record_t*), included);
}

// Determine leader of a component's cycles
static pony_actor_t* compute_leader(cycle_record_t* cycles, size_t cycle_count,
  pony_actor_t** comp_members, size_t comp_count)
{
  (void)cycle_count;

  pony_actor_t* leader = NULL;
  size_t max_appearances = 0;

  for(size_t i = 0; i < comp_count; i++)
  {
    size_t appearances = count_actor_appearances(comp_members[i], cycles);
    if(appearances > max_appearances ||
      (appearances == max_appearances &&
        (leader == NULL || comp_members[i] < leader)))
    {
      max_appearances = appearances;
      leader = comp_members[i];
    }
  }

  return leader;
}

static void send_trace_route(pony_ctx_t* ctx, pony_actor_t* to,
  trace_entry_t* entries, size_t count)
{
  if(ponyint_actor_pendingdestroy(to))
    return;

  trace_route_msg_t* m = (trace_route_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(trace_route_msg_t)), ACTORMSG_TRACE_ROUTE_DCD);

  m->count = count;
  m->entries = (trace_entry_t*)ponyint_pool_alloc_size(
    count * sizeof(trace_entry_t));
  memcpy(m->entries, entries, count * sizeof(trace_entry_t));

  distcd_send(ctx, to, &m->msg);
}

static void send_confirm_msg(pony_ctx_t* ctx, pony_actor_t* to,
  uint32_t msg_id, pony_actor_t** members, size_t count,
  pony_actor_t* leader)
{
  if(ponyint_actor_pendingdestroy(to))
    return;

  confirm_msg_t* m = (confirm_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(confirm_msg_t)), msg_id);

  m->count = count;
  m->leader = leader;
  m->members = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(m->members, members, count * sizeof(pony_actor_t*));

  distcd_send(ctx, to, &m->msg);
}

static void send_gossip(pony_ctx_t* ctx, pony_actor_t* actor)
{
  distcd_t* distcd = actor->distcd;
  if(distcd->known_cycles == NULL)
    return;

  pony_actor_t** comp_members;
  size_t comp_count;
  cycle_record_t* comp_cycles;
  size_t comp_cycle_count;

  compute_component(distcd, actor, &comp_members, &comp_count,
    &comp_cycles, &comp_cycle_count);

  if(comp_count <= 1)
  {
    ponyint_pool_free_size(comp_count * sizeof(pony_actor_t*), comp_members);
    return;
  }

  // Count total member entries across all component cycles
  size_t total_members = 0;
  size_t num_cycles = 0;
  cycle_record_t* cur = distcd->known_cycles;
  while(cur != NULL)
  {
    bool in_comp = false;
    for(size_t i = 0; i < comp_count; i++)
    {
      if(actor_in_cycle(comp_members[i], cur))
      {
        in_comp = true;
        break;
      }
    }
    if(in_comp)
    {
      total_members += cur->count;
      num_cycles++;
    }
    cur = cur->next;
  }

  for(size_t i = 0; i < comp_count; i++)
  {
    if(comp_members[i] == actor)
      continue;

    inform_cycles_msg_t* m = (inform_cycles_msg_t*)pony_alloc_msg(
      POOL_INDEX(sizeof(inform_cycles_msg_t)), ACTORMSG_INFORM_CYCLES_DCD);

    m->num_cycles = num_cycles;
    m->total_members = total_members;
    m->cycle_sizes = (size_t*)ponyint_pool_alloc_size(
      num_cycles * sizeof(size_t));
    m->cycle_members = (pony_actor_t**)ponyint_pool_alloc_size(
      total_members * sizeof(pony_actor_t*));

    size_t ci = 0;
    size_t mi = 0;
    cur = distcd->known_cycles;
    while(cur != NULL)
    {
      bool in_comp = false;
      for(size_t j = 0; j < comp_count; j++)
      {
        if(actor_in_cycle(comp_members[j], cur))
        {
          in_comp = true;
          break;
        }
      }
      if(in_comp)
      {
        m->cycle_sizes[ci++] = cur->count;
        memcpy(&m->cycle_members[mi], cur->members,
          cur->count * sizeof(pony_actor_t*));
        mi += cur->count;
      }
      cur = cur->next;
    }

    if(ponyint_actor_pendingdestroy(comp_members[i]))
    {
      ponyint_pool_free_size(m->num_cycles * sizeof(size_t), m->cycle_sizes);
      ponyint_pool_free_size(m->total_members * sizeof(pony_actor_t*),
        m->cycle_members);
      ponyint_pool_free(m->msg.index, m);
    } else {
      distcd_send(ctx, comp_members[i], &m->msg);
    }
  }

  ponyint_pool_free_size(comp_count * sizeof(pony_actor_t*), comp_members);
}

distcd_t* ponyint_distcd_create()
{
  distcd_t* distcd = (distcd_t*)ponyint_pool_alloc_size(sizeof(distcd_t));
  memset(distcd, 0, sizeof(distcd_t));
  return distcd;
}

void ponyint_distcd_destroy(distcd_t* distcd)
{
  if(distcd == NULL)
    return;

  free_cycle_list(distcd->known_cycles);
  free_dedup(distcd->trace_dedup);

  if(distcd->candidate != NULL)
  {
    if(distcd->candidate->members != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(pony_actor_t*),
        distcd->candidate->members);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
  }

  ponyint_pool_free_size(sizeof(distcd_t), distcd);
}

void ponyint_distcd_connection_lost(pony_actor_t* actor, pony_actor_t* target)
{
  distcd_t* distcd = actor->distcd;
  if(distcd == NULL || distcd->released)
    return;

  distcd->epoch++;

  free_dedup(distcd->trace_dedup);
  distcd->trace_dedup = NULL;

  // Prune known_cycles involving the target
  cycle_record_t** prev = &distcd->known_cycles;
  cycle_record_t* cur = distcd->known_cycles;
  while(cur != NULL)
  {
    if(actor_in_cycle(target, cur))
    {
      *prev = cur->next;
      cycle_record_free(cur);
      cur = *prev;
    } else {
      prev = &cur->next;
      cur = cur->next;
    }
  }
}

void ponyint_distcd_on_block(pony_ctx_t* ctx, pony_actor_t* actor)
{
  gc_t* gc = ponyint_actor_gc(actor);
  size_t foreign_count = ponyint_actormap_size(&gc->foreign);

  if(foreign_count == 0)
    return;

  distcd_t* distcd = actor->distcd;
  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  if(distcd->released)
    return;

  trace_entry_t entry;
  entry.actor = actor;
  entry.epoch = distcd->epoch;

  size_t idx = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(&gc->foreign, &idx)) != NULL)
  {
    send_trace_route(ctx, aref->actor, &entry, 1);
  }
}

void ponyint_distcd_handle_trace_route(pony_ctx_t* ctx, pony_actor_t* actor,
  trace_route_msg_t* m)
{
  // If actor has no outgoing CONNECTIONs, discard
  gc_t* gc = ponyint_actor_gc(actor);
  size_t foreign_count = ponyint_actormap_size(&gc->foreign);

  if(foreign_count == 0)
  {
    ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
    return;
  }

  // Lazy-allocate distcd (trace can arrive before the actor blocks)
  distcd_t* distcd = actor->distcd;
  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  if(distcd->released)
  {
    ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
    return;
  }

  // Check if this actor appears in the trace
  ssize_t self_pos = -1;
  for(size_t i = 0; i < m->count; i++)
  {
    if(m->entries[i].actor == actor)
    {
      self_pos = (ssize_t)i;
      break;
    }
  }

  if(self_pos >= 0)
  {
    if(m->entries[self_pos].epoch != distcd->epoch)
    {
      // Stale trace
      ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
      return;
    }

    // Cycle found — extract from self_pos to end
    size_t cycle_len = m->count - (size_t)self_pos;
    pony_actor_t** cycle_members = (pony_actor_t**)ponyint_pool_alloc_size(
      cycle_len * sizeof(pony_actor_t*));
    for(size_t i = 0; i < cycle_len; i++)
      cycle_members[i] = m->entries[(size_t)self_pos + i].actor;

    bool changed = add_cycle_if_new(distcd, cycle_members, cycle_len);

    ponyint_pool_free_size(cycle_len * sizeof(pony_actor_t*), cycle_members);
    ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);

    if(changed)
      send_gossip(ctx, actor);

    return;
  }

  // Not in trace — augment and forward
  size_t new_count = m->count + 1;
  trace_entry_t* augmented = (trace_entry_t*)ponyint_pool_alloc_size(
    new_count * sizeof(trace_entry_t));
  memcpy(augmented, m->entries, m->count * sizeof(trace_entry_t));
  augmented[m->count].actor = actor;
  augmented[m->count].epoch = distcd->epoch;

  uint64_t aug_hash = chain_hash(augmented, new_count);

  // Forward to each outgoing CONNECTION, checking dedup.
  // Skip targets in known_cycles — those actors may be destroyed or
  // pending destruction and sending to them is unsafe.
  size_t idx = HASHMAP_BEGIN;
  actorref_t* aref;
  while((aref = ponyint_actormap_next(&gc->foreign, &idx)) != NULL)
  {
    bool in_cycle = false;
    if(distcd->known_cycles != NULL)
    {
      cycle_record_t* cr = distcd->known_cycles;
      while(cr != NULL && !in_cycle)
      {
        if(actor_in_cycle(aref->actor, cr))
          in_cycle = true;
        cr = cr->next;
      }
    }

    if(!in_cycle && !dedup_check_and_add(distcd, aref->actor, aug_hash))
    {
      send_trace_route(ctx, aref->actor, augmented, new_count);
    }
  }

  ponyint_pool_free_size(new_count * sizeof(trace_entry_t), augmented);
  ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
}

void ponyint_distcd_handle_inform_cycles(pony_ctx_t* ctx,
  pony_actor_t* actor, inform_cycles_msg_t* m)
{
  distcd_t* distcd = actor->distcd;

  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  bool changed = false;
  size_t offset = 0;
  for(size_t i = 0; i < m->num_cycles; i++)
  {
    size_t sz = m->cycle_sizes[i];
    if(add_cycle_if_new(distcd, &m->cycle_members[offset], sz))
      changed = true;
    offset += sz;
  }

  ponyint_pool_free_size(m->num_cycles * sizeof(size_t), m->cycle_sizes);
  ponyint_pool_free_size(m->total_members * sizeof(pony_actor_t*),
    m->cycle_members);

  if(changed)
    send_gossip(ctx, actor);
}

void ponyint_distcd_handle_confirm_blocked(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m)
{
  if(actor->distcd == NULL)
    actor->distcd = ponyint_distcd_create();

  distcd_t* distcd = actor->distcd;
  if(distcd->released)
  {
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    return;
  }

  bool queue_ok = ponyint_messageq_isempty(&actor->q);

  if(queue_ok)
  {
    send_confirm_msg(ctx, m->leader, ACTORMSG_CONFIRMED_DCD,
      m->members, m->count, m->leader);
  } else {
    send_confirm_msg(ctx, m->leader, ACTORMSG_DENIED_DCD,
      m->members, m->count, m->leader);
  }

  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
}

void ponyint_distcd_handle_confirmed(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m)
{
  distcd_t* distcd = actor->distcd;

  if(distcd == NULL || distcd->conf_state != DISTCD_CONF_LEADER_WAITING ||
    distcd->candidate == NULL)
  {
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    return;
  }

  distcd->candidate->confirmed_count++;

  // All members except leader confirmed?
  if(distcd->candidate->confirmed_count >= distcd->candidate->count - 1)
  {
    // Re-verify leader's queue before sending RELEASE
    bool queue_ok = ponyint_messageq_isempty(&actor->q);

    if(queue_ok)
    {
      for(size_t i = 0; i < distcd->candidate->count; i++)
      {
        send_confirm_msg(ctx, distcd->candidate->members[i],
          ACTORMSG_RELEASE_DCD, distcd->candidate->members,
          distcd->candidate->count, actor);
      }

      distcd->conf_state = DISTCD_CONF_NONE;
    } else {
      // Leader isn't idle — abandon this confirmation round.
      distcd->conf_state = DISTCD_CONF_NONE;
    }

    ponyint_pool_free_size(
      distcd->candidate->count * sizeof(pony_actor_t*),
      distcd->candidate->members);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
    distcd->candidate = NULL;
  }

  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
}

void ponyint_distcd_handle_denied(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m)
{
  distcd_t* distcd = actor->distcd;

  if(distcd == NULL || distcd->conf_state != DISTCD_CONF_LEADER_WAITING ||
    distcd->candidate == NULL)
  {
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    return;
  }

  // Messages carry no sender field, so delegate to the first non-leader
  // member instead of the actual denier.
  pony_actor_t* delegate_to = NULL;
  for(size_t i = 0; i < distcd->candidate->count; i++)
  {
    if(distcd->candidate->members[i] != actor)
    {
      delegate_to = distcd->candidate->members[i];
      break;
    }
  }

  if(delegate_to != NULL)
  {
    send_confirm_msg(ctx, delegate_to, ACTORMSG_DELEGATE_DCD,
      distcd->candidate->members, distcd->candidate->count, delegate_to);
  }

  ponyint_pool_free_size(
    distcd->candidate->count * sizeof(pony_actor_t*),
    distcd->candidate->members);
  ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
  distcd->candidate = NULL;
  distcd->conf_state = DISTCD_CONF_NONE;

  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
}

void ponyint_distcd_handle_delegate(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m)
{
  (void)ctx;
  distcd_t* distcd = actor->distcd;

  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  // Store the candidate for re-confirmation on next scheduler run
  if(distcd->candidate != NULL)
  {
    ponyint_pool_free_size(
      distcd->candidate->count * sizeof(pony_actor_t*),
      distcd->candidate->members);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
  }

  distcd->candidate = (candidate_record_t*)ponyint_pool_alloc_size(
    sizeof(candidate_record_t));
  distcd->candidate->members = m->members;
  distcd->candidate->count = m->count;
  distcd->candidate->leader = actor;
  distcd->candidate->confirmed_count = 0;

  distcd->conf_state = DISTCD_CONF_MEMBER_PENDING;
  // Don't free m->members — ownership transferred to candidate
}

void ponyint_distcd_handle_release(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_msg_t* m)
{
  (void)ctx;
  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);

  distcd_t* distcd = actor->distcd;
  if(distcd != NULL)
  {
    distcd->released = true;

    cycle_record_t* cur = distcd->known_cycles;
    while(cur != NULL)
    {
      cycle_record_t* next = cur->next;
      ponyint_pool_free_size(cur->count * sizeof(pony_actor_t*), cur->members);
      ponyint_pool_free_size(sizeof(cycle_record_t), cur);
      cur = next;
    }
    distcd->known_cycles = NULL;

    if(distcd->candidate != NULL)
    {
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(pony_actor_t*),
        distcd->candidate->members);
      ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
      distcd->candidate = NULL;
    }
    distcd->conf_state = DISTCD_CONF_NONE;
  }
}

bool ponyint_distcd_can_self_reap(distcd_t* distcd)
{
  if(distcd == NULL)
    return true;

  return (distcd->known_cycles == NULL) &&
    (distcd->conf_state == DISTCD_CONF_NONE);
}

bool ponyint_distcd_released(distcd_t* distcd)
{
  if(distcd == NULL)
    return false;

  return distcd->released;
}

void ponyint_distcd_try_confirm(pony_ctx_t* ctx, pony_actor_t* actor)
{
  distcd_t* distcd = actor->distcd;
  if(distcd == NULL)
    return;

  if(distcd->released)
    return;

  // If we have a pending delegate, try to become leader
  if(distcd->conf_state == DISTCD_CONF_MEMBER_PENDING &&
    distcd->candidate != NULL)
  {
    for(size_t i = 0; i < distcd->candidate->count; i++)
    {
      if(distcd->candidate->members[i] != actor)
      {
        send_confirm_msg(ctx, distcd->candidate->members[i],
          ACTORMSG_CONFIRM_BLOCKED_DCD, distcd->candidate->members,
          distcd->candidate->count, actor);
      }
    }
    distcd->candidate->confirmed_count = 0;
    distcd->conf_state = DISTCD_CONF_LEADER_WAITING;
    return;
  }

  // If we're naturally the leader of a component, try to confirm
  if(distcd->conf_state != DISTCD_CONF_NONE || distcd->known_cycles == NULL)
    return;

  pony_actor_t** comp_members;
  size_t comp_count;
  cycle_record_t* comp_cycles;
  size_t comp_cycle_count;

  compute_component(distcd, actor, &comp_members, &comp_count,
    &comp_cycles, &comp_cycle_count);

  if(comp_count == 0)
    return;

  pony_actor_t* leader = compute_leader(comp_cycles, comp_cycle_count,
    comp_members, comp_count);

  if(leader != actor)
  {
    ponyint_pool_free_size(comp_count * sizeof(pony_actor_t*), comp_members);
    return;
  }

  // We're the leader and conditions hold — initiate confirmation
  distcd->candidate = (candidate_record_t*)ponyint_pool_alloc_size(
    sizeof(candidate_record_t));
  distcd->candidate->members = comp_members;
  distcd->candidate->count = comp_count;
  distcd->candidate->leader = actor;
  distcd->candidate->confirmed_count = 0;

  distcd->conf_state = DISTCD_CONF_LEADER_WAITING;

  for(size_t i = 0; i < comp_count; i++)
  {
    if(comp_members[i] != actor)
    {
      send_confirm_msg(ctx, comp_members[i], ACTORMSG_CONFIRM_BLOCKED_DCD,
        comp_members, comp_count, actor);
    }
  }
}

PONY_EXTERN_C_END
