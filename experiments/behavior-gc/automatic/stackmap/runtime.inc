#include <unwind.h>

extern const unsigned char pony_maps_begin[], pony_maps_end[];

static uint16_t map_u16(const unsigned char* p)
{
  uint16_t n;
  memcpy(&n, p, sizeof(n));
  return n;
}

static uint32_t map_u32(const unsigned char* p)
{
  uint32_t n;
  memcpy(&n, p, sizeof(n));
  return n;
}

static uint64_t map_u64(const unsigned char* p)
{
  uint64_t n;
  memcpy(&n, p, sizeof(n));
  return n;
}

typedef struct map_location_t
{
  uint64_t role;
  uint16_t reg;
  int32_t offset;
  uint64_t frame_size;
} map_location_t;

static bool map_lookup(uintptr_t pc, map_location_t* result)
{
  const unsigned char* begin = pony_maps_begin;
  const unsigned char* end = pony_maps_end;
  size_t size = (uintptr_t)end - (uintptr_t)begin;
  if(size < 16 || map_u32(begin) != 3 || map_u32(begin + 8) != 0)
    return false;
  uint32_t functions = map_u32(begin + 4);
  uint32_t remaining = map_u32(begin + 12);
  if(functions > (size - 16) / 24)
    return false;
  const unsigned char* f = begin + 16;
  const unsigned char* r = f + functions * (size_t)24;
  bool found = false;
  for(uint32_t i = 0; i < functions; i++, f += 24)
  {
    uint64_t count = map_u64(f + 16);
    if(count > remaining)
      return false;
    remaining -= (uint32_t)count;
    for(uint64_t j = 0; j < count; j++)
    {
      if((size_t)(end - r) < 16)
        return false;
      unsigned n = map_u16(r + 14);
      uint64_t role = map_u64(r);
      size_t bytes = n == 0 ? 24 : 40;
      if(role < 1 || role > 4 || n != (role == 4 ? 0U : 1U) ||
        (size_t)(end - r) < bytes || map_u16(r + 12) != 0 ||
        map_u32(r + bytes - 8) != 0)
        return false;
      if(map_u64(f) <= pc && pc - map_u64(f) == map_u32(r + 8) && role != 1)
      {
        if(found || (n && (r[16] != 2 || map_u16(r + 18) != sizeof(void*) ||
          (map_u16(r + 20) != 6 && map_u16(r + 20) != 7))))
          return false;
        found = true;
        result->role = role;
        result->frame_size = map_u64(f + 8);
        result->reg = n ? map_u16(r + 20) : 0;
        result->offset = n ? (int32_t)map_u32(r + 24) : 0;
      }
      r += bytes;
    }
  }
  return found && remaining == 0 && r == end;
}

typedef struct map_walk_t
{
  unsigned depth;
  uintptr_t slots[2];
} map_walk_t;

static _Unwind_Reason_Code map_visit(struct _Unwind_Context* context, void* arg)
{
  map_walk_t* walk = (map_walk_t*)arg;
  if(walk->depth == 0)
  {
    walk->depth++;
    return _URC_NO_REASON;
  }
  int before = 0;
  uintptr_t pc = _Unwind_GetIPInfo(context, &before);
  const unsigned roles[] = {4, 2, 3};
  map_location_t record;
  if(before || !map_lookup(pc, &record) || record.role != roles[walk->depth - 1])
    return _URC_END_OF_STACK;
  if(walk->depth >= 2)
  {
    uintptr_t fp = _Unwind_GetGR(context, 6);
    uintptr_t sp = _Unwind_GetCFA(context);
    if(fp < sp || fp - sp < sizeof(void*))
      return _URC_END_OF_STACK;
    uintptr_t slot;
    if(record.reg == 6)
    {
      uint64_t distance = (uint64_t)-(int64_t)record.offset;
      if(record.frame_size == UINT64_MAX || record.offset >= 0 ||
        distance < sizeof(void*) || distance > record.frame_size ||
        distance > fp - sp)
        return _URC_END_OF_STACK;
      slot = fp - distance;
    }
    else
    {
      if(record.offset < 0 ||
        (uintptr_t)record.offset > fp - sp - sizeof(void*) ||
        (uintptr_t)record.offset + sizeof(void*) > record.frame_size)
        return _URC_END_OF_STACK;
      slot = sp + (uintptr_t)record.offset;
    }
    if(slot % sizeof(void*) != 0)
      return _URC_END_OF_STACK;
    walk->slots[walk->depth - 2] = slot;
  }
  walk->depth++;
  return walk->depth == 4 ? _URC_END_OF_STACK : _URC_NO_REASON;
}

__attribute__((noinline))
void ponyint_mapped_collect(pony_actor_t* actor)
{
  map_walk_t walk = {0};
  _Unwind_Backtrace(map_visit, &walk);
  if(walk.depth != 4)
    return;
  void* inner;
  void* outer;
  memcpy(&inner, (void*)walk.slots[0], sizeof(inner));
  memcpy(&outer, (void*)walk.slots[1], sizeof(outer));
  ponyint_threshold_collect(actor, outer, inner);
}
