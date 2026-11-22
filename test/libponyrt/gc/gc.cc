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

// One GC epoch (a message send, a message receive, or a GC mark) can reach
// the same object through several aliases in any order, such as a tag and a
// val in one message.

namespace
{
// A child larger than its parent, so the two are in different size classes
// and add different amounts of heap pressure.
struct Child
{
  pony_type_t* type;
  size_t pad[8];
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
  pony_traceknown(ctx, parent->child, parent->child->type, PONY_TRACE_MUTABLE);
}

// An object that holds a parent as a val.
struct Holder
{
  pony_type_t* type;
  Parent* parent;
};

void trace_holder(pony_ctx_t* ctx, void* p)
{
  Holder* holder = static_cast<Holder*>(p);
  pony_traceknown(ctx, holder->parent, holder->parent->type,
    PONY_TRACE_IMMUTABLE);
}

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

bool traces_fields(const Deep& d)
{
  return !is_val(d) || d.might_reference_actor;
}

enum Visit { TagAlias, DeepAlias };

using Sequence = std::vector<Visit>;

std::vector<Sequence> all_sequences()
{
  std::vector<Sequence> out;
  for(size_t len = 1; len <= 3; len++)
  {
    for(size_t bits = 0; bits < ((size_t)1 << len); bits++)
    {
      Sequence s;
      for(size_t i = 0; i < len; i++)
        s.push_back(((bits >> i) & 1) ? DeepAlias : TagAlias);
      out.push_back(s);
    }
  }
  return out;
}

bool has_deep(const Sequence& s)
{
  for(Visit v : s)
    if(v == DeepAlias)
      return true;
  return false;
}

std::string describe(const Deep& d, const Sequence& s)
{
  std::string out = d.name;
  out += ":";
  for(Visit v : s)
    out += (v == DeepAlias) ? " DeepAlias" : " TagAlias";
  return out;
}

struct Seed
{
  size_t rc;
  bool immutable;
};

// An actor that already held a counted reference must trace the fields
// until the owner receives its first immutable acquire for the object.
size_t remote_val_traced(const Deep& d, const Seed& seed)
{
  if(d.might_reference_actor)
    return 1;

  if(!seed.immutable && (seed.rc > 0))
    return 1;

  return 0;
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
  _pony_type_t holder_type{};

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
    // counted in epoch 0. Start both actors in their first live epoch.
    ponyint_gc_done(&a.gc);
    ponyint_gc_done(&b.gc);

    parent_type.size = sizeof(Parent);
    parent_type.trace = trace_parent;
    parent_type_r = parent_type;
    parent_type_r.might_reference_actor = true;
    child_type.size = sizeof(Child);
    holder_type.size = sizeof(Holder);
    holder_type.trace = trace_holder;
    holder_type.might_reference_actor = true;
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

  void trace(Parent* p, const Deep& d, const Sequence& s)
  {
    parent_traced = 0;
    for(Visit v : s)
    {
      if(v == DeepAlias)
        pony_traceknown(ctx, p, type_for(d), d.mutability);
      else
        pony_traceknown(ctx, p, type_for(d), PONY_TRACE_OPAQUE);
    }
    ponyint_gc_handlestack(ctx);
  }

  // These actors have no scheduler to deliver acquire messages.
  void end_epoch()
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

  size_t acquired_actor_rc()
  {
    size_t index = HASHMAP_UNKNOWN;
    actorref_t* aref = ponyint_actormap_getactor(&ctx->acquire, &b, &index);
    return (aref == NULL) ? 0 : aref->rc;
  }

  size_t acquired_rc(void* p)
  {
    object_t* obj = acquired(p);
    return (obj == NULL) ? 0 : obj->rc;
  }

  object_t* seed_foreign(Parent* p, const Deep& d, size_t rc, bool immutable)
  {
    actorref_t* aref = ponyint_actormap_getorput(&a.gc.foreign, &b);
    object_t* obj = ponyint_actorref_getorput(aref, p, type_for(d));
    obj->rc = rc;
    obj->immutable = immutable;
    return obj;
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

      end_epoch();
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

      // The receive supplies the missing parent descriptor. It consumes the
      // child's reference only if it traces the parent's fields.
      object_t* obj = ponyint_objectmap_getorput(&a.gc.local, p, nullptr);
      obj->rc = 1;
      object_t* child = ponyint_objectmap_getorput(&a.gc.local, p->child,
        &child_type);
      child->rc = 1;
      a.gc.rc++;
      size_t actor_rc = a.gc.rc;

      pony_gc_recv(ctx);
      trace(p, d, s);

      // A later untyped visit must preserve the descriptor.
      pony_trace(ctx, p);
      EXPECT_EQ(type_for(d), obj->type);
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

      end_epoch();
    }
  }
}

TEST_F(GcAliasOrder, RecvRemote)
{
  // The first object received from b adds heap pressure for b itself.
  pony_gc_recv(ctx);
  trace(alloc_parent(&b, VAL), VAL, {TagAlias});
  end_epoch();

  for(const Deep& d : deep_cases)
  {
    Parent* reference = alloc_parent(&b, d);
    size_t used = a.heap.used;
    pony_gc_recv(ctx);
    trace(reference, d, {DeepAlias});
    size_t deep_only_pressure = a.heap.used - used;
    end_epoch();

    reference = alloc_parent(&b, d);
    used = a.heap.used;
    pony_gc_recv(ctx);
    trace(reference, d, {TagAlias});
    size_t tag_only_pressure = a.heap.used - used;
    end_epoch();

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
        EXPECT_EQ(tag_only_pressure, a.heap.used - used);
        EXPECT_EQ(nullptr, child);
      }

      end_epoch();
    }
  }
}

TEST_F(GcAliasOrder, RecvRemoteAlreadyHeld)
{
  for(const Deep& d : deep_cases)
  {
    for(const Seed& seed : {Seed{1, false}, Seed{1, true}})
    {
      // An actor can't hold an iso of an object it holds as immutable.
      if(!is_val(d) && seed.immutable)
        continue;

      for(const Sequence& s : all_sequences())
      {
        if(!has_deep(s))
          continue;

        SCOPED_TRACE(describe(d, s) + (seed.immutable ? " immutable" :
          " mutable"));
        Parent* p = alloc_parent(&b, d);
        object_t* obj = seed_foreign(p, d, seed.rc, seed.immutable);
        foreign_ref()->rc++;
        size_t used = a.heap.used;

        pony_gc_recv(ctx);
        trace(p, d, s);

        EXPECT_EQ(seed.rc + 1, obj->rc);
        EXPECT_EQ(is_val(d) || seed.immutable, obj->immutable);

        bool traced = !obj->immutable || d.might_reference_actor;
        EXPECT_EQ(traced ? 1u : 0u, parent_traced);
        EXPECT_EQ(traced ? chunk_size(p->child) : 0, a.heap.used - used);
        end_epoch();
      }
    }
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
        size_t owner_rc = foreign_ref()->rc;

        pony_gc_send(ctx, &b);
        trace(p, d, s);

        // The message holds one reference to the object and one to its owner.
        // Whatever this actor adds to its own counts, it also acquires from
        // the owner.
        EXPECT_EQ(seed.rc + acquired_rc(p), obj->rc + 1);
        EXPECT_EQ(owner_rc + acquired_actor_rc(), foreign_ref()->rc + 1);
        EXPECT_EQ(a.gc.mark, foreign_ref()->mark);

        bool upgrade = has_deep(s) && is_val(d) && !seed.immutable;
        EXPECT_EQ(((seed.rc <= 1) || upgrade) ? GC_INC_MORE : 0u,
          acquired_rc(p));

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
            object_t* acq = acquired(p);
            ASSERT_NE(nullptr, acq);
            EXPECT_TRUE(acq->immutable);
          }

          EXPECT_EQ(remote_val_traced(d, seed), parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);
        }

        if(parent_traced == 1)
        {
          object_t* child = foreign(p->child);
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(static_cast<size_t>(GC_INC_MORE), acquired_rc(p->child));
          EXPECT_EQ(static_cast<size_t>(GC_INC_MORE - 1), child->rc);
        }

        end_epoch();
      }
    }
  }
}

TEST_F(GcAliasOrder, MarkRemote)
{
  const Seed seeds[] = {{0, false}, {GC_INC_MORE, false}, {GC_INC_MORE, true}};

  for(const Deep& d : deep_cases)
  {
    for(const Seed& seed : seeds)
    {
      if(!is_val(d) && seed.immutable)
        continue;

      Parent* reference = alloc_parent(&b, d);
      seed_foreign(reference, d, seed.rc, seed.immutable);
      size_t used = a.heap.used;
      ponyint_gc_mark(ctx);
      trace(reference, d, {DeepAlias});
      size_t deep_only_pressure = a.heap.used - used;
      size_t deep_only_traced = parent_traced;
      end_epoch();

      reference = alloc_parent(&b, d);
      seed_foreign(reference, d, seed.rc, seed.immutable);
      used = a.heap.used;
      ponyint_gc_mark(ctx);
      ponyint_gc_markactor(ctx, &b);
      size_t before_object = a.heap.used;
      trace(reference, d, {TagAlias});
      EXPECT_EQ(chunk_size(reference), a.heap.used - before_object);
      size_t tag_only_pressure = a.heap.used - used;
      end_epoch();

      // A val this mark doesn't trace adds pressure beyond its own size for
      // what the val reaches. Mark another of b's objects first, so that the
      // pressure for b itself isn't counted.
      if(is_val(d) && (deep_only_traced == 0))
      {
        Parent* other = alloc_parent(&b, d);
        Parent* val = alloc_parent(&b, d);
        seed_foreign(val, d, seed.rc, seed.immutable);
        ponyint_gc_mark(ctx);
        trace(other, d, {TagAlias});
        used = a.heap.used;
        trace(val, d, {DeepAlias});
        EXPECT_GT(a.heap.used - used, chunk_size(val));
        end_epoch();
      }

      for(const Sequence& s : all_sequences())
      {
        SCOPED_TRACE(describe(d, s) + " seed rc " + std::to_string(seed.rc) +
          (seed.immutable ? " immutable" : " mutable"));
        Parent* p = alloc_parent(&b, d);
        object_t* obj = seed_foreign(p, d, seed.rc, seed.immutable);
        size_t owner_rc = seed.rc;
        foreign_ref()->rc = owner_rc;
        used = a.heap.used;

        ponyint_gc_mark(ctx);
        trace(p, d, s);

        // Marking hands over no reference: whatever this actor adds to its own
        // count, it acquires from the owner.
        EXPECT_EQ(seed.rc + acquired_rc(p), obj->rc);
        EXPECT_TRUE(ponyint_object_marked(obj, a.gc.mark));
        EXPECT_EQ(owner_rc + acquired_actor_rc(), foreign_ref()->rc);
        EXPECT_EQ(owner_rc == 0 ? GC_INC_MORE : 0u, acquired_actor_rc());
        EXPECT_EQ(a.gc.mark, foreign_ref()->mark);

        bool upgrade = has_deep(s) && is_val(d) && !seed.immutable;
        EXPECT_EQ(((seed.rc == 0) || upgrade) ? GC_INC_MORE : 0u,
          acquired_rc(p));

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

          EXPECT_EQ(remote_val_traced(d, seed), parent_traced);
        } else {
          EXPECT_EQ(seed.immutable, obj->immutable);
          EXPECT_EQ(1u, parent_traced);
        }

        if(has_deep(s))
        {
          EXPECT_EQ(deep_only_pressure, a.heap.used - used);
        } else {
          EXPECT_EQ(tag_only_pressure, a.heap.used - used);
        }

        if(parent_traced == 1)
        {
          object_t* child = foreign(p->child);
          ASSERT_NE(nullptr, child);
          EXPECT_EQ(a.gc.mark, child->mark);
          EXPECT_EQ(static_cast<size_t>(GC_INC_MORE), acquired_rc(p->child));
          EXPECT_EQ(acquired_rc(p->child), child->rc);
        }

        end_epoch();
      }
    }
  }
}

TEST_F(GcAliasOrder, TagOnlyEpochDoesNotCarryOver)
{
  Parent* p = alloc_parent(&a, ISO);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {TagAlias});
  end_epoch();
  EXPECT_EQ(0u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {DeepAlias, DeepAlias});
  end_epoch();
  EXPECT_EQ(1u, parent_traced);

  pony_gc_send(ctx, &b);
  trace(p, ISO, {TagAlias, DeepAlias});
  end_epoch();
  EXPECT_EQ(1u, parent_traced);

  EXPECT_EQ(3u, local(p)->rc);
  EXPECT_EQ(2u, local(p->child)->rc);
}

TEST_F(GcAliasOrder, SweepKeepsCurrentOpaqueMarks)
{
  Parent* held = alloc_parent(&b, ISO);
  Parent* borrowed = alloc_parent(&b, ISO);
  Parent* expired = alloc_parent(&b, ISO);
  ponyint_gc_mark(ctx);
  trace(expired, ISO, {TagAlias});
  trace(held, ISO, {TagAlias});
  end_epoch();

  ponyint_gc_mark(ctx);
  trace(held, ISO, {TagAlias});
  trace(borrowed, ISO, {TagAlias});
  // Suppress delivery of the release to the fixture's unscheduled actor.
  ponyint_actor_setpendingdestroy(&b);
  ponyint_gc_sweep(ctx, &a.gc);
  EXPECT_NE(nullptr, foreign(held));
  EXPECT_NE(nullptr, foreign(borrowed));
  EXPECT_EQ(nullptr, foreign(expired));
  end_epoch();
}

TEST_F(GcAliasOrder, EpochRolloverClearsVisitMarks)
{
  Parent* local_parent = alloc_parent(&a, ISO);
  Parent* foreign_parent = alloc_parent(&b, ISO);
  pony_gc_send(ctx, &b);
  trace(local_parent, ISO, {TagAlias});
  trace(foreign_parent, ISO, {TagAlias});
  uint32_t first_epoch = a.gc.mark;
  ASSERT_TRUE(ponyint_object_marked(local(local_parent), first_epoch));
  ASSERT_TRUE(ponyint_object_marked(foreign(foreign_parent), first_epoch));
  size_t local_rc = local(local_parent)->rc;
  size_t foreign_rc = foreign(foreign_parent)->rc;
  a.gc.mark = UINT32_MAX - first_epoch + 1;
  end_epoch();
  EXPECT_EQ(first_epoch, a.gc.mark);
  EXPECT_EQ(0u, a.gc.rc_mark);
  EXPECT_EQ(0u, local(local_parent)->mark);
  EXPECT_EQ(0u, foreign_ref()->mark);
  EXPECT_EQ(0u, foreign(foreign_parent)->mark);
  EXPECT_EQ(local_rc, local(local_parent)->rc);
  EXPECT_EQ(foreign_rc, foreign(foreign_parent)->rc);
}

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
    end_epoch();
  }
}

TEST_F(GcAliasOrder, MergedSendRetainsCreditFromBothMessages)
{
  Parent* p = alloc_parent(&b, VAL);
  object_t* obj = seed_foreign(p, VAL, 1, false);
  pony_gc_send(ctx, &b);
  trace(p, VAL, {TagAlias});
  pony_send_next(ctx, &b);
  trace(p, VAL, {TagAlias, DeepAlias});

  ASSERT_NE(nullptr, acquired(p));
  EXPECT_EQ(2u * GC_INC_MORE, acquired_rc(p));
  EXPECT_TRUE(acquired(p)->immutable);
  EXPECT_EQ(&parent_type, acquired(p)->type);
  EXPECT_EQ((2u * GC_INC_MORE) - 1, obj->rc);
  EXPECT_EQ(1u, parent_traced);
  end_epoch();
}

TEST_F(GcAliasOrder, OwnerReceivesImmutableCredit)
{
  for(size_t rc : {0u, 1u, 2u})
  {
    Parent* p = alloc_parent(&b, VAL);
    object_t* local_obj = ponyint_objectmap_getorput(&b.gc.local, p, nullptr);
    local_obj->rc = rc;
    seed_foreign(p, VAL, rc, false)->type = nullptr;
    pony_gc_send(ctx, &b);
    pony_trace(ctx, p);
    if(rc <= 1)
    {
      ASSERT_NE(nullptr, acquired(p));
      EXPECT_EQ(nullptr, acquired(p)->type);
    }
    pony_traceknown(ctx, p, &parent_type, PONY_TRACE_IMMUTABLE);
    pony_trace(ctx, p);
    EXPECT_EQ(&parent_type, foreign(p)->type);
    ponyint_gc_handlestack(ctx);

    size_t index = HASHMAP_UNKNOWN;
    actorref_t* loan = ponyint_actormap_getactor(&ctx->acquire, &b, &index);
    ASSERT_NE(nullptr, loan);
    ponyint_actormap_clearindex(&ctx->acquire, index);
    ponyint_gc_acquire(&b.gc, loan);
    EXPECT_EQ(rc + GC_INC_MORE, local_obj->rc);
    EXPECT_TRUE(local_obj->immutable);
    EXPECT_EQ(&parent_type, local_obj->type);
    end_epoch();
  }
}

TEST_F(GcAliasOrder, UntypedEntryTakesDescriptorFromTypedVisit)
{
  for(bool same_epoch : {true, false})
  {
    Parent* p = alloc_parent(&a, VAL);
    pony_gc_send(ctx, &b);
    pony_trace(ctx, p);
    if(!same_epoch)
    {
      end_epoch();
      pony_gc_send(ctx, &b);
    }
    pony_traceknown(ctx, p, &parent_type, PONY_TRACE_IMMUTABLE);
    ponyint_gc_handlestack(ctx);
    ASSERT_NE(nullptr, local(p));
    EXPECT_EQ(&parent_type, local(p)->type);
    EXPECT_TRUE(local(p)->immutable);
    end_epoch();
  }
}

// The direct tag reaches the parent before the val inside the holder, even
// when the holder is visited first.
TEST_F(GcAliasOrder, ValThroughAnotherObjectAfterTag)
{
  for(pony_actor_t* owner : {&a, &b})
  {
    SCOPED_TRACE((owner == &a) ? "send local" : "mark remote");
    Parent* p = alloc_parent(owner, VAL_MIGHT_REFERENCE_ACTOR);
    Holder* holder = static_cast<Holder*>(ponyint_heap_alloc(owner,
      &owner->heap, sizeof(Holder), TRACK_NO_FINALISERS));
    holder->type = &holder_type;
    holder->parent = p;

    object_t* obj;
    if(owner == &a)
    {
      pony_gc_send(ctx, &b);
    } else {
      seed_foreign(p, VAL_MIGHT_REFERENCE_ACTOR, 1, true);
      ponyint_gc_mark(ctx);
    }

    parent_traced = 0;
    pony_traceknown(ctx, holder, &holder_type, PONY_TRACE_IMMUTABLE);
    pony_traceknown(ctx, p, type_for(VAL_MIGHT_REFERENCE_ACTOR),
      PONY_TRACE_OPAQUE);
    ponyint_gc_handlestack(ctx);

    obj = (owner == &a) ? local(p) : foreign(p);
    ASSERT_NE(nullptr, obj);
    EXPECT_TRUE(obj->immutable);
    EXPECT_EQ(1u, parent_traced);

    object_t* child = (owner == &a) ? local(p->child) : foreign(p->child);
    ASSERT_NE(nullptr, child);
    EXPECT_EQ(a.gc.mark, child->mark);
    end_epoch();
  }
}
