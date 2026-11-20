#include "gc/root_audit.h"
#include "actor/actor.h"
#include "sched/scheduler.h"
#include "mem/pagemap.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool probe_collect(pony_ctx_t*, pony_actor_t*, void*);

enum { GARBAGE_COUNT = 1024 };
enum phase { WAITING, PUBLISHED, POLLED, RETURNING, RESUMED };

static _Thread_local struct
{
  enum phase phase;
  pony_actor_t* actor;
  void* root;
  uintptr_t identity;
  uintptr_t garbage[GARBAGE_COUNT];
  size_t count, before, threshold, at_poll, after;
  bool enabled, due, admitted, completed, reused;
} frame;

static void require(bool ok, const char* why)
{
  if(!ok)
  {
    fprintf(stderr, "caller experiment failed: %s\n", why);
    exit(2);
  }
}

static void check_cell(void* cell)
{
  pony_actor_t* owner = NULL;
  chunk_t* chunk = ponyint_pagemap_get(cell, &owner);
  require(chunk != NULL && owner == frame.actor, "cell on measured actor heap");
}

void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* actor,
  const char* method, uint32_t line, uint32_t column, const char* reason,
  size_t count, void* const* roots, const char* const* modes)
{
  if(strncmp(method, "Probe_tag_measure_", strlen("Probe_tag_measure_")) != 0)
    return;
  if(frame.phase == RETURNING)
  {
    require(ctx == pony_ctx() && ctx->current == frame.actor &&
      actor == frame.actor && frame.root == NULL && line == SITE_LINE + 1 &&
      reason != NULL && strcmp(reason, "unsupported-prefix") == 0,
      "caller resumed with publication cleared");
    frame.phase = RESUMED;
    return;
  }
  if(line != SITE_LINE || column != SITE_COLUMN)
    return;
  require(frame.phase == WAITING && reason == NULL && ctx == pony_ctx() &&
    ctx->current == actor, "ready caller site on current actor");
  require(count == 2 && roots[0] == actor && !strcmp(modes[0], "receiver") &&
    !strcmp(modes[1], "ref"), "receiver and one mutable local");
  const char* mode = getenv("CALLER_MODE");
  require(mode && (!strcmp(mode, "on") || !strcmp(mode, "off") ||
    !strcmp(mode, "unarmed")), "recognized mode");
  frame.actor = actor;
  check_cell(roots[1]);
  frame.identity = (uintptr_t)roots[1];
  frame.root = strcmp(mode, "unarmed") ? roots[1] : NULL;
  frame.enabled = strcmp(mode, "off") != 0;
  frame.before = actor->heap.used;
  frame.threshold = actor->heap.next_gc;
  require(frame.before <= frame.threshold, "below threshold before helper");
  frame.phase = PUBLISHED;
}

void caller_garbage(void* cell)
{
  require(frame.phase == PUBLISHED && pony_ctx()->current == frame.actor &&
    frame.count < GARBAGE_COUNT, "garbage allocated inside helper");
  check_cell(cell);
  require((uintptr_t)cell != frame.identity, "caller and garbage distinct");
  for(size_t i = 0; i < frame.count; i++)
    require(frame.garbage[i] != (uintptr_t)cell, "distinct garbage allocations");
  frame.garbage[frame.count++] = (uintptr_t)cell;
}

void caller_poll(void)
{
  pony_ctx_t* ctx = pony_ctx();
  pony_actor_t* actor = ctx->current;
  require(frame.phase == PUBLISHED && actor == frame.actor &&
    frame.count == GARBAGE_COUNT, "poll after garbage helper returned");
  require(!ctx->stack && !ctx->msg_target && !ponyint_actormap_size(&ctx->acquire)
    && !ponyint_objectmap_size(&actor->gc.local) &&
    !ponyint_actormap_size(&actor->gc.foreign) &&
    !ponyint_heap_has_finalisers(&actor->heap), "isolated actor and idle tracer");
  require(actor->heap.next_gc == frame.threshold, "stable threshold before poll");
  frame.at_poll = actor->heap.used;
  frame.due = frame.at_poll > frame.threshold;
  frame.admitted = frame.root != NULL;
  if(frame.due && frame.admitted && frame.enabled)
    frame.completed = probe_collect(ctx, actor, frame.root);
  frame.root = NULL;
  frame.after = actor->heap.used;
  frame.phase = POLLED;
}

void caller_leave_phase(void)
{
  require(frame.phase == POLLED && pony_ctx()->current == frame.actor,
    "phase continued after poll");
  frame.phase = RETURNING;
}

void caller_addresses(void* kept, void* fresh)
{
  require(frame.phase == RESUMED && pony_ctx()->current == frame.actor,
    "caller continued after phase");
  require((uintptr_t)kept == frame.identity && kept != fresh, "caller identity");
  check_cell(kept);
  check_cell(fresh);
  for(size_t i = 0; i < frame.count; i++)
    frame.reused |= frame.garbage[i] == (uintptr_t)fresh;
}

void caller_result(uint64_t before, uint64_t after, uint64_t fresh_value)
{
  require(before == 137 && after == 142 && fresh_value == 211, "caller payloads");
  printf("before=%zu threshold=%zu poll=%zu after=%zu allocations=%zu "
    "due=%d admitted=%d completed=%d reused=%d kept=%" PRIu64
    " mutated=%" PRIu64 " fresh=%" PRIu64 "\n", frame.before, frame.threshold,
    frame.at_poll, frame.after, frame.count, frame.due, frame.admitted,
    frame.completed, frame.reused, before, after, fresh_value);
  pony_exitcode(0);
}
