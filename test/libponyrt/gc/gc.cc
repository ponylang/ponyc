#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/trace.h>
#include <mem/heap.h>
#include <mem/pagemap.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

// One trace round (a message send, a message receive, or a GC mark) can reach
// the same object through several aliases in any order, such as a tag and a
// val in one message. These tests trace every order of a tag alias (T) and a
// deep alias (D) and check the bookkeeping each order leaves.

namespace
{
struct Child
{
  pony_type_t* type;
  size_t pad[5];
};

struct Parent
{
  pony_type_t* type;
  Child* child;
};

size_t parent_traced;

void trace_parent(pony_ctx_t* ctx, void* p)
{
  parent_traced++;
  Parent* parent = static_cast<Parent*>(p);
  pony_traceknown(ctx, parent->child, parent->child->type,
    PONY_TRACE_MUTABLE);
}

// The deep alias: a val of a type that cannot reference an actor, a val of a
// type that might, or an iso.
struct Deep
{
  const char* name;
  int mutability;
  bool might_reference_actor;
};

const Deep VAL = {"val", PONY_TRACE_IMMUTABLE, false};
const Deep VAL_MIGHT_REFERENCE_ACTOR =
  {"val-might-reference-actor", PONY_TRACE_IMMUTABLE, true};
const Deep ISO = {"iso", PONY_TRACE_MUTABLE, false};
const Deep deep_cases[] = {VAL, VAL_MIGHT_REFERENCE_ACTOR, ISO};

bool is_val(const Deep& d)
{
  return d.mutability == PONY_TRACE_IMMUTABLE;
}

// A val of a type that cannot reference an actor is the only deep alias whose
// fields a local or receiving trace leaves alone.
bool traces_fields(const Deep& d)
{
  return !is_val(d) || d.might_reference_actor;
}

// A visit through the tag alias or through the deep alias.
enum Visit { T, D };

using Sequence = std::vector<Visit>;

// Every sequence of 1 to 3 visits.
std::vector<Sequence> all_sequences()
{
  std::vector<Sequence> out;
  for(size_t len = 1; len <= 3; len++)
  {
    for(size_t bits = 0; bits < ((size_t)1 << len); bits++)
    {
      Sequence s;
      for(size_t i = 0; i < len; i++)
        s.push_back(((bits >> i) & 1) ? D : T);
      out.push_back(s);
    }
  }
  return out;
}

bool has_deep(const Sequence& s)
{
  for(Visit v : s)
    if(v == D)
      return true;
  return false;
}

std::string describe(const Deep& d, const Sequence& s)
{
  std::string out = d.name;
  out += ":";
  for(Visit v : s)
    out += (v == D) ? " D" : " T";
  return out;
}

struct Seed
{
  size_t rc;
  bool immutable;
};

const size_t EITHER = (size_t)-1;

// How many times a remote send or mark of a val traces the parent's fields.
// Tracing continues into the fields until the owner has the immutable acquire.
// An actor that already holds the object as immutable has nothing to tell the
// owner. One case is EITHER: an actor that held no reference sends or marks
// the tag first, then the val. Tag first, it acquires twice and traces the
// fields; val first, it acquires once as immutable and doesn't. Both are safe.
size_t remote_val_traced(const Deep& d, const Seed& seed)
{
  if(d.might_reference_actor)
    return 1;

  if(seed.immutable)
    return 0;

  if(seed.rc > 0)
    return 1;

  return EITHER;
}

size_t chunk_size(void* p)
{
  return ponyint_heap_size(ponyint_pagemap_get_chunk(p));
}

class GcAliasOrder : public testing::Test
{
protected:
  pony_ctx_t* ctx;
  pony_actor_t a{};
  pony_actor_t b{};
  _pony_type_t actor_type{};
  _pony_type_t parent_type{};
  _pony_type_t parent_type_r{};
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

    // A zero-initialised gc_t records the actor's own reference as already
    // counted in round 0. Start in round 1.
    ponyint_gc_done(&a.gc);

    parent_type.size = sizeof(Parent);
    parent_type.trace = trace_parent;
    parent_type_r = parent_type;
    parent_type_r.might_reference_actor = true;
    child_type.size = sizeof(Child);
  }

  void TearDown() override
  {
    ponyint_actormap_destroy(&ctx->acquire);
    ctx->current = NULL;
    ctx->msg_target = NULL;
    ponyint_gc_destroy(&a.gc);
    ponyint_gc_destroy(&b.gc);
    ponyint_heap_destroy(&a.heap);
    ponyint_heap_destroy(&b.heap);
    ponyint_unregister_thread();
  }

  pony_type_t* type_for(const Deep& d)
  {
    return d.might_reference_actor ? &parent_type_r : &parent_type;
  }

  Parent* alloc_parent(pony_actor_t* owner, const Deep& d)
  {
    Child* child = static_cast<Child*>(ponyint_heap_alloc(owner, &owner->heap,
      sizeof(Child), TRACK_NO_FINALISERS));
    child->type = &child_type;
    Parent* parent = static_cast<Parent*>(ponyint_heap_alloc(owner,
      &owner->heap, sizeof(Parent), TRACK_NO_FINALISERS));
    parent->type = type_for(d);
    parent->child = child;
    return parent;
  }

  // Trace the visits and drain the trace stack, without finishing the round.
  void trace(Parent* p, const Deep& d, const Sequence& s)
  {
    parent_traced = 0;
    for(Visit v : s)
      pony_traceknown(ctx, p, type_for(d),
        (v == D) ? d.mutability : PONY_TRACE_OPAQUE);
    ponyint_gc_handlestack(ctx);
  }

  // Finish the round. Acquire messages are dropped instead of sent, since
  // these actors have no scheduler to receive them.
  void end_round()
  {
    ponyint_actormap_destroy(&ctx->acquire);
    ponyint_gc_done(&a.gc);
    ctx->msg_target = NULL;
  }

  object_t* local(void* p)
  {
    size_t index = HASHMAP_UNKNOWN;
    return ponyint_objectmap_getobject(&a.gc.local, p, &index);
  }

  actorref_t* foreign_ref()
  {
    size_t index = HASHMAP_UNKNOWN;
    return ponyint_actormap_getactor(&a.gc.foreign, &b, &index);
  }

  object_t* foreign(void* p)
  {
    actorref_t* aref = foreign_ref();
    return (aref == NULL) ? NULL : ponyint_actorref_getobject(aref, p);
  }

  object_t* acquired(void* p)
  {
    size_t index = HASHMAP_UNKNOWN;
    actorref_t* aref = ponyint_actormap_getactor(&ctx->acquire, &b, &index);
    return (aref == NULL) ? NULL : ponyint_actorref_getobject(aref, p);
  }

  size_t acquired_rc(void* p)
  {
    object_t* obj = acquired(p);
    return (obj == NULL) ? 0 : obj->rc;
  }

  object_t* seed_foreign(Parent* p, const Deep& d, size_t rc, bool immutable)
  {
    actorref_t* aref = ponyint_actormap_getorput(&a.gc.foreign, &b,
      a.gc.mark);
    object_t* obj = ponyint_actorref_getorput(aref, p, type_for(d),
      a.gc.mark);
    obj->rc = rc;
    obj->immutable = immutable;
    return obj;
  }

  void collect_a()
  {
    a.heap.next_gc = 0;
#ifdef USE_RUNTIMESTATS
    ASSERT_TRUE(ponyint_heap_startgc(&a.heap, &a));
#else
    ASSERT_TRUE(ponyint_heap_startgc(&a.heap));
#endif
    ponyint_gc_mark(ctx);
    ponyint_mark_done(ctx);
#ifdef USE_RUNTIMESTATS
    ponyint_heap_endgc(&a.heap, 0, &a);
#else
    ponyint_heap_endgc(&a.heap, 0);
#endif
  }
};
}

TEST_F(GcAliasOrder, SendLocal)
{
  for(const Deep& d : deep_cases)
  {
    for(const Sequence& s : all_sequences())
    {
      SCOPED_TRACE(describe(d, s));
      Parent* p = alloc_parent(&a, d);
      size_t actor_rc = a.gc.rc;

      pony_gc_send(ctx, &b);
      trace(p, d, s);

      object_t* obj = local(p);
      ASSERT_NE(nullptr, obj);
      EXPECT_EQ(1u, obj->rc);
      EXPECT_EQ(actor_rc + 1, a.gc.rc);

      object_t* child = local(p->child);

      if(has_deep(s))
      {
        EXPECT_EQ(is_val(d), obj->immutable);
        EXPECT_EQ(traces_fields(d) ? 1u : 0u, parent_traced);

        if(traces_fields(d))
        {
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(1u, child->rc);
        } else {
          EXPECT_EQ(nullptr, child);
        }
      } else {
        EXPECT_FALSE(obj->immutable);
        EXPECT_EQ(0u, parent_traced);
        EXPECT_EQ(nullptr, child);
      }

      end_round();
    }
  }
}

TEST_F(GcAliasOrder, RecvLocal)
{
  for(const Deep& d : deep_cases)
  {
    for(const Sequence& s : all_sequences())
    {
      SCOPED_TRACE(describe(d, s));
      Parent* p = alloc_parent(&a, d);

      // The state a send of this object leaves, but with the mutability left
      // unrecorded, so only the receive can record it.
      object_t* obj = ponyint_objectmap_getorput(&a.gc.local, p, type_for(d),
        a.gc.mark);
      obj->rc = 1;
      object_t* child = ponyint_objectmap_getorput(&a.gc.local, p->child,
        &child_type, a.gc.mark);
      child->rc = 1;
      a.gc.rc++;
      size_t actor_rc = a.gc.rc;

      pony_gc_recv(ctx);
      trace(p, d, s);

      EXPECT_EQ(0u, obj->rc);
      EXPECT_EQ(actor_rc - 1, a.gc.rc);

      if(has_deep(s))
      {
        EXPECT_EQ(is_val(d), obj->immutable);
        EXPECT_EQ(traces_fields(d) ? 1u : 0u, parent_traced);
        EXPECT_EQ(traces_fields(d) ? 0u : 1u, child->rc);
      } else {
        EXPECT_FALSE(obj->immutable);
        EXPECT_EQ(0u, parent_traced);
        EXPECT_EQ(1u, child->rc);
      }

      end_round();
    }
  }
}

TEST_F(GcAliasOrder, RecvRemote)
{
  // The first object received from b adds heap pressure for b itself.
  pony_gc_recv(ctx);
  trace(alloc_parent(&b, VAL), VAL, {T});
  end_round();

  for(const Deep& d : deep_cases)
  {
    // The heap pressure a receive of only the deep alias adds.
    Parent* reference = alloc_parent(&b, d);
    size_t used = a.heap.used;
    pony_gc_recv(ctx);
    trace(reference, d, {D});
    size_t deep_only_pressure = a.heap.used - used;
    end_round();

    // This actor doesn't trace a val that can't reference an actor, so it
    // adds pressure beyond the val's own size for what the val reaches.
    if(!traces_fields(d))
      EXPECT_GT(deep_only_pressure, chunk_size(reference));

    for(const Sequence& s : all_sequences())
    {
      SCOPED_TRACE(describe(d, s));
      Parent* p = alloc_parent(&b, d);
      actorref_t* aref = foreign_ref();
      size_t actor_rc = (aref == NULL) ? 0 : aref->rc;
      used = a.heap.used;

      pony_gc_recv(ctx);
      trace(p, d, s);

      object_t* obj = foreign(p);
      ASSERT_NE(nullptr, obj);
      EXPECT_EQ(1u, obj->rc);
      EXPECT_EQ(actor_rc + 1, foreign_ref()->rc);

      object_t* child = foreign(p->child);

      if(has_deep(s))
      {
        EXPECT_EQ(is_val(d), obj->immutable);
        EXPECT_EQ(traces_fields(d) ? 1u : 0u, parent_traced);
        EXPECT_EQ(deep_only_pressure, a.heap.used - used);

        if(traces_fields(d))
        {
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(1u, child->rc);
        } else {
          EXPECT_EQ(nullptr, child);
        }
      } else {
        EXPECT_FALSE(obj->immutable);
        EXPECT_EQ(0u, parent_traced);
        EXPECT_EQ(nullptr, child);
      }

      end_round();
    }
  }
}

// Receiving a val of an object this actor already holds adds no heap
// pressure, whichever alias comes first: the memory was counted when the
// object first arrived.
TEST_F(GcAliasOrder, RecvRemoteAlreadyHeld)
{
  for(const Sequence& s : all_sequences())
  {
    if(!has_deep(s))
      continue;

    SCOPED_TRACE(describe(VAL, s));
    Parent* p = alloc_parent(&b, VAL);
    object_t* obj = seed_foreign(p, VAL, 2, false);
    foreign_ref()->rc++;
    size_t used = a.heap.used;

    pony_gc_recv(ctx);
    trace(p, VAL, s);

    EXPECT_EQ(3u, obj->rc);
    EXPECT_TRUE(obj->immutable);
    EXPECT_EQ(0u, a.heap.used - used);
    end_round();
  }
}

TEST_F(GcAliasOrder, SendRemote)
{
  const Seed seeds[] = {{0, false}, {1, false}, {2, false}, {1, true},
    {2, true}};

  for(const Deep& d : deep_cases)
  {
    for(const Seed& seed : seeds)
    {
      for(const Sequence& s : all_sequences())
      {
        // An actor can't send an iso of an object it holds as immutable.
        if(!is_val(d) && seed.immutable)
          continue;

        SCOPED_TRACE(describe(d, s) + " seed rc " + std::to_string(seed.rc) +
          (seed.immutable ? " immutable" : " mutable"));
        Parent* p = alloc_parent(&b, d);
        object_t* obj = seed_foreign(p, d, seed.rc, seed.immutable);

        pony_gc_send(ctx, &b);
        trace(p, d, s);

        // The message holds one reference. Whatever this actor adds to its
        // own count, it also acquires from the owner.
        EXPECT_EQ(seed.rc + acquired_rc(p), obj->rc + 1);

        // Already immutable, with references to spare: nothing to acquire.
        if(seed.immutable && (seed.rc > 1))
          EXPECT_EQ(nullptr, acquired(p));

        if(!has_deep(s))
        {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(0u, parent_traced);
        } else if(is_val(d)) {
          EXPECT_TRUE(obj->immutable);

          if(!seed.immutable)
          {
            // The owner must learn the object is now immutable.
            object_t* acq = acquired(p);
            ASSERT_NE(nullptr, acq);
            EXPECT_TRUE(acq->immutable);
          }

          size_t expected_traced = remote_val_traced(d, seed);
          if(expected_traced != EITHER)
            EXPECT_EQ(expected_traced, parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);
        }

        // A traced child goes into the message too, acquired from the owner.
        if(parent_traced == 1)
        {
          object_t* child = foreign(p->child);
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(acquired_rc(p->child), child->rc + 1);
        }

        end_round();
      }
    }
  }
}

TEST_F(GcAliasOrder, MarkRemote)
{
  const Seed seeds[] = {{0, false}, {GC_INC_MORE, false},
    {GC_INC_MORE, true}};

  for(const Deep& d : deep_cases)
  {
    for(const Seed& seed : seeds)
    {
      if(!is_val(d) && seed.immutable)
        continue;

      // The heap pressure and field tracing a mark of only the deep alias
      // gives, from the same starting state.
      Parent* reference = alloc_parent(&b, d);
      seed_foreign(reference, d, seed.rc, seed.immutable);
      size_t used = a.heap.used;
      ponyint_gc_mark(ctx);
      trace(reference, d, {D});
      size_t deep_only_pressure = a.heap.used - used;
      size_t deep_only_traced = parent_traced;
      end_round();

      // A val this mark doesn't trace adds pressure beyond its own size for
      // what the val reaches. Mark another of b's objects first, so that the
      // pressure for b itself isn't counted.
      if(is_val(d) && (deep_only_traced == 0))
      {
        Parent* other = alloc_parent(&b, d);
        Parent* val = alloc_parent(&b, d);
        seed_foreign(val, d, seed.rc, seed.immutable);
        ponyint_gc_mark(ctx);
        trace(other, d, {T});
        used = a.heap.used;
        trace(val, d, {D});
        EXPECT_GT(a.heap.used - used, chunk_size(val));
        end_round();
      }

      for(const Sequence& s : all_sequences())
      {
        SCOPED_TRACE(describe(d, s) + " seed rc " + std::to_string(seed.rc) +
          (seed.immutable ? " immutable" : " mutable"));
        Parent* p = alloc_parent(&b, d);
        object_t* obj = seed_foreign(p, d, seed.rc, seed.immutable);
        used = a.heap.used;

        ponyint_gc_mark(ctx);
        trace(p, d, s);

        // Marking hands over no reference: whatever this actor adds to its own
        // count, it acquires from the owner.
        EXPECT_EQ(seed.rc + acquired_rc(p), obj->rc);
        EXPECT_EQ(a.gc.mark, obj->mark);

        if(seed.immutable)
          EXPECT_EQ(nullptr, acquired(p));

        if(!has_deep(s))
        {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(0u, parent_traced);
        } else if(is_val(d)) {
          EXPECT_TRUE(obj->immutable);

          if(!seed.immutable)
          {
            object_t* acq = acquired(p);
            ASSERT_NE(nullptr, acq);
            EXPECT_TRUE(acq->immutable);
          }

          size_t expected_traced = remote_val_traced(d, seed);
          if(expected_traced != EITHER)
            EXPECT_EQ(expected_traced, parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);
        }

        if(has_deep(s))
        {
          // Each order adds the same pressure as the deep alias alone, plus
          // the child's size when it traces the child and the deep alias
          // alone doesn't.
          size_t child_pressure = (parent_traced > deep_only_traced) ?
            chunk_size(p->child) : 0;
          EXPECT_EQ(deep_only_pressure + child_pressure, a.heap.used - used);
        }

        // A traced child is marked, and acquired from the owner if this actor
        // held no references to it.
        if(parent_traced == 1)
        {
          object_t* child = foreign(p->child);
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(a.gc.mark, child->mark);
          EXPECT_EQ(acquired_rc(p->child), child->rc);
        }

        end_round();
      }
    }
  }
}

// After a message carries a parent as a tag and as a val or iso, in either
// order, the sender's GC keeps the child that the receiver can reach.
TEST_F(GcAliasOrder, SenderGcKeepsChildOfDeepAlias)
{
  for(const Deep& d : deep_cases)
  {
    for(const Sequence& s : {Sequence{T, D}, Sequence{D, T}})
    {
      SCOPED_TRACE(describe(d, s));
      Parent* p = alloc_parent(&a, d);
      size_t live = a.heap.used;

      pony_gc_send(ctx, &b);
      trace(p, d, s);
      end_round();

      // Nothing in the actor itself refers to the parent.
      collect_a();
      EXPECT_EQ(live, a.heap.used);
    }
  }
}

// A round with only tag visits leaves nothing that changes what the next round
// does on its first visit.
TEST_F(GcAliasOrder, TagOnlyRoundDoesNotCarryOver)
{
  Parent* p = alloc_parent(&a, ISO);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {T});
  end_round();
  EXPECT_EQ(0u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {D, D});
  end_round();
  EXPECT_EQ(1u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {T, D});
  end_round();
  EXPECT_EQ(1u, parent_traced);

  EXPECT_EQ(3u, local(p)->rc);
  EXPECT_EQ(2u, local(p->child)->rc);
}

// A self-send pins a foreign object once per message, however many aliases
// of it the message holds.
TEST_F(GcAliasOrder, SelfSendPinsOncePerMessage)
{
  for(const Sequence& s : all_sequences())
  {
    SCOPED_TRACE(describe(VAL_MIGHT_REFERENCE_ACTOR, s));
    Parent* p = alloc_parent(&b, VAL_MIGHT_REFERENCE_ACTOR);

    pony_gc_send(ctx, &a);
    trace(p, VAL_MIGHT_REFERENCE_ACTOR, s);

    object_t* obj = foreign(p);
    ASSERT_NE(nullptr, obj);
    EXPECT_EQ(1u, obj->self_send_pins);
    end_round();
  }
}

// An untyped trace records no type for an object. A later typed visit, in the
// same round or a later one, supplies it, so the GC can trace the object's
// fields when it marks it as immutable.
TEST_F(GcAliasOrder, UntypedTraceTakesTypeFromLaterVisit)
{
  Parent* same_round = alloc_parent(&a, VAL);
  pony_gc_send(ctx, &b);
  pony_trace(ctx, same_round);
  pony_traceknown(ctx, same_round, &parent_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  end_round();
  EXPECT_EQ(&parent_type, local(same_round)->type);

  Parent* later_round = alloc_parent(&a, VAL);
  pony_gc_send(ctx, &b);
  pony_trace(ctx, later_round);
  end_round();
  EXPECT_EQ(nullptr, local(later_round)->type);

  pony_gc_send(ctx, &b);
  pony_traceknown(ctx, later_round, &parent_type, PONY_TRACE_IMMUTABLE);
  ponyint_gc_handlestack(ctx);
  end_round();
  EXPECT_EQ(&parent_type, local(later_round)->type);

  // Nothing in the actor itself refers to either parent.
  size_t live = a.heap.used;
  collect_a();
  EXPECT_EQ(live, a.heap.used);
}
