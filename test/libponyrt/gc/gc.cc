#include <platform.h>

#include <actor/actor.h>
#include <gc/gc.h>
#include <gc/trace.h>
#include <mem/heap.h>
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

const Deep deep_cases[] = {
  {"val", PONY_TRACE_IMMUTABLE, false},
  {"val-might-reference-actor", PONY_TRACE_IMMUTABLE, true},
  {"iso", PONY_TRACE_MUTABLE, false},
};

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

using Sequence = std::vector<bool>; // true is a deep visit, false a tag visit

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
        s.push_back(((bits >> i) & 1) != 0);
      out.push_back(s);
    }
  }
  return out;
}

bool has_deep(const Sequence& s)
{
  for(bool deep : s)
    if(deep)
      return true;
  return false;
}

std::string describe(const Deep& d, const Sequence& s)
{
  std::string out = d.name;
  out += ":";
  for(bool deep : s)
    out += deep ? " D" : " T";
  return out;
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
    for(bool deep : s)
      pony_traceknown(ctx, p, type_for(d),
        deep ? d.mutability : PONY_TRACE_OPAQUE);
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

      // Balance the child so later rounds don't see a stale count.
      child->rc = 0;
      end_round();
    }
  }
}

TEST_F(GcAliasOrder, RecvRemote)
{
  // The first object received from b adds heap pressure for b itself.
  pony_gc_recv(ctx);
  trace(alloc_parent(&b, deep_cases[0]), deep_cases[0], {false});
  end_round();

  for(const Deep& d : deep_cases)
  {
    // The heap pressure a receive of only the deep alias adds.
    Parent* reference = alloc_parent(&b, d);
    size_t used = a.heap.used;
    pony_gc_recv(ctx);
    trace(reference, d, {true});
    size_t deep_only_pressure = a.heap.used - used;
    end_round();

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
  const Deep& val = deep_cases[0];

  for(const Sequence& s : all_sequences())
  {
    if(!has_deep(s))
      continue;

    SCOPED_TRACE(describe(val, s));
    Parent* p = alloc_parent(&b, val);
    object_t* obj = seed_foreign(p, val, 2, false);
    foreign_ref()->rc++;
    size_t used = a.heap.used;

    pony_gc_recv(ctx);
    trace(p, val, s);

    EXPECT_EQ(3u, obj->rc);
    EXPECT_TRUE(obj->immutable);
    EXPECT_EQ(0u, a.heap.used - used);
    end_round();
  }
}

namespace
{
struct Seed
{
  size_t rc;
  bool immutable;
};
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

          // Tracing continues into the fields until the owner has the acquire,
          // unless this actor already held the object as immutable.
          size_t expected_traced;
          if(d.might_reference_actor)
            expected_traced = 1;
          else if(seed.immutable)
            expected_traced = 0;
          else if(seed.rc > 0)
            expected_traced = 1;
          else
            expected_traced = s[0] ? 0 : 1;

          EXPECT_EQ(expected_traced, parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);

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
      trace(reference, d, {true});
      size_t deep_only_pressure = a.heap.used - used;
      size_t deep_only_traced = parent_traced;
      end_round();

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

          // Tracing the fields adds the child's pressure too.
          if(parent_traced == deep_only_traced)
            EXPECT_EQ(deep_only_pressure, a.heap.used - used);

          if(!seed.immutable)
          {
            object_t* acq = acquired(p);
            ASSERT_NE(nullptr, acq);
            EXPECT_TRUE(acq->immutable);
          }

          size_t expected_traced;
          if(d.might_reference_actor)
            expected_traced = 1;
          else if(seed.immutable)
            expected_traced = 0;
          else if(seed.rc > 0)
            expected_traced = 1;
          else
            expected_traced = s[0] ? 0 : 1;

          EXPECT_EQ(expected_traced, parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);
          EXPECT_EQ(deep_only_pressure, a.heap.used - used);

          object_t* child = foreign(p->child);
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(a.gc.mark, child->mark);
        }

        end_round();
      }
    }
  }
}

// The use-after-free behind the bookkeeping: after sending a parent as a tag
// and then as a val or iso in one message, the sender's GC must keep the child
// the receiver can reach.
TEST_F(GcAliasOrder, SenderGcKeepsChildOfDeepAlias)
{
  for(const Deep& d : deep_cases)
  {
    for(const Sequence& s : {Sequence{false, true}, Sequence{true, false}})
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

// A round with only tag visits must not change how the next round treats its
// first visit.
TEST_F(GcAliasOrder, TagOnlyRoundDoesNotCarryOver)
{
  const Deep& iso = deep_cases[2];
  Parent* p = alloc_parent(&a, iso);

  pony_gc_send(ctx, &b);
  trace(p, iso, {false});
  end_round();
  EXPECT_EQ(0u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, iso, {true, true});
  end_round();
  EXPECT_EQ(1u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, iso, {false, true});
  end_round();
  EXPECT_EQ(1u, parent_traced);

  EXPECT_EQ(3u, local(p)->rc);
  EXPECT_EQ(2u, local(p->child)->rc);
}

// A self-send pins a foreign object once per message, however many aliases
// of it the message holds.
TEST_F(GcAliasOrder, SelfSendPinsOncePerMessage)
{
  const Deep& val = deep_cases[1];

  for(const Sequence& s : all_sequences())
  {
    SCOPED_TRACE(describe(val, s));
    Parent* p = alloc_parent(&b, val);

    pony_gc_send(ctx, &a);
    trace(p, val, s);

    object_t* obj = foreign(p);
    ASSERT_NE(nullptr, obj);
    EXPECT_EQ(1u, obj->self_send_pins);
    end_round();
  }
}
