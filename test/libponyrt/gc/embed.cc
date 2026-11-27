#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/image.h>
#include <gc/trace.h>
#include <mem/heap.h>
#include <mem/pagemap.h>
#include <mem/pool.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>

// GC visits to objects that share memory with other objects.

// A struct whose first field is embedded shares its address with that field.
// The GcSharedAddress tests visit one address as both types and check that the
// struct's own fields are traced whichever type is visited first.

namespace
{
struct Child
{
  pony_type_t* type;
  size_t pad;
};

// A class embedded at the start of S.
struct C
{
  pony_type_t* type;
  uint64_t x;
};

struct S
{
  C c;
  Child* d;
};

size_t c_traced;
size_t s_traced;
int field_mutability;

void trace_c(pony_ctx_t* ctx, void* p)
{
  (void)ctx;
  (void)p;
  c_traced++;
}

void trace_s(pony_ctx_t* ctx, void* p)
{
  s_traced++;
  S* s = static_cast<S*>(p);
  // An embedded field is traced in place, as compiled code does.
  trace_c(ctx, &s->c);
  pony_traceknown(ctx, s->d, s->d->type, field_mutability);
}

class GcSharedAddress : public testing::Test
{
protected:
  pony_ctx_t* ctx;
  pony_actor_t a{};
  pony_actor_t b{};
  _pony_type_t actor_type{};
  _pony_type_t c_type{};
  _pony_type_t s_type{};
  _pony_type_t child_type{};
  _pony_type_t zero_type{};

  void SetUp() override
  {
    ponyint_register_thread();
    ctx = pony_ctx();
    ctx->current = &a;
    a.type = &actor_type;
    b.type = &actor_type;
    // An embedded struct with no size, which shares its address with the
    // object that holds it.
    zero_type.size = 0;
    zero_type.might_reference_actor = true;
    zero_type.embed_flags = PONY_EMBED_ROUTED;
    ponyint_heap_init(&a.heap);
    ponyint_heap_init(&b.heap);
    ponyint_gc_done(&a.gc);
    ponyint_gc_done(&b.gc);

    // Compiled descriptors all have might_reference_actor set.
    c_type.size = sizeof(C);
    c_type.trace = trace_c;
    c_type.might_reference_actor = true;
    // C is embedded at the start of S.
    c_type.embed_flags = PONY_EMBED_ROUTED | PONY_EMBED_AT_START;
    s_type.size = sizeof(S);
    s_type.trace = trace_s;
    s_type.instance = &c_type;
    s_type.might_reference_actor = true;
    s_type.embed_flags = PONY_EMBED_ROUTED;
    child_type.size = sizeof(Child);
    child_type.might_reference_actor = true;

    c_traced = 0;
    s_traced = 0;
  }

  void TearDown() override
  {
    ponyint_gc_free_visits(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ctx->current = NULL;
    ctx->msg_target = NULL;
    ponyint_gc_destroy(&a.gc);
    ponyint_gc_destroy(&b.gc);
    ponyint_heap_destroy(&a.heap);
    ponyint_heap_destroy(&b.heap);
    ponyint_unregister_thread();
  }

  S* alloc_s(pony_actor_t* owner)
  {
    Child* child = static_cast<Child*>(ponyint_heap_alloc(owner, &owner->heap,
      sizeof(Child), TRACK_NO_FINALISERS));
    child->type = &child_type;
    // S starts with an embedded field, so it is allocated as compiled code
    // allocates such a struct.
    S* s = static_cast<S*>(ponyint_heap_alloc(owner, &owner->heap, sizeof(S),
      TRACK_NO_FINALISERS));
    ponyint_heap_set_shared_start(s);
    s->c.type = &c_type;
    s->c.x = 7;
    s->d = child;
    return s;
  }

  void visit(S* s, bool c_first, int mutability)
  {
    if(c_first)
    {
      pony_traceknown_routed(ctx, s, &c_type, mutability);
      pony_traceknown_routed(ctx, s, &s_type, mutability);
    } else {
      pony_traceknown_routed(ctx, s, &s_type, mutability);
      pony_traceknown_routed(ctx, s, &c_type, mutability);
    }
    ponyint_gc_handlestack(ctx);
  }

  object_t* local(pony_actor_t* owner, void* p)
  {
    size_t index = HASHMAP_UNKNOWN;
    return ponyint_objectmap_getobject(&owner->gc.local, p, &index);
  }

  object_t* foreign(pony_actor_t* holder, pony_actor_t* owner, void* p)
  {
    size_t index = HASHMAP_UNKNOWN;
    actorref_t* aref = ponyint_actormap_getactor(&holder->gc.foreign, owner,
      &index);
    return (aref == NULL) ? NULL : ponyint_actorref_getobject(aref, p);
  }

  // A mark of the owner's own heap starts with every allocation unmarked.
  void start_local_mark(pony_actor_t* owner)
  {
    ponyint_heap_startgc(&owner->heap
#ifdef USE_RUNTIMESTATS
      , owner
#endif
      );
    ponyint_gc_mark(ctx);
  }

  void end_local_mark(pony_actor_t* owner)
  {
    ponyint_heap_endgc(&owner->heap, 0
#ifdef USE_RUNTIMESTATS
      , owner
#endif
      );
    end_epoch(owner);
  }

  void end_epoch(pony_actor_t* actor)
  {
    ponyint_gc_end_visits(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ponyint_gc_done(&actor->gc);
    ctx->msg_target = NULL;
  }
};
}

// Whichever type the mark visits first, the struct's own trace runs once.
TEST_F(GcSharedAddress, MarkTracesStructAfterItsFirstField)
{
  for(bool c_first : {true, false})
  {
    SCOPED_TRACE(c_first ? "C first" : "S first");
    S* s = alloc_s(&a);
    s_traced = 0;
    field_mutability = PONY_TRACE_MUTABLE;
    start_local_mark(&a);
    visit(s, c_first, PONY_TRACE_MUTABLE);
    EXPECT_EQ(1u, s_traced);
    // The struct's trace marked its field (ponyint_heap_mark reports a mark
    // that was already set).
    EXPECT_TRUE(ponyint_heap_mark(ponyint_pagemap_get_chunk(s->d), s->d));
    end_local_mark(&a);
  }
}

// A mark that records many visits grows the set that holds them and still
// finds every one; a later mark that records few gives the storage back.
TEST_F(GcSharedAddress, VisitSetGrowsAndShrinks)
{
  const size_t count = 100;
  S* objects[count];
  for(size_t i = 0; i < count; i++)
    objects[i] = alloc_s(&a);

  start_local_mark(&a);

  // Each struct is visited after its first field has marked the allocation,
  // so each struct visit is recorded in the set.
  for(size_t round = 0; round < 2; round++)
  {
    for(size_t i = 0; i < count; i++)
    {
      pony_traceknown_routed(ctx, objects[i], &c_type, PONY_TRACE_MUTABLE);
      pony_traceknown_routed(ctx, objects[i], &c_type, PONY_TRACE_MUTABLE);
      pony_traceknown_routed(ctx, objects[i], &s_type, PONY_TRACE_MUTABLE);
    }

    ponyint_gc_handlestack(ctx);
    EXPECT_EQ(count, s_traced);
  }

  EXPECT_GE(ctx->visits.capacity, count * 2);
  end_local_mark(&a);
  EXPECT_GE(ctx->visits.capacity, count * 2);

  // The next mark records the struct's visit afresh and traces it again.
  s_traced = 0;
  start_local_mark(&a);
  pony_traceknown_routed(ctx, objects[0], &c_type, PONY_TRACE_MUTABLE);
  pony_traceknown_routed(ctx, objects[0], &s_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, s_traced);
  end_local_mark(&a);
  EXPECT_EQ(0u, ctx->visits.capacity);
}

// An immutable root that is routed and has no heap chunk, as an object in an
// actor's own allocation has none, is marked without touching a heap.
TEST_F(GcSharedAddress, RoutedImmutableRootWithoutAChunk)
{
  _pony_type_t root_type = c_type;
  root_type.might_reference_actor = false;
  C inside{};
  inside.type = &root_type;
  object_t* obj = ponyint_objectmap_getorput(&a.gc.local, &inside, &root_type);
  obj->immutable = true;
  obj->rc = 1;

  start_local_mark(&a);
  ponyint_gc_markimmutable(ctx, &a.gc);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(0u, c_traced);
  end_local_mark(&a);
}

// Sending and receiving visits as both types count the struct's field once
// on each side, so the counts match.
TEST_F(GcSharedAddress, SendAndReceiveCountStructFieldOnce)
{
  // A failed ASSERT returns from run, so the other order is still checked.
  auto run = [&](bool c_first)
  {
    field_mutability = PONY_TRACE_IMMUTABLE;
    S* s = alloc_s(&a);

    ctx->current = &a;
    pony_gc_send(ctx, &b);
    visit(s, c_first, PONY_TRACE_IMMUTABLE);
    object_t* sent = local(&a, s->d);
    ASSERT_NE(nullptr, sent);
    EXPECT_EQ(1u, sent->rc);
    ASSERT_NE(nullptr, local(&a, s));
    EXPECT_EQ(1u, local(&a, s)->rc);
    end_epoch(&a);

    ctx->current = &b;
    pony_gc_recv(ctx);
    visit(s, c_first, PONY_TRACE_IMMUTABLE);
    object_t* received = foreign(&b, &a, s->d);
    ASSERT_NE(nullptr, received);
    EXPECT_EQ(1u, received->rc);
    ASSERT_NE(nullptr, foreign(&b, &a, s));
    EXPECT_EQ(1u, foreign(&b, &a, s)->rc);
    end_epoch(&b);
    ctx->current = &a;
  };

  for(bool c_first : {true, false})
  {
    SCOPED_TRACE(c_first ? "C first" : "S first");
    run(c_first);
  }
}

// An actor that gets its own object back in a message counts the struct's
// field once, whichever type is visited first, so the counts return to zero.
TEST_F(GcSharedAddress, ReceivingOwnObjectCountsStructFieldOnce)
{
  // A failed ASSERT returns from run, so the other order is still checked.
  auto run = [&](bool c_first)
  {
    field_mutability = PONY_TRACE_IMMUTABLE;
    S* s = alloc_s(&a);

    ctx->current = &a;
    pony_gc_send(ctx, &b);
    visit(s, c_first, PONY_TRACE_IMMUTABLE);
    end_epoch(&a);
    ASSERT_NE(nullptr, local(&a, s->d));
    ASSERT_EQ(1u, local(&a, s->d)->rc);

    pony_gc_recv(ctx);
    visit(s, c_first, PONY_TRACE_IMMUTABLE);
    EXPECT_EQ(0u, local(&a, s->d)->rc);
    EXPECT_EQ(0u, local(&a, s)->rc);
    end_epoch(&a);
  };

  for(bool c_first : {true, false})
  {
    SCOPED_TRACE(c_first ? "C first" : "S first");
    run(c_first);
  }
}

// A type recorded for the address in an earlier round doesn't stop a later
// round from tracing the struct when its first field is visited first.
TEST_F(GcSharedAddress, EarlierRoundTypeDoesNotBlockTheStructTrace)
{
  field_mutability = PONY_TRACE_IMMUTABLE;
  S* s = alloc_s(&a);

  ctx->current = &a;
  pony_gc_send(ctx, &b);
  pony_traceknown_routed(ctx, s, &s_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  end_epoch(&a);
  ASSERT_NE(nullptr, local(&a, s->d));
  ASSERT_EQ(1u, local(&a, s->d)->rc);

  pony_gc_send(ctx, &b);
  visit(s, true, PONY_TRACE_IMMUTABLE);
  EXPECT_EQ(2u, local(&a, s->d)->rc);
  end_epoch(&a);
}

// When a send reaches another actor's object first as its embedded field and
// then as the struct, the pending acquire carries the struct's descriptor.
TEST_F(GcSharedAddress, SendPendingAcquireCarriesTheStructType)
{
  field_mutability = PONY_TRACE_IMMUTABLE;
  S* s = alloc_s(&b);
  actorref_t* aref = ponyint_actormap_getorput(&a.gc.foreign, &b);
  aref->rc = GC_INC_MORE;
  ponyint_actorref_getorput(aref, s, &c_type)->rc = 1;
  ponyint_actorref_getorput(aref, s->d, &child_type)->rc = 1;

  ctx->current = &a;
  pony_gc_send(ctx, &b);
  visit(s, true, PONY_TRACE_IMMUTABLE);

  size_t index = HASHMAP_UNKNOWN;
  actorref_t* pending = ponyint_actormap_getactor(&ctx->acquire, &b, &index);
  ASSERT_NE(nullptr, pending);
  object_t* obj = ponyint_actorref_getobject(pending, s);
  ASSERT_NE(nullptr, obj);
  EXPECT_EQ(&s_type, obj->type);
  end_epoch(&a);
}

// An actor holding another actor's struct and that struct's first field marks
// the struct's other field in every collection, including after an earlier
// collection recorded the struct's type for the address.
TEST_F(GcSharedAddress, RemoteMarkTracesStructAfterItsFirstField)
{
  field_mutability = PONY_TRACE_MUTABLE;
  S* s = alloc_s(&b);
  actorref_t* aref = ponyint_actormap_getorput(&a.gc.foreign, &b);
  aref->rc = GC_INC_MORE;
  object_t* obj = ponyint_actorref_getorput(aref, s, &s_type);
  obj->rc = GC_INC_MORE;
  object_t* d = ponyint_actorref_getorput(aref, s->d, &child_type);
  d->rc = GC_INC_MORE;

  for(bool c_first : {true, false, true})
  {
    SCOPED_TRACE(c_first ? "C first" : "S first");
    ctx->current = &a;
    ponyint_gc_mark(ctx);
    visit(s, c_first, PONY_TRACE_MUTABLE);
    EXPECT_TRUE(ponyint_object_marked(d, a.gc.mark));
    end_epoch(&a);
  }
}

// When a mark reaches another actor's object first as its embedded field and
// then as the struct, the pending acquire carries the struct's descriptor.
TEST_F(GcSharedAddress, PendingAcquireCarriesTheStructType)
{
  field_mutability = PONY_TRACE_IMMUTABLE;
  S* s = alloc_s(&b);
  ctx->current = &a;
  ponyint_gc_mark(ctx);
  visit(s, true, PONY_TRACE_IMMUTABLE);

  size_t index = HASHMAP_UNKNOWN;
  actorref_t* pending = ponyint_actormap_getactor(&ctx->acquire, &b, &index);
  ASSERT_NE(nullptr, pending);
  object_t* obj = ponyint_actorref_getobject(pending, s);
  ASSERT_NE(nullptr, obj);
  EXPECT_EQ(&s_type, obj->type);
  end_epoch(&a);
}

// An opaque visit to an object with no size at the struct's address doesn't
// mark the address as traced: the struct's field is still counted when it is
// sent.
TEST_F(GcSharedAddress, ZeroSizeVisitDoesNotHideTheStruct)
{
  field_mutability = PONY_TRACE_IMMUTABLE;
  S* s = alloc_s(&a);
  pony_gc_send(ctx, &b);
  pony_traceknown_routed(ctx, s, &zero_type, PONY_TRACE_OPAQUE);
  pony_traceknown_routed(ctx, s, &s_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  ASSERT_NE(nullptr, local(&a, s->d));
  EXPECT_EQ(1u, local(&a, s->d)->rc);
  // The object map records the struct's type for the shared address, not the
  // empty one visited first.
  ASSERT_NE(nullptr, local(&a, s));
  EXPECT_EQ(&s_type, local(&a, s)->type);
  end_epoch(&a);

  // The same holds for the actor that receives them.
  ctx->current = &b;
  pony_gc_recv(ctx);
  pony_traceknown_routed(ctx, s, &zero_type, PONY_TRACE_OPAQUE);
  pony_traceknown_routed(ctx, s, &s_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  ASSERT_NE(nullptr, foreign(&b, &a, s));
  EXPECT_EQ(&s_type, foreign(&b, &a, s)->type);
  end_epoch(&b);
  ctx->current = &a;
}

// Start and end a collection of one actor's heap.
namespace
{
void start_gc(pony_actor_t* actor)
{
  ponyint_heap_startgc(&actor->heap
#ifdef USE_RUNTIMESTATS
    , actor
#endif
    );
}

void end_gc(pony_actor_t* actor)
{
  ponyint_heap_endgc(&actor->heap, 0
#ifdef USE_RUNTIMESTATS
    , actor
#endif
    );
}
}

// An allocation whose start is shared is never marked, is kept while its
// start is visited, and loses the record when it is freed, so a later
// allocation in the same slot is marked as usual.
TEST(HeapMarkObject, SharedStartLastsAsLongAsTheAllocation)
{
  // A 16 KiB chunk is recycled through
  // heap->recyclable[HEAP_RECYCLE_LARGE_OVERFLOW].
  for(size_t size : {(size_t)64, (size_t)16384})
  {
    SCOPED_TRACE(size);
    ponyint_register_thread();
    pony_actor_t actor{};
    _pony_type_t type{};
    actor.type = &type;
    ponyint_heap_init(&actor.heap);

    // Another allocation, kept throughout, so a small chunk stays in use and
    // the freed slot is reused inside it.
    char* keeper = static_cast<char*>(ponyint_heap_alloc(&actor, &actor.heap,
      size, TRACK_NO_FINALISERS));
    char* p = static_cast<char*>(ponyint_heap_alloc(&actor, &actor.heap,
      size, TRACK_NO_FINALISERS));
    ponyint_heap_set_shared_start(p);
    chunk_t* chunk = ponyint_pagemap_get_chunk(p);
    chunk_t* keeper_chunk = ponyint_pagemap_get_chunk(keeper);

    // Each collection that visits its start keeps it, and it stays recorded.
    // A visit of the allocation's own type marks it, and every later visit
    // finds it marked.
    for(int i = 0; i < 2; i++)
    {
      start_gc(&actor);
      ponyint_heap_mark_object(keeper_chunk, keeper, false);
      EXPECT_EQ(HEAP_MARK_SHARED_START_FIRST,
        ponyint_heap_mark_object(chunk, p, false));
      EXPECT_EQ(HEAP_MARK_SHARED_START,
        ponyint_heap_mark_object(chunk, p, false));
      EXPECT_EQ(HEAP_MARK_NEW, ponyint_heap_mark_object(chunk, p, true));
      EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p, false));
      EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p, true));
      end_gc(&actor);
    }

    // A collection that doesn't visit it frees it. An allocation that reuses
    // the memory is marked as usual.
    start_gc(&actor);
    ponyint_heap_mark_object(keeper_chunk, keeper, false);
    end_gc(&actor);

    char* q = static_cast<char*>(ponyint_heap_alloc(&actor, &actor.heap,
      size, TRACK_NO_FINALISERS));
    ASSERT_EQ(p, q);
    chunk = ponyint_pagemap_get_chunk(q);
    start_gc(&actor);
    EXPECT_EQ(HEAP_MARK_NEW, ponyint_heap_mark_object(chunk, q, false));
    end_gc(&actor);

    ponyint_heap_destroy(&actor.heap);
    ponyint_unregister_thread();
  }
}

// An interior visit keeps its allocation through the sweep.
TEST(HeapMarkObject, InteriorVisitKeepsAllocation)
{
  ponyint_register_thread();
  pony_actor_t actor{};
  _pony_type_t type{};
  actor.type = &type;
  ponyint_heap_init(&actor.heap);

  char* p = static_cast<char*>(ponyint_heap_alloc(&actor, &actor.heap, 64,
    TRACK_NO_FINALISERS));
  chunk_t* chunk = ponyint_pagemap_get_chunk(p);
  start_gc(&actor);
  EXPECT_EQ(HEAP_MARK_INTERIOR_FIRST, ponyint_heap_mark_object(chunk, p + 16, false));
  end_gc(&actor);
  void* next = ponyint_heap_alloc(&actor, &actor.heap, 64, TRACK_NO_FINALISERS);
  EXPECT_NE(static_cast<void*>(p), next);

  ponyint_heap_destroy(&actor.heap);
  ponyint_unregister_thread();
}

TEST(HeapMarkObject, ReportsInteriorPointers)
{
  ponyint_register_thread();
  pony_actor_t actor{};
  _pony_type_t type{};
  actor.type = &type;
  ponyint_heap_init(&actor.heap);

  for(size_t size : {(size_t)64, (size_t)HEAP_MAX * 2})
  {
    SCOPED_TRACE(size);
    char* p = static_cast<char*>(ponyint_heap_alloc(&actor, &actor.heap, size,
      TRACK_NO_FINALISERS));
    chunk_t* chunk = ponyint_pagemap_get_chunk(p);
    start_gc(&actor);
    EXPECT_EQ(HEAP_MARK_INTERIOR_FIRST,
      ponyint_heap_mark_object(chunk, p + 16, false));
    EXPECT_EQ(HEAP_MARK_INTERIOR, ponyint_heap_mark_object(chunk, p + 16, false));
    EXPECT_EQ(HEAP_MARK_NEW, ponyint_heap_mark_object(chunk, p, false));
    EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p, false));
    EXPECT_EQ(HEAP_MARK_COVERED, ponyint_heap_mark_object(chunk, p + 16, false));
    end_gc(&actor);
  }

  ponyint_heap_destroy(&actor.heap);
  ponyint_unregister_thread();
}

namespace
{
// A class embedded in a class, and in nothing else.
struct B
{
  pony_type_t* type;
  uint64_t x;
};

struct A
{
  pony_type_t* type;
  B b;
  Child* d;
};

size_t a_traced;
size_t b_traced;

void trace_b(pony_ctx_t* ctx, void* p)
{
  (void)ctx;
  (void)p;
  b_traced++;
}

void trace_a(pony_ctx_t* ctx, void* p)
{
  a_traced++;
  A* a = static_cast<A*>(p);
  trace_b(ctx, &a->b);
  pony_traceknown(ctx, a->d, a->d->type, PONY_TRACE_MUTABLE);
}

class GcEmbeddedInClass : public testing::Test
{
protected:
  pony_ctx_t* ctx;
  pony_actor_t a{};
  _pony_type_t actor_type{};
  _pony_type_t a_type{};
  _pony_type_t b_type{};
  _pony_type_t child_type{};

  void SetUp() override
  {
    ponyint_register_thread();
    ctx = pony_ctx();
    ctx->current = &a;
    a.type = &actor_type;
    ponyint_heap_init(&a.heap);
    ponyint_gc_done(&a.gc);

    a_type.size = sizeof(A);
    a_type.trace = trace_a;
    a_type.might_reference_actor = true;
    b_type.size = sizeof(B);
    b_type.trace = trace_b;
    b_type.might_reference_actor = true;
    b_type.embed_flags = PONY_EMBED_ROUTED;
    child_type.size = sizeof(Child);
    child_type.might_reference_actor = true;
    a_traced = 0;
    b_traced = 0;
  }

  void TearDown() override
  {
    ponyint_gc_free_visits(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ctx->current = NULL;
    ponyint_gc_destroy(&a.gc);
    ponyint_heap_destroy(&a.heap);
    ponyint_unregister_thread();
  }

  A* alloc_a()
  {
    Child* child = static_cast<Child*>(ponyint_heap_alloc(&a, &a.heap,
      sizeof(Child), TRACK_NO_FINALISERS));
    child->type = &child_type;
    A* obj = static_cast<A*>(ponyint_heap_alloc(&a, &a.heap, sizeof(A),
      TRACK_NO_FINALISERS));
    obj->type = &a_type;
    obj->b.type = &b_type;
    obj->d = child;
    return obj;
  }

  void start_mark()
  {
    ponyint_heap_startgc(&a.heap
#ifdef USE_RUNTIMESTATS
      , &a
#endif
      );
    ponyint_gc_mark(ctx);
  }

  void end_mark()
  {
    ponyint_heap_endgc(&a.heap, 0
#ifdef USE_RUNTIMESTATS
      , &a
#endif
      );
    ponyint_gc_end_visits(ctx);
    ponyint_gc_done(&a.gc);
  }
};
}

// A mark that reaches B through a pointer into its allocation traces B on the
// first visit and on the first repeat, which is recorded; later visits stop.
// Once the class that starts the allocation is traced, a pointer to B needs
// nothing more.
TEST_F(GcEmbeddedInClass, InteriorVisitsEndAndAreCoveredByTheClass)
{
  A* obj = alloc_a();
  start_mark();
  for(int i = 0; i < 3; i++)
    pony_traceknown_routed(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(2u, b_traced);
  EXPECT_EQ(0u, a_traced);

  pony_traceknown(ctx, obj, &a_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, a_traced);
  EXPECT_EQ(3u, b_traced);

  pony_traceknown_routed(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(3u, b_traced);
  end_mark();
}

// Once the class that starts the allocation is traced, a pointer into it to B
// needs nothing more.
TEST_F(GcEmbeddedInClass, InteriorVisitAfterTheClassIsCovered)
{
  A* obj = alloc_a();
  start_mark();
  pony_traceknown(ctx, obj, &a_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, b_traced);

  pony_traceknown_routed(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, b_traced);
  end_mark();
}

// A trace of an interface-typed reference that can reach B reads the descriptor
// at run time and, finding B routed, traces it as a known trace of B would:
// visits end after the recorded repeat.
TEST_F(GcEmbeddedInClass, UnknownTraceFollowsTheDescriptorFlag)
{
  A* obj = alloc_a();
  start_mark();
  for(int i = 0; i < 3; i++)
    pony_traceunknown_routed(ctx, &obj->b, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(2u, b_traced);
  end_mark();
}

namespace
{
void dispatch_nothing(pony_ctx_t* ctx, pony_actor_t* self, pony_msg_t* msg)
{
  (void)ctx;
  (void)self;
  (void)msg;
}
}

// Actor descriptors are constants in the program image, as compiled ones are.
const _pony_type_t small_actor_type = {0,
  (uint32_t)(sizeof(pony_actor_t) + 64), 0, 0, nullptr, nullptr, nullptr,
  nullptr, dispatch_nothing, nullptr, 0, true, PONY_EMBED_ACTOR_LOOKUP,
  nullptr, nullptr, nullptr};

// Constants that start pool memory in PoolMemoryThatIsNotAnActor.
const _pony_type_t not_an_actor_type = {0, 64, 0, 0, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, 0, true, PONY_EMBED_ACTOR_LOOKUP,
  nullptr, nullptr, nullptr};
const _pony_type_t plain_actor_type = {0,
  (uint32_t)(sizeof(pony_actor_t) + 64), 0, 0, nullptr, nullptr, nullptr,
  nullptr, dispatch_nothing, nullptr, 0, true, 0, nullptr, nullptr, nullptr};
const _pony_type_t tiny_actor_type = {0, 16, 0, 0, nullptr, nullptr, nullptr,
  nullptr, dispatch_nothing, nullptr, 0, true, PONY_EMBED_ACTOR_LOOKUP,
  nullptr, nullptr, nullptr};

// Larger than an arena, so the pool gives it a mapping of its own.
const size_t huge_actor_size = 16 * 1024 * 1024;
const _pony_type_t huge_actor_type = {0, (uint32_t)huge_actor_size, 0, 0,
  nullptr, nullptr, nullptr, nullptr, dispatch_nothing, nullptr, 0, true,
  PONY_EMBED_ACTOR_LOOKUP, nullptr, nullptr, nullptr};

// ponyint_actor_containing finds an actor from the address of one of its
// fields.
TEST(ActorContaining, FindsTheActorFromAnInteriorAddress)
{
  ponyint_register_thread();
  ponyint_image_init();

  pony_actor_t* actor = pony_create(pony_ctx(), &small_actor_type, true);
  char* field = reinterpret_cast<char*>(actor) + sizeof(pony_actor_t) + 8;
  EXPECT_EQ(actor, ponyint_actor_containing(field));

  uint64_t local = 0;
  EXPECT_EQ(nullptr, ponyint_actor_containing(&local));

  ponyint_destroy(actor);
  ponyint_unregister_thread();
}

// An actor too large for an arena is found too.
TEST(ActorContaining, FindsAnActorTooLargeForAnArena)
{
  ponyint_register_thread();
  ponyint_image_init();

  pony_actor_t* actor = pony_create(pony_ctx(), &huge_actor_type, true);
  char* field = reinterpret_cast<char*>(actor) + huge_actor_size - 8;
  EXPECT_EQ(actor, ponyint_actor_containing(field));

  ponyint_destroy(actor);
  EXPECT_EQ(nullptr, ponyint_actor_containing(field));
  ponyint_unregister_thread();
}

// An actor type whose descriptor isn't in the image, as when the runtime is
// built into a different image from the program.
static struct _pony_type_t writable_actor_type = {0,
  (uint32_t)(sizeof(pony_actor_t) + 64), 0, 0, nullptr, nullptr, nullptr,
  nullptr, dispatch_nothing, nullptr, 0, true, PONY_EMBED_ACTOR_LOOKUP,
  nullptr, nullptr, nullptr};

// Such an actor is found through the pagemap, which forgets it when it is
// destroyed.
TEST(ActorContaining, FindsAnActorWhoseTypeIsNotInTheImage)
{
  ponyint_register_thread();
  ponyint_image_init();
  ASSERT_FALSE(ponyint_image_constant(&writable_actor_type));

  pony_actor_t* actor = pony_create(pony_ctx(), &writable_actor_type, true);
  char* field = reinterpret_cast<char*>(actor) + sizeof(pony_actor_t) + 8;
  EXPECT_EQ(actor, ponyint_actor_containing(field));

  ponyint_destroy(actor);

  pony_actor_t* in_pagemap = nullptr;
  EXPECT_EQ(nullptr, ponyint_pagemap_get(field, &in_pagemap));
  EXPECT_EQ(nullptr, in_pagemap);
  ponyint_unregister_thread();
}

// Pool memory handed to Pony through FFI isn't taken for an actor.
TEST(ActorContaining, PoolMemoryThatIsNotAnActor)
{
  ponyint_register_thread();
  ponyint_image_init();

  size_t size = sizeof(pony_actor_t) + 64;
  char* block = static_cast<char*>(ponyint_pool_alloc_size(size));
  memset(block, 0, size);
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 16));

  // A first word in the image that isn't an actor descriptor.
  *reinterpret_cast<const _pony_type_t**>(block) = &not_an_actor_type;
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 16));

  // An actor descriptor for a type with no embedded field.
  *reinterpret_cast<const _pony_type_t**>(block) = &plain_actor_type;
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 16));

  // An actor descriptor, but the address is past that actor's size.
  *reinterpret_cast<const _pony_type_t**>(block) = &tiny_actor_type;
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 16));
#ifdef POOL_USE_ARENA
  // In the arena pool, ponyint_pool_base finds the allocation's start, and
  // ponyint_actor_containing takes it for an actor by its descriptor. The
  // memalign pool finds actors only through the pagemap.
  EXPECT_EQ(reinterpret_cast<pony_actor_t*>(block),
    ponyint_actor_containing(block + 8));

  uintptr_t ro_start = ponyint_image_ro_start;
  uintptr_t ro_end = ponyint_image_ro_end;

  // A descriptor that starts in the image but ends past it.
  ponyint_image_ro_start = (uintptr_t)&tiny_actor_type;
  ponyint_image_ro_end = (uintptr_t)&tiny_actor_type + sizeof(_pony_type_t) - 1;
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 8));

  // A descriptor in the image that isn't aligned.
#pragma pack(push, 1)
  struct misaligned_t
  {
    char pad;
    struct _pony_type_t type;
  };
#pragma pack(pop)
  static misaligned_t misaligned = {0, {0, 16, 0, 0, nullptr, nullptr,
    nullptr, nullptr, dispatch_nothing, nullptr, 0, true,
    PONY_EMBED_ACTOR_LOOKUP, nullptr, nullptr, nullptr}};
  ponyint_image_ro_start = (uintptr_t)&misaligned;
  ponyint_image_ro_end = (uintptr_t)&misaligned + sizeof(misaligned);
  *reinterpret_cast<struct _pony_type_t**>(block) = &misaligned.type;
  EXPECT_EQ(nullptr, ponyint_actor_containing(block + 8));

  ponyint_image_ro_start = ro_start;
  ponyint_image_ro_end = ro_end;
#endif

  ponyint_pool_free_size(size, block);
  ponyint_unregister_thread();
}
