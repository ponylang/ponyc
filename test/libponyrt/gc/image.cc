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
}

// A send or a mark doesn't trace an object in read-only image memory, on the
// plain or the routed path; a writable object outside every heap is traced.
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
    ponyint_gc_handlestack(ctx);
    EXPECT_EQ(1u, constant_traced);

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
