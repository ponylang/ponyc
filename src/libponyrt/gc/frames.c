#include "frames.h"
#include "map_format.h"
#include "../mem/pool.h"
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_IS_LINUX) && defined(__x86_64__) && !defined(__ILP32__)
#  define PONY_NATIVE_GC_MAPS
#  include <unwind.h>
#endif

PONY_EXTERN_C_BEGIN

static gc_frame_t* trace_records(pony_ctx_t* ctx, gc_frame_t* frame)
{
  while(frame != NULL && ((uintptr_t)frame & PONY_GC_RETURN_SLOT_TAG) == 0)
  {
    frame->trace(ctx, frame);
    frame = frame->parent;
  }
  return frame;
}

#ifdef PONY_NATIVE_GC_MAPS
extern const char __start_pony_gc_sites[]
  __attribute__((weak, visibility("hidden")));
extern const char __stop_pony_gc_sites[]
  __attribute__((weak, visibility("hidden")));

static const gc_map_site_t** index_sites;
static size_t index_count;

static uintptr_t return_pc(const gc_map_site_t* site)
{
  return (uintptr_t)&site->function + (uintptr_t)site->function +
    site->return_offset;
}

static int by_return_pc(const void* a, const void* b)
{
  uintptr_t left = return_pc(*(const gc_map_site_t* const*)a);
  uintptr_t right = return_pc(*(const gc_map_site_t* const*)b);
  return (left > right) - (left < right);
}

void ponyint_gc_maps_init(void)
{
  uintptr_t start = (uintptr_t)__start_pony_gc_sites;
  uintptr_t end = (uintptr_t)__stop_pony_gc_sites;
  size_t count = 0;
  for(uintptr_t p = start; p < end;)
  {
    if(end - p < sizeof(gc_map_site_t))
      return;
    const gc_map_site_t* site = (const gc_map_site_t*)p;
    if(site->location_count < 2 || site->location_count > PONY_GC_MAP_ROOTS + 1)
      return;
    size_t bytes = sizeof(*site) + site->location_count *
      sizeof(gc_map_location_t);
    if(bytes > end - p)
      return;
    p += bytes;
    count++;
  }
  if(count == 0)
    return;

  index_sites = (const gc_map_site_t**)ponyint_pool_alloc_size(
    count * sizeof(*index_sites));
  for(uintptr_t p = start; p < end;)
  {
    const gc_map_site_t* site = (const gc_map_site_t*)p;
    index_sites[index_count++] = site;
    p += sizeof(*site) + site->location_count * sizeof(gc_map_location_t);
  }
  qsort(index_sites, index_count, sizeof(*index_sites), by_return_pc);
}

void ponyint_gc_maps_destroy(void)
{
  if(index_sites != NULL)
    ponyint_pool_free_size(index_count * sizeof(*index_sites), index_sites);
  index_sites = NULL;
  index_count = 0;
}

static const gc_map_site_t* lookup(uintptr_t pc)
{
  size_t low = 0;
  size_t high = index_count;
  while(low < high)
  {
    size_t mid = low + (high - low) / 2;
    if(return_pc(index_sites[mid]) < pc)
      low = mid + 1;
    else
      high = mid;
  }
  if(low == index_count || return_pc(index_sites[low]) != pc ||
    (low + 1 < index_count && return_pc(index_sites[low + 1]) == pc))
    return NULL;
  return index_sites[low];
}

typedef struct frame_walk_t
{
  pony_ctx_t* ctx;
  gc_frame_t* token;
  const gc_map_site_t* site;
  uintptr_t sp;
  uintptr_t registers[16];
} frame_walk_t;

static bool preserved_register(uint16_t reg)
{
  return reg == 3 || reg == 6 || (reg >= 12 && reg <= 15);
}

static bool read_location(frame_walk_t* walk,
  const gc_map_location_t* loc, uintptr_t end,
  uintptr_t* value)
{
  switch(loc->kind)
  {
    case GC_MAP_REGISTER:
      if(loc->size != sizeof(uintptr_t) || !preserved_register(loc->reg))
        return false;
      *value = walk->registers[loc->reg];
      return true;
    case GC_MAP_DIRECT:
    case GC_MAP_INDIRECT:
    {
      if(loc->size != sizeof(uintptr_t) || (loc->reg != 6 && loc->reg != 7))
        return false;
      uintptr_t base = loc->reg == 6 ? walk->registers[6] : walk->sp;
      uintptr_t address = base + (uintptr_t)loc->offset;
      if(address < walk->sp || address >= end)
        return false;
      if(loc->kind == GC_MAP_DIRECT)
        *value = address;
      else if(end - address >= sizeof(*value))
        memcpy(value, (const void*)address, sizeof(*value));
      else
        return false;
      return true;
    }
    case GC_MAP_CONSTANT:
      *value = (uintptr_t)(intptr_t)loc->offset;
      return true;
    default:
      return false;
  }
}

static bool trace_site(frame_walk_t* walk, uintptr_t slot)
{
  const gc_map_site_t* site = walk->site;
  uintptr_t record[PONY_GC_MAP_ROOTS + 2];
  const gc_map_location_t* loc = (const gc_map_location_t*)(site + 1);
  for(uint32_t i = 0; i < site->location_count; i++)
    if(!read_location(walk, &loc[i], slot,
      &record[i == 0 ? 0 : i + 1]))
      return false;
  record[1] = (uintptr_t)&site->trace + (uintptr_t)site->trace;
  ((pony_trace_fn)record[1])(walk->ctx, record);
  walk->token = trace_records(walk->ctx, (gc_frame_t*)record[0]);
  return true;
}

static _Unwind_Reason_Code visit(struct _Unwind_Context* unwind, void* arg)
{
  frame_walk_t* walk = (frame_walk_t*)arg;
  uintptr_t sp = _Unwind_GetCFA(unwind);
  uintptr_t slot = (uintptr_t)walk->token - PONY_GC_RETURN_SLOT_TAG;
  if(sp > slot)
  {
    // The older frame's stack pointer is just above the return address.
    if(sp - slot != sizeof(void*) || walk->site == NULL ||
      !trace_site(walk, slot) || walk->token == NULL)
      return _URC_END_OF_STACK;
    if(sp > (uintptr_t)walk->token - PONY_GC_RETURN_SLOT_TAG)
      return _URC_END_OF_STACK;
  }

  walk->sp = sp;
  walk->site = lookup(_Unwind_GetIP(unwind));
  if(walk->site != NULL)
  {
    const gc_map_location_t* loc = (const gc_map_location_t*)(walk->site + 1);
    for(uint32_t i = 0; i < walk->site->location_count; i++)
      if(preserved_register(loc[i].reg) && loc[i].kind <= GC_MAP_INDIRECT)
        walk->registers[loc[i].reg] = _Unwind_GetGR(unwind, loc[i].reg);
  }
  return _URC_NO_REASON;
}
#else
void ponyint_gc_maps_init(void) {}
void ponyint_gc_maps_destroy(void) {}
#endif

bool ponyint_gc_trace_frames(pony_ctx_t* ctx, gc_frame_t* frame)
{
  frame = trace_records(ctx, frame);
#ifdef PONY_NATIVE_GC_MAPS
  if(frame != NULL && index_count != 0)
  {
    frame_walk_t walk = {ctx, frame, NULL, 0, {0}};
    _Unwind_Backtrace(visit, &walk);
    frame = walk.token;
  }
#endif
  return frame == NULL;
}

PONY_EXTERN_C_END
