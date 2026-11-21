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

bool poll(pony_ctx_t* ctx, pony_actor_t* actor, pony_trace_fn trace, void* frame)
{
  pony_ctx_t* marking = ponyint_gc_begin(ctx, actor, nullptr);
  if(marking == nullptr)
    return false;
  if(trace != nullptr)
    trace(marking, frame);
  return ponyint_gc_finish(marking);
}

class LocalGc : public testing::Test
{
protected:
  pony_ctx_t ctx{};
  pony_actor_t actor{};
  _pony_type_t actor_type{};
  _pony_type_t node_type{};

  void SetUp() override
  {
    ctx.current = &actor;
    actor.type = &actor_type;
    node_type.size = sizeof(Node);
    node_type.trace = trace_node;
    ponyint_heap_init(&actor.heap);
  }

  void TearDown() override
  {
    ponyint_gc_destroy(&actor.gc);
    ponyint_heap_destroy(&actor.heap);
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
  ASSERT_FALSE(poll(&ctx, &actor, trace_frame, &promoted));
  EXPECT_EQ(before, actor.heap.used);

  actor.heap.next_gc--;
  errno = EDOM;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(ERROR_INVALID_HANDLE);
#endif
  bool collected = poll(&ctx, &actor, trace_frame, &promoted);
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
  EXPECT_EQ(0u, actor.gc.mark);
  EXPECT_NE(0, actor.internal_flags & ACTOR_FLAG_BOUNDARY_GC);
  EXPECT_EQ(dead, node());
}

TEST_F(LocalGc, ForeignDescendantRefusesBeforeSweep)
{
  pony_actor_t other{};
  ponyint_heap_init(&other.heap);
  Node* parent = node();
  parent->child = static_cast<Node*>(ponyint_heap_alloc(&other, &other.heap,
    sizeof(Node), TRACK_NO_FINALISERS));
  parent->child->type = &node_type;
  parent->child->child = nullptr;
  node();
  size_t before = actor.heap.used;
  actor.heap.next_gc = 0;
  errno = EDOM;
#ifdef PLATFORM_IS_WINDOWS
  SetLastError(ERROR_INVALID_HANDLE);
#endif
  bool collected = poll(&ctx, &actor, trace_frame, parent);
#ifdef PLATFORM_IS_WINDOWS
  DWORD saved_error = GetLastError();
#endif
  int saved_errno = errno;
#ifdef PLATFORM_IS_WINDOWS
  EXPECT_EQ(ERROR_INVALID_HANDLE, saved_error);
#endif
  EXPECT_FALSE(collected);
  EXPECT_EQ(EDOM, saved_errno);
  EXPECT_EQ(before, actor.heap.used);
  EXPECT_EQ(0u, actor.heap.next_gc);
  EXPECT_EQ(0, actor.internal_flags & ACTOR_FLAG_BOUNDARY_GC);
  parent->child = nullptr;
  ponyint_heap_destroy(&other.heap);
  EXPECT_TRUE(poll(&ctx, &actor, trace_frame, parent));
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
    ASSERT_FALSE(actor.heap.gc_due);
    node(large ? HEAP_MAX + 1 : HEAP_MIN);
    ASSERT_TRUE(actor.heap.gc_due);
    ASSERT_TRUE(poll(&ctx, &actor, nullptr, nullptr));
    ASSERT_FALSE(actor.heap.gc_due);
    EXPECT_EQ(0u, actor.heap.used);
  }
}

TEST_F(LocalGc, MissingNativeFrameRefusesWithoutSweeping)
{
  node();
  actor.heap.next_gc = 0;
  size_t used = actor.heap.used;
  auto* token = reinterpret_cast<gc_frame_t*>(
    reinterpret_cast<uintptr_t>(&actor) | 1);
  pony_ctx_t* mark = ponyint_gc_begin(&ctx, &actor, token);
  ASSERT_NE(nullptr, mark);
  EXPECT_FALSE(ponyint_gc_finish(mark));
  EXPECT_EQ(used, actor.heap.used);
  EXPECT_EQ(0, actor.internal_flags & ACTOR_FLAG_BOUNDARY_GC);
}

#if defined(PLATFORM_IS_LINUX) && defined(__x86_64__) && !defined(__ILP32__)
extern "C" bool gc_test_caller(pony_ctx_t*, pony_actor_t*, unsigned);

uintptr_t expected_address;

extern "C" bool gc_test_poll(pony_ctx_t* ctx, pony_actor_t* actor,
  gc_frame_t* token, uintptr_t address)
{
  expected_address = address;
  pony_ctx_t* marking = ponyint_gc_begin(ctx, actor, token);
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
  leaq 8(%rsp),%rcx
  movl $37,%ebx
  movl %edx,%eax
  leaq 25(%rsp),%rdx
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
    EXPECT_FALSE(gc_test_caller(&ctx, &actor, invalid));
    EXPECT_EQ(0u, observed_root);
    EXPECT_EQ(used, actor.heap.used);
    EXPECT_EQ(0u, actor.heap.next_gc);
    EXPECT_EQ(0, actor.internal_flags & ACTOR_FLAG_BOUNDARY_GC);
  }

  ASSERT_TRUE(gc_test_caller(&ctx, &actor, 1));
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
  pony_ctx_t* marking = ponyint_gc_begin(&ctx, &actor, nullptr);
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
    pony_ctx_t* marking = ponyint_gc_begin(&ctx, &actor, nullptr);
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
  ponyint_gc_final_ready(&ctx, small);
  actor.heap.next_gc = 0;
  size_t before = actor.heap.used;
  EXPECT_FALSE(poll(&ctx, &actor, nullptr, nullptr));
  EXPECT_EQ(before, actor.heap.used);
  EXPECT_TRUE(actor.gc_wait_finaliser);
  ponyint_gc_final_ready(&ctx, large);
  EXPECT_FALSE(actor.gc_wait_finaliser);
  for(size_t i = 0; i < 2; i++)
  {
    actor.heap.next_gc = 0;
    ASSERT_TRUE(poll(&ctx, &actor, nullptr, nullptr));
    EXPECT_EQ(0u, finalised);
    EXPECT_EQ(live, actor.heap.used);
    EXPECT_EQ(dead, node());
  }
  ponyint_heap_final(&actor.heap);
  EXPECT_EQ(2u, finalised);
}

TEST_F(LocalGc, BoundaryMarkKeepsOnlyReadableFinalisersEligible)
{
  finalised = 0;
  node_type.final = final_node;
  Node* resource = static_cast<Node*>(ponyint_heap_alloc(&actor, &actor.heap,
    sizeof(Node), TRACK_ALL_FINALISERS));
  resource->type = &node_type;
  resource->child = node();
  ponyint_gc_final_ready(&ctx, resource);
  pony_actor_t* owner;
  chunk_t* chunk = ponyint_pagemap_get(resource, &owner);
  for(bool readable : {true, false})
  {
    ponyint_heap_startgc(&actor.heap
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
    if(readable)
    {
      ponyint_heap_mark(chunk, resource);
      ponyint_heap_mark(ponyint_pagemap_get(resource->child, &owner),
        resource->child);
    } else {
      ponyint_heap_mark_shallow(chunk, resource);
    }
    ponyint_heap_endgc(&actor.heap, 0
#ifdef USE_RUNTIMESTATS
      , &actor
#endif
      );
    actor.heap.next_gc = 0;
    EXPECT_EQ(readable, poll(&ctx, &actor, nullptr, nullptr));
    EXPECT_EQ(!readable, actor.gc_wait_finaliser);
    EXPECT_EQ(0u, finalised);
  }
}

TEST_F(LocalGc, NativeStoreChecksWhoOwnsTheDestination)
{
  ponyint_register_thread();
  pony_ctx_t* current = pony_ctx();
  pony_actor_t* previous = current->current;
  current->current = &actor;
  Node* owned = node();
  ponyint_gc_native_store(&owned->child);
  EXPECT_FALSE(actor.gc_native_alias);
  Node external{};
  ponyint_gc_native_store(&external.child);
  EXPECT_TRUE(actor.gc_native_alias);
  current->current = previous;
  ponyint_unregister_thread();
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
  ASSERT_TRUE(poll(&ctx, &actor, nullptr, nullptr));
  EXPECT_EQ(charge, actor.heap.used);
  EXPECT_EQ(rc, ref->rc);
}

TEST_F(LocalGc, LocalInterestAndActiveReceive)
{
  Node* root = node();
  size_t live = actor.heap.used;
  node();
  actor.heap.next_gc = 0;
  object_t* entry = ponyint_objectmap_getorput(&actor.gc.local, root,
    &node_type, actor.gc.mark);
  entry->rc = 1;
  EXPECT_FALSE(poll(&ctx, &actor, nullptr, nullptr));
  entry->rc = 0;
  entry->self_send_pins = 1;
  EXPECT_FALSE(poll(&ctx, &actor, nullptr, nullptr));
  entry->self_send_pins = 0;
  pony_gc_recv(&ctx);
  EXPECT_FALSE(poll(&ctx, &actor, nullptr, nullptr));
  pony_recv_done(&ctx);
  EXPECT_TRUE(poll(&ctx, &actor, nullptr, nullptr));
  EXPECT_EQ(live, actor.heap.used);
}
}
