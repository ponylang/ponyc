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

  void SetUp() override
  {
    ponyint_register_thread();
    ctx = pony_ctx();
    ctx->current = &a;
    a.type = &actor_type;
    b.type = &actor_type;
    ponyint_heap_init(&a.heap);
    ponyint_heap_init(&b.heap);
    ponyint_gc_done(&a.gc);
    ponyint_gc_done(&b.gc);

    // Compiled descriptors all say they might reference an actor.
    c_type.size = sizeof(C);
    c_type.trace = trace_c;
    c_type.might_reference_actor = true;
    s_type.size = sizeof(S);
    s_type.trace = trace_s;
    s_type.first_embed = &c_type;
    s_type.might_reference_actor = true;
    child_type.size = sizeof(Child);
    child_type.might_reference_actor = true;

    c_traced = 0;
    s_traced = 0;
  }

  void TearDown() override
  {
    ponyint_gc_end_views(ctx);
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
      pony_traceknown(ctx, s, &c_type, mutability);
      pony_traceknown(ctx, s, &s_type, mutability);
    } else {
      pony_traceknown(ctx, s, &s_type, mutability);
      pony_traceknown(ctx, s, &c_type, mutability);
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
    ponyint_gc_mark(ctx);
    visit(s, c_first, PONY_TRACE_MUTABLE);
    EXPECT_EQ(1u, s_traced);
    end_epoch(&a);
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
    end_epoch(&a);

    ctx->current = &b;
    pony_gc_recv(ctx);
    visit(s, c_first, PONY_TRACE_IMMUTABLE);
    object_t* received = foreign(&b, &a, s->d);
    ASSERT_NE(nullptr, received);
    EXPECT_EQ(1u, received->rc);
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
    EXPECT_EQ(HEAP_MARK_INTERIOR, ponyint_heap_mark_object(chunk, p + 16));
    EXPECT_EQ(HEAP_MARK_NEW, ponyint_heap_mark_object(chunk, p));
    EXPECT_EQ(HEAP_MARK_MARKED, ponyint_heap_mark_object(chunk, p));
    EXPECT_EQ(HEAP_MARK_INTERIOR, ponyint_heap_mark_object(chunk, p + 16));
    ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
  }

  ponyint_heap_destroy(&actor.heap);
  ponyint_unregister_thread();
}
