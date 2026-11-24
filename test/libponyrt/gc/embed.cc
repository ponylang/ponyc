#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/trace.h>
#include <mem/heap.h>
#include <mem/pagemap.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>

// A struct whose first field is embedded shares its address with that field.
// These tests visit one address as both types and check that the struct's
// own fields are traced whichever view comes first.

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
    // An embedded struct with no fields, which shares its address with
    // whatever follows it.
    zero_type.size = 0;
    zero_type.might_reference_actor = true;
    zero_type.embed_flags = PONY_EMBED_ROUTED | PONY_EMBED_AT_START;
    ponyint_heap_init(&a.heap);
    ponyint_heap_init(&b.heap);
    ponyint_gc_done(&a.gc);
    ponyint_gc_done(&b.gc);

    // Compiled descriptors all say they might reference an actor.
    c_type.size = sizeof(C);
    c_type.trace = trace_c;
    c_type.might_reference_actor = true;
    // C is embedded at the start of S.
    c_type.embed_flags = PONY_EMBED_ROUTED | PONY_EMBED_AT_START;
    s_type.size = sizeof(S);
    s_type.trace = trace_s;
    s_type.first_embed = &c_type;
    s_type.might_reference_actor = true;
    s_type.embed_flags = PONY_EMBED_ROUTED;
    child_type.size = sizeof(Child);
    child_type.might_reference_actor = true;

    c_traced = 0;
    s_traced = 0;
  }

  void TearDown() override
  {
    ponyint_gc_free_views(ctx);
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
    S* s = static_cast<S*>(ponyint_heap_alloc(owner, &owner->heap, sizeof(S),
      TRACK_NO_FINALISERS));
    s->c.type = &c_type;
    s->c.x = 7;
    s->d = child;
    return s;
  }

  void visit(S* s, bool c_first, int mutability)
  {
    if(c_first)
    {
      pony_traceknown_embeddable(ctx, s, &c_type, mutability);
      pony_traceknown_embeddable(ctx, s, &s_type, mutability);
    } else {
      pony_traceknown_embeddable(ctx, s, &s_type, mutability);
      pony_traceknown_embeddable(ctx, s, &c_type, mutability);
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
    ponyint_gc_end_views(ctx);
    ponyint_actormap_destroy(&ctx->acquire);
    ponyint_gc_done(&actor->gc);
    ctx->msg_target = NULL;
  }
};
}

// The mark records the struct's visit apart from the allocation's mark, which
// the embedded field's visit already set.
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
    // The struct's field survives the sweep, and so does the struct.
    end_local_mark(&a);
    Child* next = static_cast<Child*>(ponyint_heap_alloc(&a, &a.heap,
      sizeof(Child), TRACK_NO_FINALISERS));
    EXPECT_NE(s->d, next);
  }
}

// Sending and receiving the same two views count the struct's field once on
// each side, so the counts match.
TEST_F(GcSharedAddress, SendAndReceiveCountStructFieldOnce)
{
  for(bool c_first : {true, false})
  {
    SCOPED_TRACE(c_first ? "C first" : "S first");
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
  }
}

// An actor holding both views of another actor's object marks the struct's
// field in every collection, including after an earlier collection recorded
// the struct's type for the address.
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

// A mark that reaches another actor's object first through its embedded field
// and then as the struct acquires it with the struct's descriptor.
TEST_F(GcSharedAddress, UpgradeGivesPendingAcquireTheStructType)
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

// An interior visit keeps its allocation through the sweep.
// A visit to an object with no size at the struct's address doesn't stand in
// for the struct: the struct's field is still counted when it is sent.
TEST_F(GcSharedAddress, ZeroSizeVisitDoesNotHideTheStruct)
{
  field_mutability = PONY_TRACE_IMMUTABLE;
  S* s = alloc_s(&a);
  pony_gc_send(ctx, &b);
  pony_traceknown_embeddable(ctx, s, &zero_type, PONY_TRACE_IMMUTABLE);
  pony_traceknown_embeddable(ctx, s, &s_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  ASSERT_NE(nullptr, local(&a, s->d));
  EXPECT_EQ(1u, local(&a, s->d)->rc);
  end_epoch(&a);
}

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
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  EXPECT_EQ(HEAP_MARK_INTERIOR, ponyint_heap_mark_object(chunk, p + 16, true));
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  void* next = ponyint_heap_alloc(&actor, &actor.heap, 64,
    TRACK_NO_FINALISERS);
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
    ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
    EXPECT_EQ(HEAP_MARK_INTERIOR,
      ponyint_heap_mark_object(chunk, p + 16, true));
    EXPECT_EQ(HEAP_MARK_UNCLAIMED, ponyint_heap_mark_object(chunk, p, false));
    EXPECT_EQ(HEAP_MARK_NEW, ponyint_heap_mark_object(chunk, p, true));
    EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p, true));
    EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p, false));
    EXPECT_EQ(HEAP_MARK_COVERED,
      ponyint_heap_mark_object(chunk, p + 16, true));
    ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
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
    ponyint_gc_free_views(ctx);
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
    ponyint_gc_end_views(ctx);
    ponyint_gc_done(&a.gc);
  }
};
}

// A mark that reaches B through a pointer into its allocation traces B once,
// and once the class that starts the allocation is traced, a pointer to B
// needs nothing more.
TEST_F(GcEmbeddedInClass, InteriorVisitTracedOnceAndCoveredByTheClass)
{
  A* obj = alloc_a();
  start_mark();
  pony_traceknown_embeddable(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  pony_traceknown_embeddable(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, b_traced);
  EXPECT_EQ(0u, a_traced);

  pony_traceknown(ctx, obj, &a_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, a_traced);
  EXPECT_EQ(2u, b_traced);

  pony_traceknown_embeddable(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(2u, b_traced);
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

  pony_traceknown_embeddable(ctx, &obj->b, &b_type, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, b_traced);
  end_mark();
}

// A trace of an interface-typed reference reads the descriptor at run time and
// takes the same path for B: a repeat visit is recorded, not traced again.
TEST_F(GcEmbeddedInClass, UnknownTraceFollowsTheDescriptorFlag)
{
  A* obj = alloc_a();
  start_mark();
  pony_traceunknown(ctx, &obj->b, PONY_TRACE_MUTABLE);
  pony_traceunknown(ctx, &obj->b, PONY_TRACE_MUTABLE);
  ponyint_gc_handlestack(ctx);
  EXPECT_EQ(1u, b_traced);
  end_mark();
}
