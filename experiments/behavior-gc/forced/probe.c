#include "gc/root_audit.h"
#include "actor/actor.h"
#include "sched/scheduler.h"
#include "mem/pagemap.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool probe_collect(pony_ctx_t*, pony_actor_t*, void*);

static uint64_t (*allocate_temporary)(void);
static uint64_t (*read_cell)(void*);
static uintptr_t temporary_id, kept_id;
static bool observing, visited, collect_now, completed;
static chunk_t* kept_chunk;

static void require(bool valid, const char* message)
{
  if(!valid)
  {
    fprintf(stderr, "experiment failed: %s\n", message);
    exit(2);
  }
}

static chunk_t* owned(void* cell)
{
  pony_actor_t* owner = NULL;
  chunk_t* chunk = ponyint_pagemap_get(cell, &owner);
  require(chunk != NULL && owner == pony_ctx()->current, "actor heap ownership");
  return chunk;
}

void probe_setup(uint64_t (*allocate)(void), uint64_t (*read)(void*))
{
  const char* mode = getenv("PROBE_COLLECTION");
  require(mode && (!strcmp(mode, "off") || !strcmp(mode, "on")), "trial mode");
  collect_now = !strcmp(mode, "on");
  allocate_temporary = allocate;
  read_cell = read;
}

void probe_observe(void* cell)
{
  require(observing && !temporary_id, "temporary observed once inside helper");
  require(owned(cell) == kept_chunk, "same allocation size and chunk");
  require(read_cell(cell) == 83, "temporary payload");
  temporary_id = (uintptr_t)cell;
  require(temporary_id != kept_id, "distinct live allocations");
}

void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* receiver,
  const char* method, uint32_t line, uint32_t column, const char* reason,
  size_t count, void* const* roots, const char* const* modes)
{
  if(strncmp(method, "Probe_tag_measure_", strlen("Probe_tag_measure_")) ||
    line != SITE_LINE || column != SITE_COLUMN)
    return;
  require(!visited && allocate_temporary && read_cell, "single configured site");
  require(!reason && ctx == pony_ctx() && ctx->current == receiver, "ready frame");
  require(count == 2 && roots[0] == receiver && !strcmp(modes[0], "receiver")
    && !strcmp(modes[1], "ref"), "published receiver and one mutable local");
  require(!strcmp(receiver->type->name, "Probe"), "expected receiver");
  visited = true;
  kept_chunk = owned(roots[1]);
  kept_id = (uintptr_t)roots[1];
  require(read_cell(roots[1]) == 137, "published local payload before GC");
  observing = true;
  require(allocate_temporary() == 83 && temporary_id, "helper returned");
  observing = false;
  require(!ctx->stack && !ctx->msg_target && !ponyint_actormap_size(&ctx->acquire)
    && !ponyint_heap_has_finalisers(&receiver->heap), "idle trace and no finalisers");
  require(!ponyint_objectmap_size(&receiver->gc.local) &&
    !ponyint_actormap_size(&receiver->gc.foreign), "isolated actor heap");
  if(collect_now)
  {
    pony_triggergc(ctx);
    completed = probe_collect(ctx, receiver, roots[1]);
    require(completed, "completed collection");
  }
  printf("site=%s %u:%u frame=receiver,ref slot_bytes=%zu\n",
    method, line, column, ponyint_heap_size(kept_chunk));
}

void probe_check(void* kept, void* replacement, uint64_t expected)
{
  require(visited && !observing && temporary_id && collect_now == completed,
    "selected boundary completed");
  require((uintptr_t)kept == kept_id && kept != replacement, "live identity");
  require(owned(kept) == kept_chunk && owned(replacement) == kept_chunk,
    "resumed allocations belong to the same chunk");
  bool reused = (uintptr_t)replacement == temporary_id;
  require(reused == collect_now, "temporary slot reuse agrees with trial");
  uint64_t original = read_cell(kept);
  uint64_t fresh = read_cell(replacement);
  require(expected == 137, "expected behavior argument");
  require(original == expected, "retained field payload");
  require(fresh == 211, "replacement field payload");
  printf("completed=%d reused=%d kept=%" PRIu64 " replacement=%" PRIu64 "\n",
    completed, reused, original, fresh);
  pony_exitcode(0);
}
