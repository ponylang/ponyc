#include "distcd.h"
#include "actormap.h"
#include "gc.h"
#include "../actor/actor.h"
#include "../ds/fun.h"
#include "../mem/pool.h"
#include "ponyassert.h"
#include <string.h>
#include <stdlib.h>

PONY_EXTERN_C_BEGIN

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

static bool cycle_within_members(cycle_record_t* cycle,
  pony_actor_t** members, size_t member_count)
{
  size_t mi = 0;
  for(size_t i = 0; i < cycle->count; i++)
  {
    while(mi < member_count && members[mi] < cycle->members[i])
      mi++;
    if(mi >= member_count || members[mi] != cycle->members[i])
      return false;
    mi++;
  }
  return true;
}

static size_t count_appearances_in_component(pony_actor_t* actor,
  cycle_record_t* known_cycles, pony_actor_t** comp_members,
  size_t comp_count)
{
  size_t count = 0;
  cycle_record_t* cur = known_cycles;
  while(cur != NULL)
  {
    if(actor_in_cycle(actor, cur) &&
      cycle_within_members(cur, comp_members, comp_count))
      count++;
    cur = cur->next;
  }
  return count;
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

static void invalidate_component_cache(distcd_t* distcd)
{
  if(distcd->cached_comp_members != NULL)
  {
    ponyint_pool_free_size(
      distcd->cached_comp_count * sizeof(pony_actor_t*),
      distcd->cached_comp_members);
    distcd->cached_comp_members = NULL;
    distcd->cached_comp_count = 0;
    distcd->cached_leader = NULL;
  }
}

static bool dedup_check_and_add(distcd_t* distcd, pony_actor_t* target,
  pony_actor_t* originator, uint32_t orig_epoch)
{
  dedup_conn_t* conn = distcd->trace_dedup;
  while(conn != NULL)
  {
    if(conn->target == target)
    {
      dedup_chain_t* chain = conn->chains;
      while(chain != NULL)
      {
        if(chain->originator == originator && chain->epoch == orig_epoch)
          return true;
        chain = chain->next;
      }
      dedup_chain_t* new_chain = (dedup_chain_t*)ponyint_pool_alloc_size(
        sizeof(dedup_chain_t));
      new_chain->originator = originator;
      new_chain->epoch = orig_epoch;
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
  new_chain->originator = originator;
  new_chain->epoch = orig_epoch;
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

  cycle_record_t* rec = (cycle_record_t*)ponyint_pool_alloc_size(
    sizeof(cycle_record_t));
  rec->count = count;
  rec->members = sorted;
  rec->next = distcd->known_cycles;
  distcd->known_cycles = rec;
  distcd->cycles_generation++;
  return true;
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
// out_cycles is set to the list of cycles in the component.
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

static pony_actor_t* compute_leader(cycle_record_t* known_cycles,
  pony_actor_t** comp_members, size_t comp_count)
{
  pony_actor_t* leader = NULL;
  size_t max_appearances = 0;

  for(size_t i = 0; i < comp_count; i++)
  {
    size_t appearances = count_appearances_in_component(comp_members[i],
      known_cycles, comp_members, comp_count);
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

static void get_component_and_leader(distcd_t* distcd, pony_actor_t* actor,
  pony_actor_t*** out_members, size_t* out_count,
  pony_actor_t** out_leader)
{
  if(distcd->cached_generation == distcd->cycles_generation &&
    distcd->cached_comp_members != NULL)
  {
    *out_count = distcd->cached_comp_count;
    *out_members = (pony_actor_t**)ponyint_pool_alloc_size(
      distcd->cached_comp_count * sizeof(pony_actor_t*));
    memcpy(*out_members, distcd->cached_comp_members,
      distcd->cached_comp_count * sizeof(pony_actor_t*));
    if(out_leader != NULL)
      *out_leader = distcd->cached_leader;
    return;
  }

  cycle_record_t* comp_cycles;
  size_t comp_cycle_count;
  compute_component(distcd, actor, out_members, out_count,
    &comp_cycles, &comp_cycle_count);

  pony_actor_t* leader = NULL;
  if(*out_count > 0)
    leader = compute_leader(distcd->known_cycles, *out_members, *out_count);

  invalidate_component_cache(distcd);
  if(*out_count > 0)
  {
    distcd->cached_comp_count = *out_count;
    distcd->cached_comp_members = (pony_actor_t**)ponyint_pool_alloc_size(
      *out_count * sizeof(pony_actor_t*));
    memcpy(distcd->cached_comp_members, *out_members,
      *out_count * sizeof(pony_actor_t*));
    distcd->cached_leader = leader;
  }
  distcd->cached_generation = distcd->cycles_generation;

  if(out_leader != NULL)
    *out_leader = leader;
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

  pony_sendv(ctx, to, &m->msg, &m->msg, false);
}

static cycle_record_t* deep_copy_cycles(cycle_record_t* src)
{
  cycle_record_t* dst_head = NULL;
  cycle_record_t** dst_tail = &dst_head;
  while(src != NULL)
  {
    cycle_record_t* copy = (cycle_record_t*)ponyint_pool_alloc_size(
      sizeof(cycle_record_t));
    copy->count = src->count;
    copy->members = (pony_actor_t**)ponyint_pool_alloc_size(
      src->count * sizeof(pony_actor_t*));
    memcpy(copy->members, src->members, src->count * sizeof(pony_actor_t*));
    copy->next = NULL;
    *dst_tail = copy;
    dst_tail = &copy->next;
    src = src->next;
  }
  return dst_head;
}

static bool cycle_already_known(cycle_record_t* known, cycle_record_t* candidate)
{
  while(known != NULL)
  {
    if(known->count == candidate->count)
    {
      bool match = true;
      for(size_t i = 0; i < candidate->count && match; i++)
      {
        bool found = false;
        for(size_t j = 0; j < known->count; j++)
        {
          if(candidate->members[i] == known->members[j])
          {
            found = true;
            break;
          }
        }
        if(!found) match = false;
      }
      if(match) return true;
    }
    known = known->next;
  }
  return false;
}

static size_t find_member_index(pony_actor_t* actor,
  pony_actor_t** members, size_t count)
{
  for(size_t i = 0; i < count; i++)
  {
    if(members[i] == actor)
      return i;
  }
  return SIZE_MAX;
}

static size_t find_outgoing_members(pony_actor_t* actor,
  pony_actor_t** members, size_t count, uint64_t exclude_bits,
  pony_actor_t** out_targets)
{
  gc_t* gc = ponyint_actor_gc(actor);
  size_t found = 0;
  for(size_t i = 0; i < count; i++)
  {
    if(members[i] == actor)
      continue;
    if(exclude_bits & ((uint64_t)1 << i))
      continue;
    size_t index = HASHMAP_UNKNOWN;
    if(ponyint_actormap_getactor(&gc->foreign, members[i], &index) != NULL)
      out_targets[found++] = members[i];
  }
  return found;
}

static void free_confirm_chain_arrays(confirm_chain_msg_t* m)
{
  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
  ponyint_pool_free_size(m->count * sizeof(size_t), m->appearances);
}

static void send_confirm_chain(pony_ctx_t* ctx, pony_actor_t* to,
  pony_actor_t** members, size_t* appearances, size_t count,
  pony_actor_t* leader, uint64_t confirmed_bits,
  bool denied, pony_actor_t* denier)
{
  if(ponyint_actor_pendingdestroy(to))
    return;

  confirm_chain_msg_t* m = (confirm_chain_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(confirm_chain_msg_t)), ACTORMSG_CONFIRM_BLOCKED_DCD);

  m->count = count;
  m->leader = leader;
  m->confirmed_bits = confirmed_bits;
  m->denied = denied;
  m->denier = denier;
  m->members = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(m->members, members, count * sizeof(pony_actor_t*));
  m->appearances = (size_t*)ponyint_pool_alloc_size(
    count * sizeof(size_t));
  memcpy(m->appearances, appearances, count * sizeof(size_t));

  pony_sendv(ctx, to, &m->msg, &m->msg, false);
}

static void send_delegate_chain(pony_ctx_t* ctx, pony_actor_t* to,
  pony_actor_t** members, size_t count,
  pony_actor_t* denier, uint64_t visited_bits,
  cycle_record_t* cycles)
{
  if(ponyint_actor_pendingdestroy(to))
    return;

  delegate_chain_msg_t* m = (delegate_chain_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(delegate_chain_msg_t)), ACTORMSG_DELEGATE_DCD);

  m->count = count;
  m->denier = denier;
  m->visited_bits = visited_bits;
  m->members = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(m->members, members, count * sizeof(pony_actor_t*));
  m->cycles = deep_copy_cycles(cycles);

  pony_sendv(ctx, to, &m->msg, &m->msg, false);
}

static void send_release_chain(pony_ctx_t* ctx, pony_actor_t* to,
  pony_actor_t** members, size_t* appearances, size_t count,
  pony_actor_t* leader)
{
  if(ponyint_actor_pendingdestroy(to))
    return;

  release_chain_msg_t* m = (release_chain_msg_t*)pony_alloc_msg(
    POOL_INDEX(sizeof(release_chain_msg_t)), ACTORMSG_RELEASE_DCD);

  m->count = count;
  m->leader = leader;
  m->members = (pony_actor_t**)ponyint_pool_alloc_size(
    count * sizeof(pony_actor_t*));
  memcpy(m->members, members, count * sizeof(pony_actor_t*));
  m->appearances = (size_t*)ponyint_pool_alloc_size(
    count * sizeof(size_t));
  memcpy(m->appearances, appearances, count * sizeof(size_t));

  pony_sendv(ctx, to, &m->msg, &m->msg, false);
}

static void prune_unreachable_cycles(pony_actor_t* actor, distcd_t* distcd)
{
  gc_t* gc = ponyint_actor_gc(actor);
  cycle_record_t** prev = &distcd->known_cycles;
  cycle_record_t* cur = distcd->known_cycles;
  bool pruned = false;

  while(cur != NULL)
  {
    bool has_reachable = false;
    for(size_t i = 0; i < cur->count; i++)
    {
      if(cur->members[i] == actor)
        continue;
      size_t index = HASHMAP_UNKNOWN;
      if(ponyint_actormap_getactor(&gc->foreign, cur->members[i], &index)
        != NULL)
      {
        has_reachable = true;
        break;
      }
    }

    if(!has_reachable)
    {
      *prev = cur->next;
      cycle_record_free(cur);
      cur = *prev;
      pruned = true;
    } else {
      prev = &cur->next;
      cur = cur->next;
    }
  }

  if(pruned)
    distcd->cycles_generation++;
}

distcd_t* ponyint_distcd_create()
{
  distcd_t* distcd = (distcd_t*)ponyint_pool_alloc_size(sizeof(distcd_t));
  memset(distcd, 0, sizeof(distcd_t));
  distcd->cached_generation = UINT32_MAX;
  return distcd;
}

void ponyint_distcd_destroy(distcd_t* distcd)
{
  if(distcd == NULL)
    return;

  invalidate_component_cache(distcd);
  free_cycle_list(distcd->known_cycles);
  free_dedup(distcd->trace_dedup);

  if(distcd->candidate != NULL)
  {
    if(distcd->candidate->members != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(pony_actor_t*),
        distcd->candidate->members);
    if(distcd->candidate->appearances != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(size_t),
        distcd->candidate->appearances);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
  }

  if(distcd->delegated_cycles != NULL)
    free_cycle_list(distcd->delegated_cycles);

  ponyint_pool_free_size(sizeof(distcd_t), distcd);
}

void ponyint_distcd_connection_lost(pony_actor_t* actor, pony_actor_t* target)
{
  distcd_t* distcd = actor->distcd;
  if(distcd == NULL || distcd->released)
    return;

  distcd->epoch++;
  distcd->retrace_needed = true;

  dedup_conn_t** dedup_prev = &distcd->trace_dedup;
  dedup_conn_t* dedup_cur = distcd->trace_dedup;
  while(dedup_cur != NULL)
  {
    if(dedup_cur->target == target)
    {
      *dedup_prev = dedup_cur->next;
      dedup_chain_t* chain = dedup_cur->chains;
      while(chain != NULL)
      {
        dedup_chain_t* cnext = chain->next;
        ponyint_pool_free_size(sizeof(dedup_chain_t), chain);
        chain = cnext;
      }
      ponyint_pool_free_size(sizeof(dedup_conn_t), dedup_cur);
      break;
    }
    dedup_prev = &dedup_cur->next;
    dedup_cur = dedup_cur->next;
  }

  // Prune known_cycles involving the target
  bool pruned = false;
  cycle_record_t** prev = &distcd->known_cycles;
  cycle_record_t* cur = distcd->known_cycles;
  while(cur != NULL)
  {
    if(actor_in_cycle(target, cur))
    {
      *prev = cur->next;
      cycle_record_free(cur);
      cur = *prev;
      pruned = true;
    } else {
      prev = &cur->next;
      cur = cur->next;
    }
  }

  if(pruned)
    distcd->cycles_generation++;

  // If the lost target is in an active confirmation's candidate,
  // abandon the confirmation round — the target may never respond.
  if(distcd->candidate != NULL)
  {
    for(size_t i = 0; i < distcd->candidate->count; i++)
    {
      if(distcd->candidate->members[i] == target)
      {
        ponyint_pool_free_size(
          distcd->candidate->count * sizeof(pony_actor_t*),
          distcd->candidate->members);
        if(distcd->candidate->appearances != NULL)
          ponyint_pool_free_size(
            distcd->candidate->count * sizeof(size_t),
            distcd->candidate->appearances);
        ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
        distcd->candidate = NULL;
        distcd->conf_state = DISTCD_CONF_NONE;
        break;
      }
    }
  }

  if(distcd->delegated_cycles != NULL)
  {
    cycle_record_t* dc = distcd->delegated_cycles;
    bool invalidate = false;
    while(dc != NULL && !invalidate)
    {
      if(actor_in_cycle(target, dc))
        invalidate = true;
      dc = dc->next;
    }
    if(invalidate)
    {
      free_cycle_list(distcd->delegated_cycles);
      distcd->delegated_cycles = NULL;
    }
  }
}

bool ponyint_distcd_on_acquire(pony_ctx_t* ctx, pony_actor_t* actor,
  pony_actor_t* target)
{
  gc_t* gc = ponyint_actor_gc(actor);
  if(gc->rc == 0)
    return false;

  distcd_t* distcd = actor->distcd;
  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  if(distcd->released)
    return false;

  trace_entry_t entry;
  entry.actor = actor;
  entry.epoch = distcd->epoch;

  send_trace_route(ctx, target, &entry, 1);
  return true;
}

void ponyint_distcd_on_block(pony_ctx_t* ctx, pony_actor_t* actor)
{
  distcd_t* distcd = actor->distcd;
  if(distcd == NULL)
    return;

  if(distcd->released)
    return;

  if(distcd->retrace_needed || !distcd->initial_trace_done)
  {
    distcd->retrace_needed = false;
    distcd->initial_trace_done = true;
    gc_t* gc = ponyint_actor_gc(actor);
    trace_entry_t entry;
    entry.actor = actor;
    entry.epoch = distcd->epoch;
    size_t idx = HASHMAP_BEGIN;
    actorref_t* aref;
    while((aref = ponyint_actormap_next(&gc->foreign, &idx)) != NULL)
    {
      aref->traced = true;
      send_trace_route(ctx, aref->actor, &entry, 1);
    }
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

    add_cycle_if_new(distcd, cycle_members, cycle_len);

    ponyint_pool_free_size(cycle_len * sizeof(pony_actor_t*), cycle_members);
    ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
    return;
  }

  // Not in trace — augment and forward
  size_t new_count = m->count + 1;
  trace_entry_t* augmented = (trace_entry_t*)ponyint_pool_alloc_size(
    new_count * sizeof(trace_entry_t));
  memcpy(augmented, m->entries, m->count * sizeof(trace_entry_t));
  augmented[m->count].actor = actor;
  augmented[m->count].epoch = distcd->epoch;

  pony_actor_t* originator = m->entries[0].actor;
  uint32_t orig_epoch = m->entries[0].epoch;

  size_t target_idx = ponyint_hash_ptr(originator) % foreign_count;
  size_t idx = HASHMAP_BEGIN;
  actorref_t* aref = NULL;
  for(size_t i = 0; i <= target_idx; i++)
    aref = ponyint_actormap_next(&gc->foreign, &idx);
  pony_assert(aref != NULL);

  if(!dedup_check_and_add(distcd, aref->actor, originator, orig_epoch))
  {
    send_trace_route(ctx, aref->actor, augmented, new_count);
  }

  ponyint_pool_free_size(new_count * sizeof(trace_entry_t), augmented);
  ponyint_pool_free_size(m->count * sizeof(trace_entry_t), m->entries);
}

void ponyint_distcd_handle_confirm_blocked(pony_ctx_t* ctx,
  pony_actor_t* actor, confirm_chain_msg_t* m)
{
  if(m->leader == actor)
  {
    distcd_t* distcd = actor->distcd;
    if(distcd == NULL || distcd->conf_state != DISTCD_CONF_LEADER_WAITING ||
      distcd->candidate == NULL)
    {
      free_confirm_chain_arrays(m);
      return;
    }

    distcd->candidate->confirmed_bits |= m->confirmed_bits;
    if(m->denied)
    {
      distcd->candidate->denied = true;
      distcd->candidate->denier = m->denier;
    }

    uint64_t all_bits = (distcd->candidate->count == 64)
      ? UINT64_MAX : (((uint64_t)1 << distcd->candidate->count) - 1);

    if((distcd->candidate->confirmed_bits & all_bits) == all_bits)
    {
      if(!distcd->candidate->denied)
      {
        bool blocked = ponyint_messageq_isempty(&actor->q)
          && (actor->live_asio_events == 0);

        if(blocked)
        {
          size_t leader_idx = find_member_index(actor,
            distcd->candidate->members, distcd->candidate->count);
          pony_assert(leader_idx != SIZE_MAX);
          blocked = (actor->gc.rc ==
            distcd->candidate->appearances[leader_idx]);
        }

        if(blocked)
        {
          distcd->released = true;
          free_cycle_list(distcd->known_cycles);
          distcd->known_cycles = NULL;
          distcd->cycles_generation++;

          pony_actor_t* targets[64];
          size_t target_count = find_outgoing_members(actor,
            distcd->candidate->members, distcd->candidate->count,
            0, targets);

          for(size_t i = 0; i < target_count; i++)
          {
            send_release_chain(ctx, targets[i],
              distcd->candidate->members, distcd->candidate->appearances,
              distcd->candidate->count, actor);
          }
        }
      }
      else
      {
        size_t my_idx = find_member_index(actor,
          distcd->candidate->members, distcd->candidate->count);
        uint64_t visited = (my_idx != SIZE_MAX)
          ? ((uint64_t)1 << my_idx) : 0;

        pony_actor_t* targets[64];
        size_t target_count = find_outgoing_members(actor,
          distcd->candidate->members, distcd->candidate->count,
          0, targets);

        if(target_count > 0)
        {
          send_delegate_chain(ctx, targets[0],
            distcd->candidate->members, distcd->candidate->count,
            distcd->candidate->denier, visited, distcd->known_cycles);
        }
      }

      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(pony_actor_t*),
        distcd->candidate->members);
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(size_t),
        distcd->candidate->appearances);
      ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
      distcd->candidate = NULL;
      distcd->conf_state = DISTCD_CONF_NONE;
    }

    free_confirm_chain_arrays(m);
    return;
  }

  if(actor->distcd == NULL)
    actor->distcd = ponyint_distcd_create();

  distcd_t* distcd = actor->distcd;
  if(distcd->released)
  {
    free_confirm_chain_arrays(m);
    return;
  }

  size_t my_idx = find_member_index(actor, m->members, m->count);
  if(my_idx == SIZE_MAX)
  {
    free_confirm_chain_arrays(m);
    return;
  }

  uint64_t my_bit = (uint64_t)1 << my_idx;

  if(m->confirmed_bits & my_bit)
  {
    gc_t* gc = ponyint_actor_gc(actor);
    size_t index = HASHMAP_UNKNOWN;
    if(ponyint_actormap_getactor(&gc->foreign, m->leader, &index) != NULL)
    {
      send_confirm_chain(ctx, m->leader, m->members, m->appearances,
        m->count, m->leader, m->confirmed_bits, m->denied, m->denier);
    }
    else
    {
      pony_actor_t* targets[64];
      size_t target_count = find_outgoing_members(actor, m->members,
        m->count, 0, targets);
      if(target_count > 0)
      {
        send_confirm_chain(ctx, targets[0], m->members, m->appearances,
          m->count, m->leader, m->confirmed_bits, m->denied, m->denier);
      }
    }
    free_confirm_chain_arrays(m);
    return;
  }

  uint64_t bits = m->confirmed_bits | my_bit;
  bool denied = m->denied;
  pony_actor_t* denier = m->denier;

  if(!denied)
  {
    bool blocked = ponyint_messageq_isempty(&actor->q)
      && (actor->live_asio_events == 0);

    if(blocked)
      blocked = (actor->gc.rc == m->appearances[my_idx]);

    if(!blocked)
    {
      denied = true;
      denier = actor;
    }
  }

  pony_actor_t* targets[64];
  size_t target_count = find_outgoing_members(actor, m->members,
    m->count, bits, targets);

  if(target_count > 0)
  {
    for(size_t i = 0; i < target_count; i++)
    {
      send_confirm_chain(ctx, targets[i], m->members, m->appearances,
        m->count, m->leader, bits, denied, denier);
    }
  }
  else
  {
    gc_t* gc = ponyint_actor_gc(actor);
    size_t index = HASHMAP_UNKNOWN;
    if(ponyint_actormap_getactor(&gc->foreign, m->leader, &index) != NULL)
    {
      send_confirm_chain(ctx, m->leader, m->members, m->appearances,
        m->count, m->leader, bits, denied, denier);
    }
    else
    {
      pony_actor_t* any_targets[64];
      size_t any_count = find_outgoing_members(actor, m->members,
        m->count, 0, any_targets);
      if(any_count > 0)
      {
        send_confirm_chain(ctx, any_targets[0], m->members, m->appearances,
          m->count, m->leader, bits, denied, denier);
      }
    }
  }

  free_confirm_chain_arrays(m);
}

void ponyint_distcd_handle_delegate(pony_ctx_t* ctx,
  pony_actor_t* actor, delegate_chain_msg_t* m)
{
  if(actor == m->denier)
  {
    distcd_t* distcd = actor->distcd;
    if(distcd == NULL)
    {
      actor->distcd = ponyint_distcd_create();
      distcd = actor->distcd;
    }

    if(distcd->delegated_cycles != NULL)
      free_cycle_list(distcd->delegated_cycles);

    distcd->delegated_cycles = m->cycles;
    distcd->conf_state = DISTCD_CONF_MEMBER_PENDING;
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    return;
  }

  size_t my_idx = find_member_index(actor, m->members, m->count);
  uint64_t visited = m->visited_bits;
  if(my_idx != SIZE_MAX)
    visited |= ((uint64_t)1 << my_idx);

  pony_actor_t* targets[64];
  size_t target_count = find_outgoing_members(actor, m->members,
    m->count, visited, targets);

  if(target_count > 0)
  {
    send_delegate_chain(ctx, targets[0], m->members, m->count,
      m->denier, visited, m->cycles);
  }
  else
  {
    pony_actor_t* any_targets[64];
    size_t any_count = find_outgoing_members(actor, m->members,
      m->count, 0, any_targets);
    if(any_count > 0)
    {
      send_delegate_chain(ctx, any_targets[0], m->members, m->count,
        m->denier, visited, m->cycles);
    }
  }

  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
  free_cycle_list(m->cycles);
}

void ponyint_distcd_handle_release(pony_ctx_t* ctx,
  pony_actor_t* actor, release_chain_msg_t* m)
{
  distcd_t* distcd = actor->distcd;
  if(distcd != NULL && distcd->released)
  {
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    ponyint_pool_free_size(m->count * sizeof(size_t), m->appearances);
    return;
  }

  if(distcd == NULL)
  {
    actor->distcd = ponyint_distcd_create();
    distcd = actor->distcd;
  }

  size_t my_idx = find_member_index(actor, m->members, m->count);
  bool blocked = (my_idx != SIZE_MAX);

  if(blocked)
  {
    blocked = ponyint_messageq_isempty(&actor->q)
      && (actor->live_asio_events == 0);
  }

  if(blocked)
    blocked = (actor->gc.rc == m->appearances[my_idx]);

  if(!blocked)
  {
    distcd->conf_state = DISTCD_CONF_NONE;
    ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
    ponyint_pool_free_size(m->count * sizeof(size_t), m->appearances);
    return;
  }

  distcd->released = true;
  free_cycle_list(distcd->known_cycles);
  distcd->known_cycles = NULL;
  distcd->cycles_generation++;

  if(distcd->candidate != NULL)
  {
    ponyint_pool_free_size(
      distcd->candidate->count * sizeof(pony_actor_t*),
      distcd->candidate->members);
    if(distcd->candidate->appearances != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(size_t),
        distcd->candidate->appearances);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
    distcd->candidate = NULL;
  }
  distcd->conf_state = DISTCD_CONF_NONE;

  pony_actor_t* targets[64];
  size_t target_count = find_outgoing_members(actor, m->members,
    m->count, 0, targets);

  for(size_t i = 0; i < target_count; i++)
  {
    send_release_chain(ctx, targets[i], m->members, m->appearances,
      m->count, m->leader);
  }

  ponyint_pool_free_size(m->count * sizeof(pony_actor_t*), m->members);
  ponyint_pool_free_size(m->count * sizeof(size_t), m->appearances);
}

bool ponyint_distcd_can_self_reap(distcd_t* distcd)
{
  if(distcd == NULL)
    return true;

  if(distcd->released)
    return false;

  if(distcd->known_cycles != NULL)
  {
    free_cycle_list(distcd->known_cycles);
    distcd->known_cycles = NULL;
    distcd->cycles_generation++;
  }

  if(distcd->candidate != NULL)
  {
    if(distcd->candidate->members != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(pony_actor_t*),
        distcd->candidate->members);
    if(distcd->candidate->appearances != NULL)
      ponyint_pool_free_size(
        distcd->candidate->count * sizeof(size_t),
        distcd->candidate->appearances);
    ponyint_pool_free_size(sizeof(candidate_record_t), distcd->candidate);
    distcd->candidate = NULL;
  }

  if(distcd->delegated_cycles != NULL)
  {
    free_cycle_list(distcd->delegated_cycles);
    distcd->delegated_cycles = NULL;
  }

  distcd->conf_state = DISTCD_CONF_NONE;

  return true;
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

  if(distcd->conf_state == DISTCD_CONF_MEMBER_PENDING)
  {
    distcd->conf_state = DISTCD_CONF_NONE;
    if(distcd->known_cycles == NULL && distcd->delegated_cycles == NULL)
      return;
  }

  if(distcd->delegated_cycles != NULL)
  {
    cycle_record_t* dc = distcd->delegated_cycles;
    while(dc != NULL)
    {
      cycle_record_t* next = dc->next;
      if(!cycle_already_known(distcd->known_cycles, dc))
      {
        dc->next = distcd->known_cycles;
        distcd->known_cycles = dc;
      }
      else
      {
        ponyint_pool_free_size(
          dc->count * sizeof(pony_actor_t*), dc->members);
        ponyint_pool_free_size(sizeof(cycle_record_t), dc);
      }
      dc = next;
    }
    distcd->delegated_cycles = NULL;
    distcd->cycles_generation++;
    invalidate_component_cache(distcd);
  }

  prune_unreachable_cycles(actor, distcd);

  if(distcd->conf_state != DISTCD_CONF_NONE || distcd->known_cycles == NULL)
    return;

  if(actor->live_asio_events > 0)
    return;

  if(!ponyint_messageq_isempty(&actor->q))
    return;

  pony_actor_t** comp_members;
  size_t comp_count;
  pony_actor_t* leader;

  get_component_and_leader(distcd, actor, &comp_members, &comp_count, &leader);

  if(comp_count == 0)
    return;

  pony_assert(comp_count <= 64);

  if(leader != actor)
  {
    ponyint_pool_free_size(comp_count * sizeof(pony_actor_t*), comp_members);
    return;
  }

  size_t* appearances = (size_t*)ponyint_pool_alloc_size(
    comp_count * sizeof(size_t));
  for(size_t i = 0; i < comp_count; i++)
  {
    appearances[i] = count_appearances_in_component(
      comp_members[i], distcd->known_cycles, comp_members, comp_count);
  }

  size_t leader_idx = find_member_index(actor, comp_members, comp_count);
  pony_assert(leader_idx != SIZE_MAX);
  if(actor->gc.rc != appearances[leader_idx])
  {
    ponyint_pool_free_size(comp_count * sizeof(pony_actor_t*), comp_members);
    ponyint_pool_free_size(comp_count * sizeof(size_t), appearances);
    return;
  }

  distcd->candidate = (candidate_record_t*)ponyint_pool_alloc_size(
    sizeof(candidate_record_t));
  distcd->candidate->members = comp_members;
  distcd->candidate->appearances = appearances;
  distcd->candidate->count = comp_count;
  distcd->candidate->leader = actor;
  distcd->candidate->confirmed_bits = (uint64_t)1 << leader_idx;
  distcd->candidate->denied = false;
  distcd->candidate->denier = NULL;

  distcd->conf_state = DISTCD_CONF_LEADER_WAITING;

  pony_actor_t* targets[64];
  size_t target_count = find_outgoing_members(actor, comp_members,
    comp_count, distcd->candidate->confirmed_bits, targets);

  for(size_t i = 0; i < target_count; i++)
  {
    send_confirm_chain(ctx, targets[i], comp_members, appearances,
      comp_count, actor, distcd->candidate->confirmed_bits, false, NULL);
  }
}

PONY_EXTERN_C_END
