#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/image.h>
#include <gc/trace.h>
#include <mem/heap.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>
#include <cstring>
#include <stdlib.h>
#if defined(__ELF__)
#  include <link.h>
#  include <type_traits>
#  include <unistd.h>
#endif

// The GC skips objects in memory that is read-only once the program is
// loaded, so that memory must not include anything a program can write.

namespace
{
const uint64_t constant_value = 7;

// A constant holding a pointer, which the loader must relocate.
const uint64_t* const constant_pointer = &constant_value;

uint64_t writable_value = 7;
uint64_t zeroed_value;
}

TEST(Image, ConstantsAreReadOnly)
{
  ponyint_image_init();

  ASSERT_TRUE(ponyint_image_constant(&constant_value));
  ASSERT_TRUE(ponyint_image_constant(&constant_pointer));
#if defined(__ELF__)
  // Protection is per page, so the read-only part ends on a page boundary.
  EXPECT_EQ(0u, ponyint_image_ro_end % (uintptr_t)sysconf(_SC_PAGESIZE));
#endif
}

TEST(Image, WritableMemoryIsNotReadOnly)
{
  ponyint_image_init();

  uint64_t local = 7;
  void* heap = malloc(64);

  ASSERT_FALSE(ponyint_image_constant(&writable_value));
  ASSERT_FALSE(ponyint_image_constant(&zeroed_value));
  ASSERT_FALSE(ponyint_image_constant(&local));
  ASSERT_FALSE(ponyint_image_constant(heap));

  free(heap);
}

#if defined(__ELF__)
// Protection is per page, so the read-only part ends at the start of the page
// holding a writable segment's start, even when that start is inside a page.
// After a RELRO region it ends where the region ends: only relocation writes
// the rest of that page's RELRO bytes. It also ends at the first gap between
// loaded segments.
TEST(Image, ReadOnlyPartEndsWhereItsPageIsWritable)
{
  const uintptr_t page = 0x1000;
  // The program header type, named the same way on every ELF platform.
  using phdr_t = std::remove_const_t<
    std::remove_pointer_t<decltype(dl_phdr_info::dlpi_phdr)>>;

  // Code, then a writable segment starting inside a page.
  phdr_t plain[2] = {};
  plain[0].p_type = PT_LOAD;
  plain[0].p_flags = PF_R | PF_X;
  plain[0].p_vaddr = 0;
  plain[0].p_memsz = 0x1200;
  plain[1].p_type = PT_LOAD;
  plain[1].p_flags = PF_R | PF_W;
  plain[1].p_vaddr = 0x13d0;
  plain[1].p_memsz = 0x200;

  // Code, then a writable segment that begins with a RELRO region ending
  // inside a page.
  phdr_t relro[3] = {};
  relro[0] = plain[0];
  relro[1].p_type = PT_LOAD;
  relro[1].p_flags = PF_R | PF_W;
  relro[1].p_vaddr = 0x2000;
  relro[1].p_memsz = 0x1800;
  relro[2].p_type = PT_GNU_RELRO;
  relro[2].p_vaddr = 0x2000;
  relro[2].p_memsz = 0xa00;

  struct dl_phdr_info info = {};
  info.dlpi_addr = 0x400000;
  uintptr_t start = 0;
  uintptr_t end = 0;

  info.dlpi_phdr = plain;
  info.dlpi_phnum = 2;
  ASSERT_TRUE(ponyint_image_elf_bounds(&info, 0x400010, page, &start, &end));
  EXPECT_EQ(0x400000u, start);
  EXPECT_EQ(0x401000u, end);

  info.dlpi_phdr = relro;
  info.dlpi_phnum = 3;
  ASSERT_TRUE(ponyint_image_elf_bounds(&info, 0x400010, page, &start, &end));
  EXPECT_EQ(0x402a00u, end);

  // Code, then read-only data more than a page later, as a linker aligning
  // segments to a larger page than the system's leaves it: nothing is mapped
  // in between, so the read-only part ends before the gap.
  phdr_t gap[3] = {};
  gap[0] = plain[0];
  gap[1].p_type = PT_LOAD;
  gap[1].p_flags = PF_R;
  gap[1].p_vaddr = 0x10000;
  gap[1].p_memsz = 0x800;
  gap[2].p_type = PT_LOAD;
  gap[2].p_flags = PF_R | PF_W;
  gap[2].p_vaddr = 0x20000;
  gap[2].p_memsz = 0x800;

  info.dlpi_phdr = gap;
  info.dlpi_phnum = 3;
  ASSERT_TRUE(ponyint_image_elf_bounds(&info, 0x400010, page, &start, &end));
  EXPECT_EQ(0x402000u, end);

  // An address outside every loaded segment isn't this object.
  EXPECT_FALSE(ponyint_image_elf_bounds(&info, 0x900000, page, &start, &end));
}
#endif

namespace
{
size_t constant_traced;

void trace_counted(pony_ctx_t* ctx, void* p)
{
  (void)ctx;
  (void)p;
  constant_traced++;
}

struct Object
{
  const _pony_type_t* type;
  uint64_t x;
};

const _pony_type_t counted_type = {0, sizeof(Object), 0, 0, nullptr, nullptr,
  nullptr, trace_counted, nullptr, nullptr, 0, true, 0, nullptr, nullptr,
  nullptr};
const _pony_type_t counted_routed_type = {0, sizeof(Object), 0, 0, nullptr,
  nullptr, nullptr, trace_counted, nullptr, nullptr, 0, true,
  PONY_EMBED_ROUTED, nullptr, nullptr, nullptr};

const Object constant_object = {&counted_type, 1};
const Object constant_routed_object = {&counted_routed_type, 1};
Object writable_object = {&counted_type, 1};
Object writable_routed_object = {&counted_routed_type, 1};
}

// A send or a mark doesn't trace an object in read-only image memory, on the
// plain or the routed path; a writable object outside every heap is traced.
// A raw trace (pony_trace) has no type. When its memory has no heap chunk, as
// for memory from C, a send, receive or mark leaves it alone.
TEST(Image, RawTraceOfMemoryOutsideTheHeap)
{
  ponyint_register_thread();
  ponyint_image_init();
  pony_ctx_t* ctx = pony_ctx();
  _pony_type_t actor_type{};
  pony_actor_t actor{};
  actor.type = &actor_type;
  ponyint_heap_init(&actor.heap);
  ponyint_gc_done(&actor.gc);
  ctx->current = &actor;
  void* from_c = malloc(64);

  for(const char* mode : {"mark", "send", "receive"})
  {
    SCOPED_TRACE(mode);

    if(strcmp(mode, "send") == 0)
      pony_gc_send(ctx, &actor);
    else if(strcmp(mode, "receive") == 0)
      pony_gc_recv(ctx);
    else
      ponyint_gc_mark(ctx);

    pony_trace(ctx, from_c);
    pony_trace(ctx, nullptr);
    ponyint_gc_handlestack(ctx);

    ponyint_gc_end_visits(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ponyint_gc_done(&actor.gc);
    ctx->msg_target = NULL;
  }

  free(from_c);
  ctx->current = NULL;
  ponyint_gc_destroy(&actor.gc);
  ponyint_heap_destroy(&actor.heap);
  ponyint_unregister_thread();
}

TEST(Image, GcDoesNotTraceConstants)
{
  ponyint_register_thread();
  ponyint_image_init();
  pony_ctx_t* ctx = pony_ctx();
  _pony_type_t actor_type{};
  pony_actor_t actor{};
  actor.type = &actor_type;
  ponyint_heap_init(&actor.heap);
  ponyint_gc_done(&actor.gc);
  ctx->current = &actor;

  for(const char* mode : {"mark", "send", "receive"})
  {
    SCOPED_TRACE(mode);
    constant_traced = 0;

    if(strcmp(mode, "send") == 0)
      pony_gc_send(ctx, &actor);
    else if(strcmp(mode, "receive") == 0)
      pony_gc_recv(ctx);
    else
      ponyint_gc_mark(ctx);

    pony_traceknown(ctx, const_cast<Object*>(&constant_object),
      &counted_type, PONY_TRACE_MUTABLE);
    pony_traceknown_routed(ctx, const_cast<Object*>(&constant_routed_object),
      &counted_routed_type, PONY_TRACE_MUTABLE);
    ponyint_gc_handlestack(ctx);
    EXPECT_EQ(0u, constant_traced);

    pony_traceknown(ctx, &writable_object, &counted_type, PONY_TRACE_MUTABLE);
    pony_traceknown_routed(ctx, &writable_routed_object, &counted_routed_type,
      PONY_TRACE_MUTABLE);
    ponyint_gc_handlestack(ctx);
    EXPECT_EQ(2u, constant_traced);

    ponyint_gc_end_visits(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ponyint_gc_done(&actor.gc);
    ctx->msg_target = NULL;
  }

  ctx->current = NULL;
  ponyint_gc_destroy(&actor.gc);
  ponyint_heap_destroy(&actor.heap);
  ponyint_unregister_thread();
}
