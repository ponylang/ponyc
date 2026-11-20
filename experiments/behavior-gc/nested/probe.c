#include "gc/root_audit.h"
#include "actor/actor.h"
#include "sched/scheduler.h"
#include "mem/pagemap.h"
#include "inner_site.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool probe_collect_roots(pony_ctx_t*, pony_actor_t*, size_t, void* const*,
  void (*check_marks)(void));

enum { OUTER, INNER, LEVELS, TEMPORARIES = 1024 };

static _Thread_local struct
{
  pony_actor_t* actor;
  size_t depth, baseline[LEVELS], threshold, poll, after, allocations;
  void* roots[LEVELS];
  uintptr_t ids[LEVELS], garbage[TEMPORARIES];
  uint64_t old[LEVELS], updated[LEVELS];
  unsigned published, marked;
  bool enabled, skip[LEVELS], collected, polled, left, checked_inner, reused;
} trial;

static void require(bool ok, const char* message)
{
  if(!ok)
  {
    fprintf(stderr, "nested caller experiment failed: %s\n", message);
    exit(2);
  }
}

static chunk_t* owned(void* cell)
{
  pony_actor_t* owner = NULL;
  chunk_t* chunk = ponyint_pagemap_get(cell, &owner);
  require(chunk != NULL && owner == trial.actor, "cell on measured heap");
  return chunk;
}

static void configure(pony_actor_t* actor)
{
  const char* mode = getenv("NESTED_MODE");
  require(mode != NULL, "mode required");
  trial.enabled = strcmp(mode, "off") != 0;
  trial.skip[OUTER] = !strcmp(mode, "omit-outer");
  trial.skip[INNER] = !strcmp(mode, "omit-inner");
  require(!trial.enabled || trial.skip[OUTER] || trial.skip[INNER] ||
    !strcmp(mode, "on"), "unknown mode");
  trial.actor = actor;
  trial.threshold = actor->heap.next_gc;
}

static void check_slots(void)
{
  for(size_t i = 0; i < LEVELS; i++)
  {
    uintptr_t expected = (i < trial.depth && !trial.skip[i]) ? trial.ids[i] : 0;
    require((uintptr_t)trial.roots[i] == expected, "published slot identity");
  }
}

void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* actor,
  const char* method, uint32_t line, uint32_t column, const char* reason,
  size_t count, void* const* roots, const char* const* modes)
{
  size_t level;
  if(!strcmp(method, "Probe_tag_measure_pWo"))
    level = OUTER;
  else if(!strcmp(method, "Probe_ref_middle_o"))
    level = INNER;
  else
    return;

  uint32_t site = level == OUTER ? SITE_LINE : INNER_LINE;
  uint32_t pos = level == OUTER ? SITE_COLUMN : INNER_COLUMN;
  require(ctx == pony_ctx() && actor == ctx->current, "active actor");
  if(line == site && column == pos)
  {
    require(trial.depth == level && reason == NULL && count == 2 &&
      roots[0] == actor && !strcmp(modes[0], "receiver") &&
      !strcmp(modes[1], "ref"), "one mutable leaf in the expected caller");
    if(level == OUTER)
      configure(actor);
    require(actor == trial.actor && actor->heap.next_gc == trial.threshold &&
      actor->heap.used <= trial.threshold, "below ordinary threshold at call");
    owned(roots[1]);
    trial.ids[level] = (uintptr_t)roots[1];
    require(level == OUTER || trial.ids[INNER] != trial.ids[OUTER],
      "distinct caller leaves");
    trial.baseline[level] = actor->heap.used;
    trial.roots[level] = trial.skip[level] ? NULL : roots[1];
    trial.depth++;
    check_slots();
  } else if(line == site + 1) {
    require(actor == trial.actor && trial.depth == level + 1 && trial.left &&
      reason != NULL && !strcmp(reason, "unsupported-prefix") &&
      (level == INNER || trial.checked_inner), "return to the expected caller");
    check_slots();
    trial.roots[--trial.depth] = NULL;
    check_slots();
  }
}

void nested_garbage(void* cell)
{
  require(trial.actor == pony_ctx()->current && trial.depth == LEVELS &&
    trial.allocations < TEMPORARIES && !trial.polled, "garbage helper extent");
  owned(cell);
  uintptr_t id = (uintptr_t)cell;
  require(id != trial.ids[OUTER] && id != trial.ids[INNER], "distinct live leaf");
  for(size_t i = 0; i < trial.allocations; i++)
    require(trial.garbage[i] != id, "distinct temporary");
  trial.garbage[trial.allocations++] = id;
}

static void check_marks(void)
{
  for(size_t i = 0; i < LEVELS; i++)
  {
    void* cell = (void*)trial.ids[i];
    require(ponyint_heap_mark(owned(cell), cell), "leaf already marked before sweep");
    trial.marked |= 1U << i;
  }
}

void nested_poll(void)
{
  pony_ctx_t* ctx = pony_ctx();
  pony_actor_t* actor = ctx->current;
  require(trial.depth == LEVELS && actor == trial.actor && !trial.polled &&
    trial.allocations == TEMPORARIES, "one poll after temporary allocations");
  require(!ctx->stack && !ctx->msg_target && !ponyint_actormap_size(&ctx->acquire)
    && !ponyint_objectmap_size(&actor->gc.local) &&
    !ponyint_actormap_size(&actor->gc.foreign) &&
    !ponyint_heap_has_finalisers(&actor->heap), "isolated heap and idle tracer");
  check_slots();
  for(size_t i = 0; i < trial.depth; i++)
    if(trial.roots[i] != NULL)
      trial.published |= 1U << i;
  require(actor->heap.next_gc == trial.threshold, "stable GC threshold");
  trial.poll = actor->heap.used;
  if(trial.enabled && trial.poll > trial.threshold && trial.published == 3)
    trial.collected = probe_collect_roots(ctx, actor, trial.depth, trial.roots,
      check_marks);
  trial.after = actor->heap.used;
  require(!trial.collected ||
    (trial.marked == 3 && trial.after == trial.baseline[INNER]),
    "both caller cells survived collection");
  trial.polled = true;
}

void nested_leave_phase(void)
{
  require(trial.actor == pony_ctx()->current && trial.polled && !trial.left &&
    trial.depth == LEVELS, "phase resumed after poll");
  trial.left = true;
}

void nested_fresh(void* cell)
{
  require(trial.actor == pony_ctx()->current && trial.depth == 1 && trial.left,
    "middle caller resumed");
  owned(cell);
  uintptr_t id = (uintptr_t)cell;
  require(id != trial.ids[INNER] && id != trial.ids[OUTER], "fresh identity");
  for(size_t i = 0; i < trial.allocations; i++)
    trial.reused |= id == trial.garbage[i];
}

void nested_value(uint8_t level, void* cell, uint64_t old, uint64_t value)
{
  require(level < LEVELS && trial.actor == pony_ctx()->current &&
    trial.depth == level && (uintptr_t)cell == trial.ids[level],
    "resumed caller has its own cell");
  owned(cell);
  trial.old[level] = old;
  trial.updated[level] = value;
  if(level == INNER)
  {
    trial.checked_inner = true;
    return;
  }
  require(trial.checked_inner, "inner caller returned first");
  printf("outer=%zu inner=%zu threshold=%zu poll=%zu after=%zu count=%zu "
    "published=%u marked=%u completed=%d reused=%d "
    "outer_old=%" PRIu64 " outer_new=%" PRIu64
    " inner_old=%" PRIu64 " inner_new=%" PRIu64 "\n",
    trial.baseline[OUTER], trial.baseline[INNER], trial.threshold, trial.poll,
    trial.after, trial.allocations, trial.published, trial.marked,
    trial.collected, trial.reused, trial.old[OUTER], trial.updated[OUTER],
    trial.old[INNER], trial.updated[INNER]);
  pony_exitcode(0);
}
