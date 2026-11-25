#include <actor/actor.h>
#include <gc/frames.h>
#include <mem/heap.h>
#include <mem/pagemap.h>
#include <sched/scheduler.h>

#include <cerrno>
#include <gtest/gtest.h>
#ifdef PLATFORM_IS_WINDOWS
#  include <windows.h>
#endif

namespace
{
struct Node
{
  pony_type_t* type;
  Node* child;
};

void trace_node(pony_ctx_t* ctx, void* p)
{
  Node* node = static_cast<Node*>(p);
  if(node->child != nullptr)
    pony_traceunknown(ctx, node->child, PONY_TRACE_MUTABLE);
}

void trace_frame(pony_ctx_t* ctx, void* p)
{
  errno = ERANGE;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(ERROR_INVALID_PARAMETER);
#endif
  pony_trace(ctx, p);
  pony_traceunknown(ctx, p, PONY_TRACE_MUTABLE);
}

class LocalGc : public testing::Test
{
protected:
  pony_ctx_t* ctx;
  pony_actor_t actor{};
  uint32_t first_epoch = 0;
  _pony_type_t actor_type{};
  _pony_type_t node_type{};

  void SetUp() override
  {
    ponyint_register_thread();
    ctx = pony_ctx();
    ctx->current = &actor;
    actor.type = &actor_type;
    node_type.size = sizeof(Node);
    node_type.trace = trace_node;
    ponyint_heap_init(&actor.heap);
    // An actor's GC epoch is never zero.
    ponyint_gc_done(&actor.gc);
    first_epoch = actor.gc.mark;
  }

  // The epoch that follows one completed boundary collection.
  static uint32_t next_epoch(uint32_t mark)
  {
    gc_t gc{};
    gc.mark = mark;
    ponyint_gc_done(&gc);
    return gc.mark;
  }

  void TearDown() override
  {
    ponyint_gc_destroy(&actor.gc);
    ponyint_heap_destroy(&actor.heap);
    ponyint_unregister_thread();
  }

  bool poll(pony_trace_fn trace = nullptr, void* frame = nullptr)
  {
    pony_ctx_t* marking = ponyint_gc_poll(&actor, nullptr);
    if(marking == nullptr)
      return false;
    if(trace != nullptr)
      trace(marking, frame);
    return ponyint_gc_finish(marking);
  }

  Node* node(size_t size = sizeof(Node))
  {
    Node* p = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap, size,
      TRACK_NO_FINALISERS));
    p->type = &node_type;
    p->child = nullptr;
    return p;
  }
};

TEST_F(LocalGc, ReclaimsGarbageAndTracesFrameThroughPromotedObject)
{
  Node* parent = node();
  parent->child = node();
  parent->child->child = parent;
  Node promoted{&node_type, parent};
  size_t live = actor.heap.used;
  Node* dead = node();
  size_t before = actor.heap.used;
  actor.heap.next_gc = before;
  ASSERT_FALSE(poll(trace_frame, &promoted));
  EXPECT_EQ(before, actor.heap.used);

  actor.heap.next_gc--;
  errno = EDOM;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(ERROR_INVALID_HANDLE);
#endif
  bool collected = poll(trace_frame, &promoted);
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error = GetLastError();
#endif
  int saved_errno = errno;
#ifdef PLATFORM_IS_WINDOWS
  EXPECT_EQ(ERROR_INVALID_HANDLE, saved_error);
#endif
  ASSERT_TRUE(collected);
  EXPECT_EQ(EDOM, saved_errno);
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(first_epoch, actor.gc.mark);
  EXPECT_TRUE(ctx->collected_locally);
  EXPECT_EQ(dead, node());
}

TEST_F(LocalGc, TracesReachableForeignMutableObjectsToOwnedChildren)
{
  pony_actor_t other{};
  ponyint_heap_init(&other.heap);
  Node* foreign = static_cast<Node*>(ponyint_heap_alloc(&other, &other.heap,
    sizeof(Node), TRACK_NO_FINALISERS));
  foreign->type = &node_type;
  foreign->child = nullptr;
  pony_gc_recv(ctx);
  pony_traceunknown(ctx, foreign, PONY_TRACE_MUTABLE);
  pony_recv_done(ctx);

  Node* parent = node();
  parent->child = foreign;
  foreign->child = node();
  size_t live = actor.heap.used;
  Node* dead = node();
  actor.heap.next_gc = 0;
  EXPECT_TRUE(poll(trace_frame, parent));
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(dead, node());
  ponyint_heap_destroy(&other.heap);
}

TEST_F(LocalGc, BoundedCallRequiresRoomForTheLargestSmallAllocation)
{
  actor.heap.used = 2 * HEAP_MAX;
  actor.heap.next_gc = 3 * HEAP_MAX;
  EXPECT_TRUE(ponyint_gc_has_small_room(&actor));
  actor.heap.next_gc--;
  EXPECT_FALSE(ponyint_gc_has_small_room(&actor));
  actor.heap.next_gc = 0;
  EXPECT_FALSE(ponyint_gc_has_small_room(&actor));
  EXPECT_FALSE(ponyint_gc_has_small_room(nullptr));
}

TEST_F(LocalGc, HeapChargesCrossTheStrictThresholdAndSweepResetsIt)
{
  for(bool large : {false, true})
  {
    actor.heap.next_gc = large ? HEAP_MAX : HEAP_MIN;
    ponyint_heap_used(&actor.heap, actor.heap.next_gc);
    EXPECT_FALSE(poll());
    node(large ? HEAP_MAX + 1 : HEAP_MIN);
    EXPECT_TRUE(poll());
    EXPECT_FALSE(poll());
    EXPECT_EQ(0u, actor.heap.used);
  }
}

TEST_F(LocalGc, MissingNativeFrameRefusesWithoutSweeping)
{
  actor_type.trace = [](pony_ctx_t*, void*) {
    ADD_FAILURE() << "Actor fields were traced after frame discovery failed";
  };
  Node* root = node();
  actor.heap.next_gc = 0;
  size_t used = actor.heap.used;
  auto* token = reinterpret_cast<gc_frame_t*>(
    reinterpret_cast<uintptr_t>(&actor) | 1);
  errno = EDOM;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(ERROR_INVALID_HANDLE);
#endif
  pony_ctx_t* mark = ponyint_gc_poll(&actor, token);
  ASSERT_NE(nullptr, mark);
  trace_frame(mark, root);
  bool collected = ponyint_gc_finish(mark);
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error = GetLastError();
#endif
  int saved_errno = errno;
#ifdef PLATFORM_IS_WINDOWS
  EXPECT_EQ(ERROR_INVALID_HANDLE, saved_error);
#endif
  EXPECT_EQ(EDOM, saved_errno);
  EXPECT_FALSE(collected);
  EXPECT_EQ(used, actor.heap.used);
  EXPECT_FALSE(ctx->collected_locally);
}

#if defined(PLATFORM_IS_LINUX) && defined(__x86_64__) && !defined(__ILP32__)
extern "C" bool gc_test_caller(pony_actor_t*, unsigned);

uintptr_t expected_address;

extern "C" bool gc_test_poll(pony_actor_t* actor, gc_frame_t* token,
  uintptr_t address)
{
  expected_address = address;
  pony_ctx_t* marking = ponyint_gc_poll(actor, token);
  EXPECT_NE(nullptr, marking);
  return marking != nullptr && ponyint_gc_finish(marking);
}

uintptr_t observed_root;
uintptr_t observed_address;
uintptr_t observed_value;

extern "C" void gc_test_trace(pony_ctx_t*, void* record)
{
  auto* roots = static_cast<uintptr_t*>(record) + 2;
  observed_root = roots[0];
  observed_address = roots[1];
  observed_value = roots[2];
}

// Both return PCs describe the same activation. The callee preserves rbx
// but can overwrite rax.
asm(R"(
  .text
  .globl gc_test_caller
  .type gc_test_caller,@function
gc_test_caller:
  .cfi_startproc
  pushq %rbx
  .cfi_adjust_cfa_offset 8
  .cfi_offset %rbx,-16
  subq $16,%rsp
  .cfi_adjust_cfa_offset 16
  movq $49,8(%rsp)
  leaq 8(%rsp),%rdx
  movl $37,%ebx
  movl %esi,%eax
  leaq 25(%rsp),%rsi
  cmpl $2,%eax
  je 5f
  testl %eax,%eax
  je 1f
  call gc_test_poll
2:
  jmp 4f
1:
  call gc_test_poll
3:
  jmp 4f
5:
  call gc_test_poll
6:
4:
  addq $16,%rsp
  .cfi_adjust_cfa_offset -16
  popq %rbx
  .cfi_adjust_cfa_offset -8
  .cfi_restore %rbx
  ret
  .cfi_endproc
  .size gc_test_caller,.-gc_test_caller

  .pushsection pony_gc_sites,"a",@progbits
  .balign 8
  .macro gc_site pc,reg,offset
    .quad gc_test_caller-.
    .quad gc_test_trace-.
    .long \pc-gc_test_caller,4
    .byte 4,8
    .short 0
    .long 0
    .byte 1,8
    .short \reg
    .long 0
    .byte 2,8
    .short 7
    .long 8
    .byte 3,8
    .short 7
    .long \offset
  .endm
  gc_site 2b,3,8
  gc_site 3b,0,8
  gc_site 6b,3,20
  .purgem gc_site
  .popsection
)");

TEST_F(LocalGc, IndexedUnwindRefusesThenCollectsThroughSupportedSite)
{
  struct Maps
  {
    Maps() { ponyint_gc_maps_init(); }
    ~Maps() { ponyint_gc_maps_destroy(); }
  } maps;
  Node* dead = node();
  actor.heap.next_gc = 0;
  size_t used = actor.heap.used;
  observed_root = 0;
  for(unsigned invalid : {0u, 2u})
  {
    SCOPED_TRACE(invalid);
    EXPECT_FALSE(gc_test_caller(&actor, invalid));
    EXPECT_EQ(0u, observed_root);
    EXPECT_EQ(used, actor.heap.used);
    EXPECT_EQ(0u, actor.heap.next_gc);
    EXPECT_FALSE(ctx->collected_locally);
  }

  ASSERT_TRUE(gc_test_caller(&actor, 1));
  EXPECT_EQ(37u, observed_root);
  EXPECT_EQ(expected_address, observed_address);
  EXPECT_EQ(49u, observed_value);
  EXPECT_EQ(0u, actor.heap.used);
  EXPECT_EQ(dead, node());
}
#endif

TEST_F(LocalGc, ShallowAndReadableNeighboursHaveIndependentChildren)
{
  Node* opaque = node();
  Node* readable = node();
  Node* promoted = node();
  readable->child = node();
  promoted->child = node();
  size_t live = actor.heap.used;
  opaque->child = node();
  for(unsigned i = 6; i < 32; i++)
    node();
  pony_actor_t* owner;
  ASSERT_EQ(ponyint_pagemap_get(opaque, &owner),
    ponyint_pagemap_get(opaque->child, &owner));

  actor.heap.next_gc = 0;
  pony_ctx_t* marking = ponyint_gc_poll(&actor, nullptr);
  ASSERT_NE(nullptr, marking);
  pony_trace(marking, opaque);
  pony_traceunknown(marking, readable, PONY_TRACE_MUTABLE);
  pony_trace(marking, promoted);
  pony_traceunknown(marking, promoted, PONY_TRACE_MUTABLE);
  pony_trace(marking, readable);
  ASSERT_TRUE(ponyint_gc_finish(marking));
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(opaque->child, node());
}

struct Views
{
  pony_type_t* type;
  Node* children[3];
};

template<unsigned Index>
void trace_view(pony_ctx_t* ctx, void* p)
{
  pony_traceunknown(ctx, static_cast<Views*>(p)->children[Index],
    PONY_TRACE_MUTABLE);
}

TEST_F(LocalGc, AlternateDescriptorsAtOneAddressKeepTheirOwnChildren)
{
  _pony_type_t types[3]{};
  types[0].trace = trace_view<0>;
  types[1].trace = trace_view<1>;
  types[2].trace = trace_view<2>;
  for(auto& type : types)
    type.size = sizeof(Views);
  auto* root = static_cast<Views*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Views), TRACK_NO_FINALISERS));
  root->type = &types[0];
  for(Node*& child : root->children)
    child = node();
  size_t live = actor.heap.used;
  Node* dead = node();
  for(bool reverse : {false, true})
  {
    actor.heap.next_gc = 0;
    pony_ctx_t* marking = ponyint_gc_poll(&actor, nullptr);
    ASSERT_NE(nullptr, marking);
    for(unsigned i = 0; i < 3; i++)
      pony_traceknown(marking, root, &types[reverse ? 2 - i : i],
        PONY_TRACE_MUTABLE);
    ASSERT_TRUE(ponyint_gc_finish(marking));
    EXPECT_EQ(live, actor.heap.used);
    EXPECT_EQ(dead, node());
  }
}

size_t finalised;

void final_node(void*)
{
  finalised++;
}

class FinaliserGcRequest : public LocalGc
{
protected:
  void SetUp() override
  {
    LocalGc::SetUp();
    finalised = 0;
    node_type.final = [](void* p) {
      final_node(p);
      pony_triggergc(pony_ctx());
    };
    Node* dead = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
      sizeof(Node), TRACK_ALL_FINALISERS));
    dead->type = &node_type;
    dead->child = nullptr;
    ponyint_gc_final_ready(ctx, dead);
    actor.gc.rc = 1;
    actor.batch_size = PONY_SCHED_BATCH;
    actor.internal_flags = ACTOR_FLAG_CD_CONTACTED;
    ponyint_messageq_init(&actor.q);
  }

  void TearDown() override
  {
    ponyint_messageq_destroy(&actor.q, false);
    LocalGc::TearDown();
  }

  void run_and_check_request()
  {
    ponyint_actor_run(ctx, &actor);
    ASSERT_EQ(1u, finalised);
    ASSERT_EQ(next_epoch(first_epoch), actor.gc.mark);
    ASSERT_EQ(0u, actor.heap.used);
    EXPECT_FALSE(poll());
    node();
    EXPECT_TRUE(poll());
  }
};

TEST_F(FinaliserGcRequest, DuringMessageBoundarySweep)
{
  actor_type.dispatch = [](pony_ctx_t*, pony_actor_t*, pony_msg_t*) {};
  pony_msg_t* msg = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), 0);
  ponyint_actor_messageq_push(&actor.q, msg, msg);
  actor.heap.next_gc = actor.heap.used - 1;
  run_and_check_request();
}

TEST_F(FinaliserGcRequest, DuringBlockedSweep)
{
  run_and_check_request();
}

TEST_F(LocalGc, TriggerCollectsAtNextEligiblePoll)
{
  node();
  ASSERT_FALSE(poll());
  pony_triggergc(ctx);
  ASSERT_TRUE(poll());
  EXPECT_EQ(0u, actor.heap.used);
  node();
  EXPECT_FALSE(poll());
}

bool sweep_after_reference_deletions(pony_actor_t* actor)
{
  ponyint_heap_startgc(&actor->heap
#ifdef USE_RUNTIMESTATS
    , actor
#endif
    );
  return ponyint_heap_endgc(&actor->heap, UINT64_MAX
#ifdef USE_RUNTIMESTATS
    , actor
#endif
    );
}

TEST_F(LocalGc, ReferenceDeletionCanHoldAZeroThreshold)
{
  actor.heap.next_gc = 0;
  EXPECT_TRUE(sweep_after_reference_deletions(&actor));
  EXPECT_EQ(0u, actor.heap.next_gc);
}

TEST_F(FinaliserGcRequest, NewRequestTakesPriorityOverThresholdHold)
{
  actor.heap.next_gc = 1;
  EXPECT_FALSE(sweep_after_reference_deletions(&actor));
  EXPECT_EQ(1u, finalised);
  EXPECT_EQ(0u, actor.heap.next_gc);
}

TEST_F(LocalGc, RecycledFinaliserAllocationsDoNotCountAsForeignMemory)
{
  finalised = 0;
  node_type.trace = nullptr;
  node_type.final = [](void* p) {
    if(++finalised > 1)
      return;
    pony_ctx_t* ctx = pony_ctx();
    for(size_t i = 0; i < (POOL_ALIGN / HEAP_MIN) + 1; i++)
      pony_alloc(ctx, HEAP_MIN);
    pony_alloc(ctx, HEAP_MAX * 2);
    Node* pending = static_cast<Node*>(ponyint_heap_alloc(ctx->current,
      &ctx->current->heap, HEAP_MIN * 2, TRACK_ALL_FINALISERS));
    pending->type = static_cast<Node*>(p)->type;
    ponyint_gc_final_ready(ctx, pending);
  };
  volatile Node* first = static_cast<Node*>(ponyint_heap_alloc(&actor,
    &actor.heap, sizeof(Node), TRACK_ALL_FINALISERS));
  first->type = &node_type;
#ifdef USE_RUNTIMESTATS
  size_t expected_used = actor.actorstats.heap_mem_used + HEAP_MIN;
  size_t expected_allocated = actor.actorstats.heap_mem_allocated;
#endif
  sweep_after_reference_deletions(&actor);
  EXPECT_EQ(1u, finalised);
  EXPECT_EQ(HEAP_MIN * 2u, actor.heap.used);
#ifdef USE_RUNTIMESTATS
  EXPECT_EQ(1u, actor.actorstats.heap_num_allocated);
  EXPECT_EQ(expected_used, actor.actorstats.heap_mem_used);
  EXPECT_EQ(expected_allocated, actor.actorstats.heap_mem_allocated);
  size_t previous_frees = actor.actorstats.heap_free_counter;
#endif

  pony_actor_t peer{};
  ponyint_gc_createactor(&actor, &peer);
  node();
  pony_triggergc(ctx);
  ASSERT_TRUE(poll());
  EXPECT_EQ(GC_ACTOR_HEAP_EQUIV + HEAP_MIN * 2u, actor.heap.used);
#ifdef USE_RUNTIMESTATS
  EXPECT_EQ(previous_frees + 1, actor.actorstats.heap_free_counter);
#endif
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(2u, finalised);
}

TEST_F(LocalGc, RetainsConstructedFinalisableObjectsAndTheirChildren)
{
  finalised = 0;
  node_type.final = final_node;
  Node* small = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  Node* large = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    HEAP_MAX * 2, TRACK_ALL_FINALISERS));
  small->type = large->type = &node_type;
  small->child = node();
  large->child = node();
  size_t live = actor.heap.used;
  Node* dead = node();
  ponyint_gc_final_ready(ctx, small);
  actor.heap.next_gc = 0;
  size_t before = actor.heap.used;
  EXPECT_FALSE(poll());
  EXPECT_EQ(before, actor.heap.used);
  EXPECT_EQ(GC_WAIT_FOR_BOUNDARY, actor.gc_wait_finaliser);
  // Reporting the large object's constructor leaves the wait in place. An
  // explicit request clears it.
  ponyint_gc_final_ready(ctx, large);
  EXPECT_EQ(GC_WAIT_FOR_BOUNDARY, actor.gc_wait_finaliser);
  pony_triggergc(ctx);
  for(size_t i = 0; i < 2; i++)
  {
    actor.heap.next_gc = 0;
    ASSERT_TRUE(poll());
    EXPECT_EQ(0u, finalised);
    EXPECT_EQ(live, actor.heap.used);
    EXPECT_EQ(dead, node());
  }
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(2u, finalised);
}

// The sweep records traceability separately for small and large chunks.
class FinaliserBoundary : public LocalGc,
  public testing::WithParamInterface<size_t>
{
protected:
  Node* finalisable()
  {
    finalised = 0;
    node_type.final = final_node;
    Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor,
      &actor.heap, GetParam(), TRACK_ALL_FINALISERS));
    resource->type = &node_type;
    resource->child = node();
    return resource;
  }

  chunk_t* chunk_of(void* p)
  {
    return ponyint_pagemap_get_chunk(p);
  }

  // This actor sent the object to another actor, and sending it counted its
  // child too, so a boundary keeps both without tracing either.
  void mark_held_elsewhere(Node* resource)
  {
    ponyint_heap_mark_shallow(chunk_of(resource), resource);
    ponyint_heap_mark_shallow(chunk_of(resource->child), resource->child);
  }
};

INSTANTIATE_TEST_SUITE_P(Sizes, FinaliserBoundary,
  testing::Values(sizeof(Node), size_t(HEAP_MAX * 2)));

TEST_P(FinaliserBoundary, MarkKeepsOnlyReadableFinalisersEligible)
{
  Node* resource = finalisable();
  ponyint_gc_final_ready(ctx, resource);
  for(bool readable : {true, false, true})
  {
    // Clear the wait as a boundary collection does.
    actor.gc_wait_finaliser = GC_WAIT_NONE;
    ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
    if(readable)
    {
      ponyint_heap_mark(chunk_of(resource), resource);
      ponyint_heap_mark(chunk_of(resource->child), resource->child);
    } else {
      mark_held_elsewhere(resource);
    }
    ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
    actor.heap.next_gc = 0;
    EXPECT_EQ(readable, poll());
    EXPECT_EQ(readable ? GC_WAIT_NONE : GC_WAIT_FOR_BOUNDARY,
      actor.gc_wait_finaliser);
    EXPECT_EQ(0u, finalised);
  }
}

TEST_P(FinaliserBoundary, DeepMarkMakesAnUnreportedFinaliserEligible)
{
  // Native code can allocate a finalisable object without reporting a
  // constructor's completion.
  Node* resource = finalisable();
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  ponyint_heap_mark(chunk_of(resource), resource);
  ponyint_heap_mark(chunk_of(resource->child), resource->child);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  actor.heap.next_gc = 0;
  EXPECT_TRUE(poll());
  EXPECT_EQ(GC_WAIT_NONE, actor.gc_wait_finaliser);
  EXPECT_EQ(0u, finalised);
}

TEST_P(FinaliserBoundary, UntraceableFinaliserWithNothingToTraceIsKept)
{
  // A type with no trace function has no fields to trace, so keeping its
  // object doesn't need them to be defined.
  static _pony_type_t plain{};
  plain.size = GetParam();
  plain.final = final_node;
  finalised = 0;
  void* resource = ponyint_heap_alloc(&actor, &actor.heap, GetParam(),
    TRACK_ALL_FINALISERS);
  *static_cast<pony_type_t**>(resource) = &plain;
  ponyint_gc_final_ready(ctx, resource);
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  ponyint_heap_mark_shallow(chunk_of(resource), resource);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  for(int i = 0; i < 2; i++)
  {
    SCOPED_TRACE(i);
    actor.heap.next_gc = 0;
    EXPECT_TRUE(poll());
    EXPECT_EQ(GC_WAIT_NONE, actor.gc_wait_finaliser);
  }
  EXPECT_EQ(0u, finalised);
  // Keeping it didn't claim a readable reference, so it stays untraceable.
  bool traceable = true;
  ponyint_heap_visit_finalisers(&actor.heap,
    [](void* arg, void*, bool t) { *static_cast<bool*>(arg) = t; return true; },
    &traceable, nullptr);
  EXPECT_FALSE(traceable);
  EXPECT_NE(resource, ponyint_heap_alloc(&actor, &actor.heap, GetParam(),
    TRACK_NO_FINALISERS));
}

TEST_P(FinaliserBoundary, InteriorMarkLeavesAFinaliserUntraceable)
{
  // An interior pointer, such as one to an embedded field, doesn't show
  // that the object's other fields are defined.
  Node* resource = finalisable();
  ponyint_gc_final_ready(ctx, resource);
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  ponyint_heap_mark(chunk_of(resource), &resource->child);
  ponyint_heap_mark_shallow(chunk_of(resource->child), resource->child);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  actor.heap.next_gc = 0;
  EXPECT_FALSE(poll());
  EXPECT_EQ(GC_WAIT_FOR_BOUNDARY, actor.gc_wait_finaliser);
  EXPECT_EQ(0u, finalised);
}

TEST_P(FinaliserBoundary, ReadableRootMakesAnUntraceableFinaliserEligible)
{
  // An object sent away as iso and received back, for example, is reached
  // only shallowly at the boundary but readably while the behavior runs.
  Node* resource = finalisable();
  ponyint_gc_final_ready(ctx, resource);
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  mark_held_elsewhere(resource);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  auto readable_root = [](pony_ctx_t* marking, void* p) {
    pony_traceknown(marking, p, *(pony_type_t**)p, PONY_TRACE_MUTABLE);
  };
  Node* child = resource->child;
  actor.heap.next_gc = 0;
  EXPECT_TRUE(poll(readable_root, resource));
  EXPECT_EQ(GC_WAIT_NONE, actor.gc_wait_finaliser);
  EXPECT_NE(child, node());
  actor.heap.next_gc = 0;
  EXPECT_TRUE(poll());
  EXPECT_EQ(0u, finalised);
  EXPECT_NE(child, node());
}

TEST_P(FinaliserBoundary, OtherRootsLeaveAnUntraceableFinaliserWaiting)
{
  // A tag, a reference to an embedded field, or a view under another
  // descriptor doesn't show that the object's fields are defined.
  pony_trace_fn roots[] = {
    [](pony_ctx_t* marking, void* p) { pony_trace(marking, p); },
    [](pony_ctx_t* marking, void* p) {
      static _pony_type_t field{};
      field.size = sizeof(void*);
      Node* node = static_cast<Node*>(p);
      pony_traceknown(marking, &node->child, &field, PONY_TRACE_MUTABLE);
    },
    [](pony_ctx_t* marking, void* p) {
      static _pony_type_t other{};
      other.size = sizeof(Node);
      pony_traceknown(marking, p, &other, PONY_TRACE_MUTABLE);
    },
  };
  Node* resource = finalisable();
  ponyint_gc_final_ready(ctx, resource);
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  mark_held_elsewhere(resource);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  for(size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++)
  {
    SCOPED_TRACE(i);
    pony_trace_fn root = roots[i];
    actor.gc_wait_finaliser = GC_WAIT_NONE;
    actor.heap.next_gc = 0;
    EXPECT_FALSE(poll(root, resource));
    EXPECT_EQ(GC_WAIT_FOR_BOUNDARY, actor.gc_wait_finaliser);
  }
  EXPECT_EQ(0u, finalised);
}

TEST_P(FinaliserBoundary, UnrelatedConstructorDoesNotClearFinaliserWait)
{
  Node* resource = finalisable();
  ponyint_gc_final_ready(ctx, resource);
  ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  mark_held_elsewhere(resource);
  ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
    , &actor
#endif
    );
  actor.heap.next_gc = 0;
  for(bool requested : {false, true})
  {
    if(requested)
    {
      pony_triggergc(ctx);
      EXPECT_EQ(GC_WAIT_NONE, actor.gc_wait_finaliser);
    } else {
      // Clear the wait as a boundary collection does.
      actor.gc_wait_finaliser = GC_WAIT_NONE;
    }
    EXPECT_FALSE(poll());
    EXPECT_EQ(GC_WAIT_FOR_BOUNDARY, actor.gc_wait_finaliser);

    Node* unrelated = static_cast<Node*>(ponyint_heap_alloc(&actor,
      &actor.heap, sizeof(Node), TRACK_ALL_FINALISERS));
    unrelated->type = &node_type;
    unrelated->child = nullptr;
    ponyint_gc_final_ready(ctx, unrelated);
    pony_ctx_t* marking = ponyint_gc_poll(&actor, nullptr);
    EXPECT_EQ(nullptr, marking);
    if(marking != nullptr)
      EXPECT_FALSE(ponyint_gc_finish(marking));
  }
  EXPECT_EQ(0u, finalised);
}

TEST_F(LocalGc, SharedFinaliserWithFieldsRequiresAReadableRoot)
{
  pony_actor_t peer{};
  finalised = 0;
  node_type.final = final_node;
  Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  resource->type = &node_type;
  resource->child = node();
  ponyint_gc_final_ready(ctx, resource);
  pony_gc_send(ctx, &peer);
  pony_trace(ctx, resource);
  pony_send_done(ctx);

  size_t live = actor.heap.used;
  Node* dead = node();
  size_t before = actor.heap.used;
  actor.heap.next_gc = 0;
  node_type.trace = [](pony_ctx_t*, void*) {
    ADD_FAILURE() << "The owner traced a finaliser with only an opaque root";
  };
  EXPECT_FALSE(poll());
  EXPECT_EQ(before, actor.heap.used);
  EXPECT_EQ(0u, finalised);

  node_type.trace = trace_node;
  Node* unrelated = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  unrelated->type = &node_type;
  unrelated->child = nullptr;
  ponyint_gc_final_ready(ctx, unrelated);
  EXPECT_EQ(nullptr, ponyint_gc_poll(&actor, nullptr));
  pony_triggergc(ctx);
  resource->child->child = unrelated;
  ASSERT_TRUE(poll(trace_frame, resource));
  EXPECT_EQ(live + HEAP_MIN, actor.heap.used);
  EXPECT_EQ(dead, node());
  EXPECT_EQ(0u, finalised);
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(2u, finalised);
}

TEST_F(LocalGc, BoundaryCollectsAfterALocalCollection)
{
  node();
  actor_type.dispatch = [](pony_ctx_t*, pony_actor_t* self, pony_msg_t*) {
    pony_ctx_t* marking = ponyint_gc_poll(self, nullptr);
    if(marking != nullptr)
      ponyint_gc_finish(marking);
  };
  actor.heap.next_gc = 0;
  actor.batch_size = PONY_SCHED_BATCH;
  ponyint_messageq_init(&actor.q);
  pony_msg_t* msg = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), 0);
  ponyint_actor_messageq_push(&actor.q, msg, msg);
  ponyint_actor_run(ctx, &actor);
  ponyint_messageq_destroy(&actor.q, false);
  EXPECT_EQ(0u, actor.heap.used);
  // The local collection left the heap under its threshold, so only the
  // request makes the boundary collect.
  EXPECT_EQ(next_epoch(first_epoch), actor.gc.mark);
  EXPECT_FALSE(ctx->collected_locally);
}

TEST_F(LocalGc, BoundaryClearsFinaliserWait)
{
  pony_actor_t peer{};
  Node* exported = node();
  pony_gc_send(ctx, &peer);
  pony_trace(ctx, exported);
  pony_send_done(ctx);
  finalised = 0;
  node_type.final = final_node;
  Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  resource->type = &node_type;
  resource->child = nullptr;
  ponyint_gc_final_ready(ctx, resource);

  pony_triggergc(ctx);
  EXPECT_FALSE(poll());
  EXPECT_TRUE(actor.gc_wait_finaliser);
  actor_type.dispatch = [](pony_ctx_t*, pony_actor_t*, pony_msg_t*) {};
  actor.batch_size = PONY_SCHED_BATCH;
  ponyint_messageq_init(&actor.q);
  pony_msg_t* msg = pony_alloc_msg(POOL_INDEX(sizeof(pony_msg_t)), 0);
  ponyint_actor_messageq_push(&actor.q, msg, msg);
  ponyint_actor_run(ctx, &actor);
  ponyint_messageq_destroy(&actor.q, false);
  EXPECT_EQ(1u, finalised);
  EXPECT_FALSE(actor.gc_wait_finaliser);
}

TEST_F(LocalGc, ReceivedObjectCanDeferAnUnreachableFinaliser)
{
  pony_actor_t owner{};
  ponyint_heap_init(&owner.heap);
  Node* foreign = static_cast<Node*>(ponyint_heap_alloc(&owner, &owner.heap,
    sizeof(Node), TRACK_NO_FINALISERS));
  foreign->type = &node_type;
  foreign->child = nullptr;
  pony_gc_recv(ctx);
  pony_traceunknown(ctx, foreign, PONY_TRACE_MUTABLE);
  pony_recv_done(ctx);

  finalised = 0;
  node_type.final = final_node;
  Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  resource->type = &node_type;
  resource->child = foreign;
  ponyint_gc_final_ready(ctx, resource);
  node_type.trace = [](pony_ctx_t*, void*) {
    ADD_FAILURE() << "Unreachable finaliser fields were traced";
  };

  pony_triggergc(ctx);
  EXPECT_FALSE(poll());
  EXPECT_EQ(nullptr, ponyint_gc_poll(&actor, nullptr));
  EXPECT_EQ(0u, finalised);
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(1u, finalised);
  ponyint_heap_destroy(&owner.heap);
}

TEST_F(LocalGc, RetainsTraceFreeFinaliserWhileSharing)
{
  pony_actor_t peer{};
  Node* exported = node();
  pony_gc_send(ctx, &peer);
  pony_trace(ctx, exported);
  pony_send_done(ctx);
  _pony_type_t resource_type{};
  resource_type.size = sizeof(Node);
  resource_type.final = final_node;
  finalised = 0;
  Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  resource->type = &resource_type;
  ponyint_gc_final_ready(ctx, resource);

  size_t live = actor.heap.used;
  Node* dead = node();
  actor.heap.next_gc = 0;
  ASSERT_TRUE(poll());
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(dead, node());
  EXPECT_EQ(0u, finalised);
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(1u, finalised);
}

TEST_F(LocalGc, NativeStoreRecognisesManagedDestinations)
{
  Node* owned = node();
  ponyint_gc_native_store(&owned->child);
  EXPECT_FALSE(actor.gc_native_alias);

  pony_actor_t other{};
  ponyint_heap_init(&other.heap);
  Node* foreign = static_cast<Node*>(ponyint_heap_alloc(&other, &other.heap,
    sizeof(Node), TRACK_NO_FINALISERS));
  foreign->type = &node_type;
  foreign->child = nullptr;
  pony_gc_recv(ctx);
  pony_traceunknown(ctx, foreign, PONY_TRACE_MUTABLE);
  pony_recv_done(ctx);
  ponyint_gc_native_store(&foreign->child);
  EXPECT_FALSE(actor.gc_native_alias);
  ponyint_heap_destroy(&other.heap);

  Node external{};
  ponyint_gc_native_store(&external.child);
  EXPECT_TRUE(actor.gc_native_alias);
}

TEST_F(LocalGc, KeepsHeapChargeForRetainedPeerActors)
{
  pony_actor_t peer{};
  ponyint_gc_createactor(&actor, &peer);
  size_t charge = actor.heap.used;
  ASSERT_GT(charge, 0u);
  size_t index = HASHMAP_UNKNOWN;
  actorref_t* ref = ponyint_actormap_getactor(&actor.gc.foreign, &peer, &index);
  ASSERT_NE(nullptr, ref);
  size_t rc = ref->rc;
  node();
  actor.heap.next_gc = 0;
  ASSERT_TRUE(poll());
  EXPECT_EQ(charge, actor.heap.used);
  EXPECT_EQ(rc, ref->rc);
}

TEST_F(LocalGc, MutableExportsAreRetainedWithoutTracingTheirFields)
{
  pony_actor_t peer{};
  Node* root = node();
  root->child = node();
  size_t live = actor.heap.used;
  pony_gc_send(ctx, &peer);
  pony_traceunknown(ctx, root, PONY_TRACE_MUTABLE);
  pony_send_done(ctx);
  node_type.trace = [](pony_ctx_t*, void*) {
    ADD_FAILURE() << "The owner traced a mutable export";
  };

  Node* dead = node();
  actor.heap.next_gc = 0;
  ASSERT_TRUE(poll());
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(dead, node());
}

TEST_F(LocalGc, ImmutableExportRetainsItsUnexportedChild)
{
  pony_actor_t peer{};
  Node* root = node();
  root->child = node();
  size_t live = actor.heap.used;
  pony_gc_send(ctx, &peer);
  pony_traceunknown(ctx, root, PONY_TRACE_IMMUTABLE);
  pony_send_done(ctx);
  ASSERT_EQ(1u, ponyint_objectmap_size(&actor.gc.local));

  Node* dead = node();
  actor.heap.next_gc = 0;
  ASSERT_TRUE(poll());
  EXPECT_EQ(live, actor.heap.used);
  EXPECT_EQ(dead, node());
}

TEST_F(LocalGc, ActiveReceiveDefersPruningUntilTraceCompletes)
{
  Node* root = node();
  pony_gc_send(ctx, &actor);
  pony_traceunknown(ctx, root, PONY_TRACE_MUTABLE);
  pony_send_done(ctx);
  actor.heap.next_gc = 0;

  pony_gc_recv(ctx);
  pony_traceunknown(ctx, root, PONY_TRACE_MUTABLE);
  EXPECT_FALSE(poll());
  EXPECT_EQ(1u, ponyint_objectmap_size(&actor.gc.local));
  pony_recv_done(ctx);
  uint32_t mark = actor.gc.mark;
  ASSERT_TRUE(poll());
  EXPECT_EQ(mark, actor.gc.mark);
  EXPECT_EQ(0u, ponyint_objectmap_size(&actor.gc.local));
  EXPECT_EQ(0u, actor.heap.used);
}
}
