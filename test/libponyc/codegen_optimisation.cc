#include <gtest/gtest.h>
#include <platform.h>

#include <vector>

#include <codegen/genopt.h>
#include <heap_to_stack.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/CFG.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CGSCCPassManager.h>

#include "llvm_config_end.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenOptimisationTest : public PassTest
{
protected:
  void check_loop_polls()
  {
    const char* src =
      "actor Main\n"
      "  new create(env: Env) => run(42)\n"
      "  be run(n: U64) =>\n"
      "    var i: U64 = 0\n"
      "    while i < n do\n"
      "      sink(Array[U64])\n"
      "      i = i + 1\n"
      "    end\n"
      "  fun sink(a: Array[U64]) => None";
    TEST_COMPILE(src);
    for(auto& fn : *llvm::unwrap(compile->module))
      if(fn.getName().contains("Main_tag_run"))
        for(auto& block : fn)
          if(block.getName().starts_with("while_body"))
            for(auto& inst : block)
              if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
                if(call->getCalledFunction() != nullptr &&
                  call->getCalledFunction()->getName() == "ponyint_gc_poll")
                  return;
    FAIL() << "expected a collection point in Main.run's allocating loop";
  }

  void optimise()
  {
    auto module = llvm::unwrap(compile->module);

    for(auto& function : *module)
    {
      if(!function.isDeclaration())
        function.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }

    ASSERT_TRUE(pony_specific_opt(compile));
  }

  // Run pony_specific_opt followed by HeapToStack via its own CGSCC pipeline.
  // In a real build HeapToStack runs during LTO; this exercises it on the
  // single in-memory module the test fixture produces.
  void optimise_with_heap_to_stack()
  {
    optimise();

    auto module = llvm::unwrap(compile->module);

    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::CGSCCPassManager CGPM;
    CGPM.addPass(pony::HeapToStack());

    llvm::ModulePassManager MPM;
    MPM.addPass(
      llvm::createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));
    MPM.run(*module, MAM);
  }

  // Defined functions whose names contain part and end in suffix. Collecting
  // copies end in ".gc"; collecting dispatchers end in ".gc.dispatch".
  std::vector<llvm::Function*> functions_named(const char* part,
    const char* suffix)
  {
    std::vector<llvm::Function*> found;
    for(auto& fn : *llvm::unwrap(compile->module))
    {
      if(!fn.isDeclaration() && fn.getName().contains(part) &&
        fn.getName().ends_with(suffix))
        found.push_back(&fn);
    }
    return found;
  }

  bool has_copy(const char* method)
  {
    return !functions_named(method, ".gc").empty();
  }

  static bool is_poll(llvm::Instruction& inst)
  {
    auto* call = llvm::dyn_cast<llvm::CallBase>(&inst);
    return (call != nullptr) && (call->getCalledFunction() != nullptr) &&
      (call->getCalledFunction()->getName() == "ponyint_gc_poll");
  }

  static bool calls_poll(llvm::BasicBlock& block)
  {
    for(auto& inst : block)
      if(is_poll(inst))
        return true;
    return false;
  }

  size_t polls_in(const char* part, const char* suffix)
  {
    size_t count = 0;
    for(auto* fn : functions_named(part, suffix))
      for(auto& block : *fn)
        for(auto& inst : block)
          if(is_poll(inst))
            count++;
    return count;
  }

  // True if the method has a collecting copy and every copy polls in its
  // entry block.
  bool polls_at_entry(const char* method)
  {
    auto copies = functions_named(method, ".gc");
    if(copies.empty())
      return false;
    for(auto* fn : copies)
      if(!calls_poll(fn->getEntryBlock()))
        return false;
    return true;
  }

  // True if the method has a collecting copy and, in every copy, every block
  // that returns has a predecessor block that polls.
  bool polls_before_returns(const char* method)
  {
    auto copies = functions_named(method, ".gc");
    if(copies.empty())
      return false;
    for(auto* fn : copies)
      for(auto& block : *fn)
        if(llvm::isa<llvm::ReturnInst>(block.getTerminator()))
        {
          bool polled = false;
          for(auto* pred : llvm::predecessors(&block))
            polled |= calls_poll(*pred);
          if(!polled)
            return false;
        }
    return true;
  }

  // True if the method has exactly one collecting copy, it calls itself, and
  // neither the rest of that call's block nor the blocks it branches to poll.
  bool no_poll_after_self_call(const char* method)
  {
    auto copies = functions_named(method, ".gc");
    if(copies.size() != 1)
      return false;
    llvm::Function* fn = copies[0];
    bool found = false;
    for(auto& block : *fn)
    {
      bool after_call = false;
      for(auto& inst : block)
      {
        if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
          if(call->getCalledFunction() == fn)
            after_call = found = true;
        if(after_call && is_poll(inst))
          return false;
      }
      if(after_call)
        for(auto* succ : llvm::successors(&block))
          for(auto& inst : *succ)
            if(is_poll(inst))
              return false;
    }
    return found;
  }

  // Calls to a root-tracing helper, in the method's only collecting copy, that
  // pass a value loaded from a returned-value slot.
  size_t rooted_result_polls(const char* method)
  {
    auto copies = functions_named(method, ".gc");
    if(copies.size() != 1)
      return 0;
    size_t rooted = 0;
    for(auto& block : *copies[0])
      for(auto& inst : block)
        if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
          if(call->getCalledFunction() != nullptr &&
            call->getCalledFunction()->getName().starts_with("gc_finish_roots"))
            for(auto& arg : call->args())
              if(auto* load = llvm::dyn_cast<llvm::LoadInst>(arg.get()))
                if(load->getPointerOperand()->getName().starts_with(
                  "gc_result"))
                  rooted++;
    return rooted;
  }

  // Count direct calls to the named function across the whole module.
  size_t count_calls_to(const char* name)
  {
    auto module = llvm::unwrap(compile->module);

    size_t count = 0;
    for(auto& function : *module)
    {
      for(auto& block : function)
      {
        for(auto& inst : block)
        {
          auto call = llvm::dyn_cast<llvm::CallBase>(&inst);
          if(call == nullptr)
            continue;

          auto callee = call->getCalledFunction();
          if((callee != nullptr) && (callee->getName() == name))
            count++;
        }
      }
    }

    return count;
  }
};


TEST_F(CodegenOptimisationTest, MergeSendCrossMessaging)
{
  // Cross-messaging sends to two different actors interleaved. Exercises the
  // MergeMessageSend path that must not produce invalid LLVM IR; the module
  // verifier run inside optimise() would reject it.
  const char* src =
    "actor A\n"
    "  be m(a: A) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    test(A, A)\n"

    "  be test(a1: A, a2: A) =>\n"
    "    a1.m(a2)\n"
    "    a2.m(a1)";

  opt.release = true;

  TEST_COMPILE(src);
  DO(optimise());
}


TEST_F(CodegenOptimisationTest, MergeSendProducesChains)
{
  // Guard against MergeMessageSend silently going inert (#5589): consecutive
  // sends to the same actor must be chained, which the pass emits as calls to
  // pony_chain. pony_chain is produced nowhere else, so a non-zero count proves
  // the pass ran and merged.
  const char* src =
    "actor Sink\n"
    "  be take(x: U64) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s = Sink\n"
    "    s.take(1)\n"
    "    s.take(2)\n"
    "    s.take(3)\n"
    "    s.take(4)";

  opt.release = true;

  TEST_COMPILE(src);
  DO(optimise());

  ASSERT_GT(count_calls_to("pony_chain"), (size_t)0);
}


TEST_F(CodegenOptimisationTest, MergeSendTracedProducesSendNext)
{
  // Consecutive sends carrying GC-traced arguments exercise the trace-merge
  // path: later sends' pony_gc_send trace rounds are folded into the first by
  // rewriting them to pony_send_next. pony_send_next is produced nowhere else,
  // so a non-zero count proves the traced merge ran.
  const char* src =
    "actor Sink\n"
    "  be take(s: String) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let s = Sink\n"
    "    s.take(\"aa\")\n"
    "    s.take(\"bbb\")\n"
    "    s.take(\"cccc\")";

  opt.release = true;

  TEST_COMPILE(src);
  DO(optimise());

  ASSERT_GT(count_calls_to("pony_send_next"), (size_t)0);
}


TEST_F(CodegenOptimisationTest, HeapToStackStoreToAlloca)
{
  // An object stored into a field of a stack-promoted parent should also
  // be promotable.
  const char* src =
    "class Inner\n"
    "  let x: U64\n"
    "  new create(x': U64) => x = x'\n"

    "class Outer\n"
    "  let inner: Inner\n"
    "  new create() =>\n"
    "    inner = Inner(42)\n"

    "  fun get_x(): U64 => inner.x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let o = Outer\n"
    "    let x = o.get_x()";

  opt.release = true;

  TEST_COMPILE(src);

  size_t before = count_calls_to("pony_alloc_small");
  DO(optimise_with_heap_to_stack());
  size_t after = count_calls_to("pony_alloc_small");

  ASSERT_LT(after, before);
}


TEST_F(CodegenOptimisationTest, HeapToStackStoreToEscapingNotPromoted)
{
  // An object stored into an actor field escapes — it must stay on the
  // heap, so the optimization should not reduce pony_alloc_small calls.
  const char* src =
    "class Inner\n"
    "  let x: U64\n"
    "  new create(x': U64) => x = x'\n"

    "class Outer\n"
    "  let inner: Inner\n"
    "  new create() =>\n"
    "    inner = Inner(42)\n"

    "  fun get_x(): U64 => inner.x\n"

    "actor Main\n"
    "  var _o: (Outer | None)\n"
    "  new create(env: Env) =>\n"
    "    _o = Outer\n"
    "    let x = match _o\n"
    "    | let o: Outer => o.get_x()\n"
    "    else 0\n"
    "    end";

  opt.release = true;

  TEST_COMPILE(src);

  size_t before = count_calls_to("pony_alloc_small");
  DO(optimise_with_heap_to_stack());
  size_t after = count_calls_to("pony_alloc_small");

  ASSERT_GE(after, before);
}

TEST_F(CodegenOptimisationTest, NativeCallbackAndStructReferenceStore)
{
  const char* src =
    "type Callback is @{(U32)}\n"
    "class Payload\n"
    "struct Slot\n"
    "  var payload: Payload = Payload\n"
    "  fun ref replace(p: Payload) => payload = p\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Slot.replace(Payload)\n"
    "    invoke(@{(i: U32) => None })\n"
    "  be invoke(cb: Callback) => cb(42)\n";

  TEST_COMPILE(src);
  bool guarded_store = false;
  bool guarded_callback = false;
  auto module = llvm::unwrap(compile->module);
  for(auto& function : *module)
  {
    for(auto& block : function)
    {
      for(auto& inst : block)
      {
        auto guard = llvm::dyn_cast<llvm::CallBase>(&inst);
        if(guard == nullptr || guard->getCalledFunction() == nullptr ||
          guard->getCalledFunction()->getName() != "ponyint_gc_native_alias")
          continue;
        auto next = inst.getNextNode();
        if(function.getName().contains("Slot_ref_replace") &&
          llvm::isa_and_nonnull<llvm::StoreInst>(next))
          guarded_store = true;
        auto callback = llvm::dyn_cast_or_null<llvm::CallBase>(next);
        if(function.getName().contains("Main_tag_invoke") &&
          callback != nullptr && callback->isIndirectCall())
          guarded_callback = true;
      }
    }
  }
  EXPECT_TRUE(guarded_store);
  EXPECT_TRUE(guarded_callback);
}

TEST_F(CodegenOptimisationTest, NullAndUnknownPointerStores)
{
  const char* src =
    "type Bytes is Pointer[U8]\n"
    "type MaybeSlot is NullablePointer[Slot]\n"
    "struct Slot\n"
    "  var p: Bytes = Bytes\n"
    "  var n: MaybeSlot = MaybeSlot.none()\n"
    "  fun ref replace(value: Pointer[U8]) => p = value\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Slot.replace(opaque(Pointer[U8]))\n"
    "  fun opaque(p: Pointer[U8]): Pointer[U8] => p\n";
  TEST_COMPILE(src);
  bool saw_constructor = false;
  bool guarded_store = false;
  bool guarded_result = false;
  for(auto& fn : *llvm::unwrap(compile->module))
  {
    bool constructor = fn.getName().contains("Slot_iso_create");
    saw_constructor |= constructor;
    for(auto& block : fn)
      for(auto& inst : block)
      {
        auto* call = llvm::dyn_cast<llvm::CallBase>(&inst);
        if(call == nullptr || call->getCalledFunction() == nullptr ||
          call->getCalledFunction()->getName() != "ponyint_gc_native_alias")
          continue;
        EXPECT_FALSE(constructor);
        if(fn.getName().contains("Slot_ref_replace") &&
          llvm::isa_and_nonnull<llvm::StoreInst>(inst.getNextNode()))
          guarded_store = true;
        auto* prev = llvm::dyn_cast_or_null<llvm::CallBase>(inst.getPrevNode());
        if(prev != nullptr && prev->getCalledFunction() != nullptr &&
          prev->getCalledFunction()->getName().contains("Main_ref_opaque"))
          guarded_result = true;
      }
  }
  EXPECT_TRUE(saw_constructor);
  EXPECT_TRUE(guarded_store);
  EXPECT_TRUE(guarded_result);
}

TEST_F(CodegenOptimisationTest, BorrowedScalarStackAddress)
{
  const char* src =
    "use @memcmp[I32](a: Pointer[U32], b: Pointer[U32], n: USize)\n"
    "use @escape[None](a: Pointer[U32])\n"
    "actor Main\n"
    "  var field: U32 = 0\n"
    "  new create(env: Env) => run()\n"
    "  be run() =>\n"
    "    var local: U32 = 0\n"
    "    @memcmp(addressof local, addressof local, 4)\n"
    "    @memcmp(addressof field, addressof local, 4)\n"
    "    @escape(addressof local)\n";
  TEST_COMPILE(src);
  unsigned guards = 0;
  std::vector<unsigned> comparisons;
  std::vector<unsigned> escapes;
  for(auto& fn : *llvm::unwrap(compile->module))
  {
    if(!fn.getName().contains("Main_tag_run"))
      continue;
    for(auto& block : fn)
      for(auto& inst : block)
      {
        auto* call = llvm::dyn_cast<llvm::CallBase>(&inst);
        if(call == nullptr || call->getCalledFunction() == nullptr)
          continue;
        auto name = call->getCalledFunction()->getName();
        if(name == "ponyint_gc_native_alias")
          guards++;
        if(name == "memcmp")
        {
          comparisons.push_back(guards);
          guards = 0;
        }
        if(name == "escape")
          escapes.push_back(guards);
      }
  }
  EXPECT_EQ((std::vector<unsigned>{0, 1}), comparisons);
  ASSERT_EQ(1u, escapes.size());
  EXPECT_GT(escapes[0], 0u);
}

TEST_F(CodegenOptimisationTest, LocalCollectionReadsAndWritesBeyondItsArguments)
{
  TEST_COMPILE("actor Main\n  new create(env: Env) => None");
  auto* module = llvm::unwrap(compile->module);
  for(const char* name : {"ponyint_gc_poll", "ponyint_gc_finish"})
  {
    auto* function = module->getFunction(name);
    ASSERT_NE(nullptr, function) << name;
    auto effects = function->getMemoryEffects();
    for(auto location :
      {llvm::IRMemLocation::ArgMem, llvm::IRMemLocation::Other})
      EXPECT_EQ(llvm::ModRefInfo::ModRef, effects.getModRef(location)) << name;
  }
}

TEST_F(CodegenOptimisationTest, AllocatingLoopEmitsPolls)
{
  check_loop_polls();
}

TEST_F(CodegenOptimisationTest, MutationBuildEmitsLoopPolls)
{
  opt.mutate = true;
  check_loop_polls();
}


TEST_F(CodegenOptimisationTest, LoopFreeCalleeHasNoCollectingCopy)
{
  // `n + 1` and `> 2` compile inline. U64.add's body `this + y` is a call to
  // add, so if reach recorded inline calls, add would look recursive and
  // loopfree would get a copy.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run()\n"
    "  be run() =>\n"
    "    loopfree(1)\n"
    "    loopfree(2)\n"
    "    loopfree(3)\n"
    "  fun loopfree(n: U64): Array[U64] =>\n"
    "    if (n + 1) > 2 then Array[U64] else Array[U64] end";
  TEST_COMPILE(src);
  EXPECT_FALSE(has_copy("Main_ref_loopfree_"));
}

TEST_F(CodegenOptimisationTest, StraightLineBehaviorDoesNotPoll)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run()\n"
    "  be run() =>\n"
    "    Array[U64]\n"
    "    Array[U64]\n"
    "    Array[U64]";
  TEST_COMPILE(src);
  EXPECT_EQ(0u, polls_in("Main_tag_run", ""));
}

TEST_F(CodegenOptimisationTest, LoopFreeCalleeInsideLoopHasNoCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      loopfree()\n"
    "      i = i + 1\n"
    "    end\n"
    "  fun loopfree(): Array[U64] => Array[U64]";
  TEST_COMPILE(src);
  EXPECT_FALSE(has_copy("Main_ref_loopfree_"));
  EXPECT_EQ(1u, polls_in("Main_tag_run", ""));
}

TEST_F(CodegenOptimisationTest, LoopingCalleeHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => filler(n)\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_filler_"));
  EXPECT_EQ(1u, polls_in("Main_ref_filler_", ".gc"));
  EXPECT_FALSE(polls_at_entry("Main_ref_filler_"));
}

TEST_F(CodegenOptimisationTest, CalleeReachingLoopHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => outer(n)\n"
    "  fun outer(n: U64) =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_outer_"));
  EXPECT_TRUE(has_copy("Main_ref_filler_"));
  EXPECT_EQ(0u, polls_in("Main_ref_outer_", ".gc"));
}

TEST_F(CodegenOptimisationTest, SelfRecursiveCalleePollsOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => descend(n)\n"
    "  fun descend(n: U64) =>\n"
    "    if n > 0 then\n"
    "      Array[U64]\n"
    "      descend(n - 1)\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_descend_"));
}

TEST_F(CodegenOptimisationTest, MutuallyRecursiveCalleesPollOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => even(n)\n"
    "  fun even(n: U64) =>\n"
    "    if n > 0 then\n"
    "      Array[U64]\n"
    "      odd(n - 1)\n"
    "    end\n"
    "  fun odd(n: U64) =>\n"
    "    if n > 0 then\n"
    "      Array[U64]\n"
    "      even(n - 1)\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_even_"));
  EXPECT_TRUE(polls_at_entry("Main_ref_odd_"));
}

TEST_F(CodegenOptimisationTest, CyclesSharingAMethodPollOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => hub(n)\n"
    "  fun hub(n: U64) =>\n"
    "    if n > 0 then\n"
    "      Array[U64]\n"
    "      left(n - 1)\n"
    "      right(n - 1)\n"
    "    end\n"
    "  fun left(n: U64) =>\n"
    "    Array[U64]\n"
    "    hub(n)\n"
    "  fun right(n: U64) =>\n"
    "    Array[U64]\n"
    "    hub(n)";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_hub_"));
  EXPECT_TRUE(polls_at_entry("Main_ref_left_"));
  EXPECT_TRUE(polls_at_entry("Main_ref_right_"));
}

TEST_F(CodegenOptimisationTest, CycleThatSkipsALoopPollsOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => branch(n, true)\n"
    "  fun branch(n: U64, recurse: Bool) =>\n"
    "    if recurse then\n"
    "      back(n)\n"
    "    else\n"
    "      var i: U64 = 0\n"
    "      while i < n do\n"
    "        Array[U64]\n"
    "        i = i + 1\n"
    "      end\n"
    "    end\n"
    "  fun back(n: U64) =>\n"
    "    Array[U64]\n"
    "    if n > 0 then branch(n - 1, n > 1) end";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_branch_"));
  EXPECT_TRUE(polls_at_entry("Main_ref_back_"));
}

TEST_F(CodegenOptimisationTest, ChainedCallDoesNotMakeCallerCollect)
{
  // A `.>` call never runs a collecting copy, so it doesn't give chainer one.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let c: Counter ref = Counter\n"
    "    chainer(c, n)\n"
    "  fun chainer(c: Counter ref, n: U64) =>\n"
    "    Array[U64]\n"
    "    c.>filler(n)\n"
    "class Counter\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_FALSE(has_copy("Main_ref_chainer_"));
}

TEST_F(CodegenOptimisationTest, BoxMethodThroughRefReceiverHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let s: Scanner ref = Scanner\n"
    "    through(s, n)\n"
    "  fun through(s: Scanner ref, n: U64) =>\n"
    "    Array[U64]\n"
    "    s.scan(n)\n"
    "class Scanner\n"
    "  fun box scan(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_through_"));
  EXPECT_TRUE(has_copy("Scanner_ref_scan"));
}

TEST_F(CodegenOptimisationTest, TypeParameterReceiverHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => Holder[Scanner](Scanner).visit(n)\n"
    "class Holder[A: Scanner ref]\n"
    "  let _a: A\n"
    "  new create(a: A) => _a = a\n"
    "  fun ref visit(n: U64) =>\n"
    "    Array[U64]\n"
    "    _a.scan(n)\n"
    "class Scanner\n"
    "  fun ref scan(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("_ref_visit_"));
  EXPECT_TRUE(has_copy("Scanner_ref_scan_"));
}

TEST_F(CodegenOptimisationTest, TraitDefaultWithLoopHasCollectingDispatch)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let s: Spinner ref = if n > 1 then First else Second end\n"
    "    through(s, n)\n"
    "  fun through(s: Spinner ref, n: U64) =>\n"
    "    Array[U64]\n"
    "    s.spin(n)\n"
    "trait Spinner\n"
    "  fun ref spin(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "class First is Spinner\n"
    "class Second is Spinner";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_through_"));
  EXPECT_FALSE(functions_named("_ref_spin_", ".gc.dispatch").empty());
  EXPECT_TRUE(has_copy("First_ref_spin"));
  EXPECT_TRUE(has_copy("Second_ref_spin"));
}

TEST_F(CodegenOptimisationTest, InterfaceCallReachingLoopHasCollectingDispatch)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let s: Stepper ref = if n > 1 then Looper else Plain end\n"
    "    s.step(n)\n"
    "interface Stepper\n"
    "  fun ref step(n: U64)\n"
    "class Looper\n"
    "  fun ref step(n: U64) =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "  fun ref filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "class Plain\n"
    "  fun ref step(n: U64) =>\n"
    "    Array[U64]";
  TEST_COMPILE(src);
  EXPECT_FALSE(functions_named("_ref_step_", ".gc.dispatch").empty());
  EXPECT_TRUE(has_copy("Looper_ref_step"));
  EXPECT_TRUE(has_copy("Looper_ref_filler"));
  EXPECT_FALSE(has_copy("Plain_ref_step"));
}

TEST_F(CodegenOptimisationTest, InterfaceWithoutLoopsHasNoCollectingDispatch)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let s: Stepper ref = if n > 1 then Plain else Other end\n"
    "    s.step(n)\n"
    "interface Stepper\n"
    "  fun ref step(n: U64)\n"
    "class Plain\n"
    "  fun ref step(n: U64) =>\n"
    "    Array[U64]\n"
    "class Other\n"
    "  fun ref step(n: U64) =>\n"
    "    Array[U64]";
  TEST_COMPILE(src);
  EXPECT_TRUE(functions_named("_ref_step_", ".gc.dispatch").empty());
  EXPECT_FALSE(has_copy("_ref_step_"));
}

TEST_F(CodegenOptimisationTest, RepeatLoopHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => through(n)\n"
    "  fun through(n: U64) =>\n"
    "    Array[U64]\n"
    "    spinner(n)\n"
    "  fun spinner(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    repeat\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    until i >= n end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_through_"));
  EXPECT_TRUE(has_copy("Main_ref_spinner_"));
}

TEST_F(CodegenOptimisationTest, RecursionThroughInterfacePollsOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun ref walk(n: U64) =>\n"
    "    Array[U64]\n"
    "    if n > 0 then\n"
    "      let v: Visitor ref = if n > 2 then Left else Right end\n"
    "      v.visit(this, n - 1)\n"
    "    end\n"
    "interface Visitor\n"
    "  fun ref visit(main: Main ref, n: U64)\n"
    "class Left\n"
    "  fun ref visit(main: Main ref, n: U64) =>\n"
    "    Array[U64]\n"
    "    main.walk(n)\n"
    "class Right\n"
    "  fun ref visit(main: Main ref, n: U64) =>\n"
    "    Array[U64]\n"
    "    main.walk(n)";
  TEST_COMPILE(src);
  EXPECT_FALSE(functions_named("_ref_visit_", ".gc.dispatch").empty());
  EXPECT_TRUE(polls_at_entry("Main_ref_walk_"));
  EXPECT_TRUE(polls_at_entry("Left_ref_visit"));
  EXPECT_TRUE(polls_at_entry("Right_ref_visit"));
}

TEST_F(CodegenOptimisationTest, SiblingsCallingOneLoopHaveCollectingCopies)
{
  // filler is reached through two callers, left and right.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => top(n)\n"
    "  fun top(n: U64) =>\n"
    "    Array[U64]\n"
    "    left(n)\n"
    "    right(n)\n"
    "  fun left(n: U64) =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "  fun right(n: U64) =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_left_"));
  EXPECT_TRUE(has_copy("Main_ref_right_"));
}

TEST_F(CodegenOptimisationTest, CallToEarlierResolvedLoopHasCollectingCopy)
{
  // The behavior calls filler directly before it calls outer, which also
  // calls filler.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    filler(n)\n"
    "    outer(n)\n"
    "  fun outer(n: U64) =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_outer_"));
}

TEST_F(CodegenOptimisationTest, RecursionStartingWithLoopLocalPollsOnEntry)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => descend(n)\n"
    "  fun descend(n: U64) =>\n"
    "    while (let limit = n; limit > 100) do\n"
    "      Array[U64]\n"
    "    end\n"
    "    Array[U64]\n"
    "    if n > 0 then descend(n - 1) end";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_descend_"));
}

TEST_F(CodegenOptimisationTest, EntryPollThatCannotTraceRootsRoutesNoCalls)
{
  // a is consumed in descend's first statement, so a poll in descend can't
  // trace a as a root.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => descend(recover iso Array[U64] end, n)\n"
    "  fun descend(a: Array[U64] iso, n: U64) =>\n"
    "    let b: Array[U64] iso = consume a\n"
    "    Array[U64]\n"
    "    if n > 0 then descend(consume b, n - 1) end";
  TEST_COMPILE(src);
  auto copies = functions_named("Main_ref_descend_", ".gc");
  ASSERT_EQ(1u, copies.size());
  EXPECT_FALSE(polls_at_entry("Main_ref_descend_"));
  bool calls_ordinary = false;
  for(auto& block : *copies[0])
    for(auto& inst : block)
      if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
        if(call->getCalledFunction() != nullptr)
        {
          auto name = call->getCalledFunction()->getName();
          EXPECT_FALSE(name.ends_with(".gc"));
          if(name.starts_with("Main_ref_descend_"))
            calls_ordinary = true;
        }
  EXPECT_TRUE(calls_ordinary);
}

TEST_F(CodegenOptimisationTest, UnionReceiverReachingLoopHasCollectingCopy)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    let s: (Looper ref | Plain ref) =\n"
    "      if n > 1 then Looper else Plain end\n"
    "    through(s, n)\n"
    "  fun through(s: (Looper ref | Plain ref), n: U64) =>\n"
    "    Array[U64]\n"
    "    s.step(n)\n"
    "class Looper\n"
    "  fun ref step(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "class Plain\n"
    "  fun ref step(n: U64) =>\n"
    "    Array[U64]";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_through_"));
  EXPECT_FALSE(functions_named("_ref_step_", ".gc.dispatch").empty());
  EXPECT_TRUE(has_copy("Looper_ref_step_"));
  EXPECT_FALSE(has_copy("Plain_ref_step_"));
}

TEST_F(CodegenOptimisationTest, UserOperatorReachingLoopHasCollectingCopy)
{
  // `a + b` on a class is a call to its add method, not inline instructions.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => through(Vec(n), Vec(n))\n"
    "  fun through(a: Vec ref, b: Vec ref) =>\n"
    "    Array[U64]\n"
    "    a + b\n"
    "class Vec\n"
    "  let n: U64\n"
    "  new create(n': U64) => n = n'\n"
    "  fun ref add(other: Vec ref): Vec ref =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "    this";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_through_"));
  EXPECT_TRUE(has_copy("Vec_ref_add_"));
}

TEST_F(CodegenOptimisationTest, CallsWithoutLoopsHaveNoCollectingCopies)
{
  // The behavior calls leaf directly before it calls top; top calls mid, and
  // mid calls leaf.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) =>\n"
    "    leaf(n)\n"
    "    top(n)\n"
    "  fun top(n: U64) =>\n"
    "    Array[U64]\n"
    "    mid(n)\n"
    "  fun mid(n: U64) =>\n"
    "    Array[U64]\n"
    "    leaf(n)\n"
    "  fun leaf(n: U64) =>\n"
    "    Array[U64]";
  TEST_COMPILE(src);
  EXPECT_FALSE(has_copy("Main_ref_top_"));
  EXPECT_FALSE(has_copy("Main_ref_mid_"));
  EXPECT_FALSE(has_copy("Main_ref_leaf_"));
}

TEST_F(CodegenOptimisationTest, RecursionPollsBeforeEachReturn)
{
  // The Array made after the recursive call returns is garbage at every
  // level; a poll before each return can collect it while the recursion
  // unwinds.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => climb(n)\n"
    "  fun climb(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let below = climb(n - 1)\n"
    "    Array[U64]\n"
    "    below + 1";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_climb_"));
  EXPECT_TRUE(polls_before_returns("Main_ref_climb_"));
  EXPECT_EQ(3u, polls_in("Main_ref_climb_", ".gc"));
}

TEST_F(CodegenOptimisationTest, NonRecursiveCopyDoesNotPollBeforeReturn)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => outer(n)\n"
    "  fun outer(n: U64): U64 =>\n"
    "    Array[U64]\n"
    "    filler(n)\n"
    "    n\n"
    "  fun filler(n: U64) =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(has_copy("Main_ref_outer_"));
  EXPECT_EQ(0u, polls_in("Main_ref_outer_", ".gc"));
}

TEST_F(CodegenOptimisationTest, MutualRecursionPollsBeforeEachReturn)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => even(n)\n"
    "  fun even(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let below = odd(n - 1)\n"
    "    Array[U64]\n"
    "    below + 1\n"
    "  fun odd(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let below = even(n - 1)\n"
    "    Array[U64]\n"
    "    below + 1";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_before_returns("Main_ref_even_"));
  EXPECT_TRUE(polls_before_returns("Main_ref_odd_"));
}

TEST_F(CodegenOptimisationTest, RecursionThroughInterfacePollsBeforeEachReturn)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun ref walk(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let v: Visitor ref = if n > 2 then Left else Right end\n"
    "    let below = v.visit(this, n - 1)\n"
    "    Array[U64]\n"
    "    below + 1\n"
    "interface Visitor\n"
    "  fun ref visit(main: Main ref, n: U64): U64\n"
    "class Left\n"
    "  fun ref visit(main: Main ref, n: U64): U64 =>\n"
    "    Array[U64]\n"
    "    main.walk(n)\n"
    "class Right\n"
    "  fun ref visit(main: Main ref, n: U64): U64 =>\n"
    "    Array[U64]\n"
    "    main.walk(n)";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_before_returns("Main_ref_walk_"));
  // visit returns the result of its call to walk's copy directly, so it only
  // polls on entry.
  EXPECT_EQ(1u, polls_in("Left_ref_visit_", ".gc"));
  EXPECT_EQ(1u, polls_in("Right_ref_visit_", ".gc"));
}

TEST_F(CodegenOptimisationTest, RecursionInsideMatchPollsBeforeReturn)
{
  // The recursive call and the garbage after it are both inside one match,
  // the method's last expression.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => down(n)\n"
    "  fun down(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    match down(n - 1)\n"
    "    | let x: U64 =>\n"
    "      Array[U64]\n"
    "      x + 1\n"
    "    end";
  TEST_COMPILE(src);
  // Entry, `return 0`, and the end of the match's only case.
  EXPECT_EQ(3u, polls_in("Main_ref_down_", ".gc"));
}

TEST_F(CodegenOptimisationTest, RecursionThroughWrapperPollsBeforeReturn)
{
  // wrap makes garbage after each recursive call returns, and has no
  // collecting copy of its own.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => down(n)\n"
    "  fun down(n: U64): U64 =>\n"
    "    if n == 0 then 0 else wrap(down(n - 1)) end\n"
    "  fun wrap(x: U64): U64 =>\n"
    "    Array[U64]\n"
    "    x + 1";
  TEST_COMPILE(src);
  EXPECT_FALSE(has_copy("Main_ref_wrap_"));
  // Entry, and the end of each branch of the returned `if`.
  EXPECT_EQ(3u, polls_in("Main_ref_down_", ".gc"));
}

TEST_F(CodegenOptimisationTest, ExitPollRootsReturnedObject)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => build(n)\n"
    "  fun build(n: U64): Array[U64] =>\n"
    "    if n == 0 then return Array[U64] end\n"
    "    let below = build(n - 1)\n"
    "    Array[U64]\n"
    "    below";
  TEST_COMPILE(src);
  auto copies = functions_named("Main_ref_build_", ".gc");
  ASSERT_EQ(1u, copies.size());
  // Every exit poll stores the returned Array to a slot and hands the value
  // loaded from that slot to the root-tracing helper.
  size_t rooted = rooted_result_polls("Main_ref_build_");
  EXPECT_EQ(2u, rooted);
}

TEST_F(CodegenOptimisationTest, ExitPollSkipsUntraceableResult)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => raw(n)\n"
    "  fun raw(n: U64): Pointer[U8] =>\n"
    "    if n == 0 then return Pointer[U8] end\n"
    "    let below = raw(n - 1)\n"
    "    Array[U8]\n"
    "    below";
  TEST_COMPILE(src);
  ASSERT_TRUE(has_copy("Main_ref_raw_"));
  EXPECT_TRUE(polls_at_entry("Main_ref_raw_"));
  EXPECT_EQ(1u, polls_in("Main_ref_raw_", ".gc"));
}

TEST_F(CodegenOptimisationTest, TailCallToOwnCopyHasNoExitPoll)
{
  // The callee polls before it returns, and nothing runs after the call, so a
  // poll after it would only stop LLVM from turning the call into a jump.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun walk(n: U64): U64 =>\n"
    "    if n == 0 then return 7 end\n"
    "    Array[U64]\n"
    "    walk(n - 1)";
  TEST_COMPILE(src);
  auto copies = functions_named("Main_ref_walk_", ".gc");
  ASSERT_EQ(1u, copies.size());
  bool found = false;
  for(auto& block : *copies[0])
  {
    bool after_call = false;
    for(auto& inst : block)
    {
      if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
        if(call->getCalledFunction() == copies[0])
          after_call = found = true;
      if(after_call)
        EXPECT_FALSE(is_poll(inst));
    }
    if(after_call)
      EXPECT_TRUE(llvm::isa<llvm::ReturnInst>(block.getTerminator()));
  }
  EXPECT_TRUE(found);
}

TEST_F(CodegenOptimisationTest, PartialRecursionPollsBeforeEachReturn)
{
  // The entry, the two value returns and the error return each poll.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => try climb(n)? end\n"
    "  fun climb(n: U64): U64 ? =>\n"
    "    if n == 0 then return 0 end\n"
    "    if n == 99 then error end\n"
    "    let below = climb(n - 1)?\n"
    "    Array[U64]\n"
    "    below + 1";
  TEST_COMPILE(src);
  EXPECT_TRUE(polls_at_entry("Main_ref_climb_"));
  EXPECT_EQ(4u, polls_in("Main_ref_climb_", ".gc"));
}

TEST_F(CodegenOptimisationTest, ErrorExitFromRecursionPolls)
{
  // Each level makes garbage in the else clause and raises the error again,
  // so the error return is the only exit on the way back up.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => try climb(n)? end\n"
    "  fun climb(n: U64): U64 ? =>\n"
    "    if n == 0 then error end\n"
    "    try climb(n - 1)? else\n"
    "      Array[U64]\n"
    "      error\n"
    "    end";
  TEST_COMPILE(src);
  auto copies = functions_named("Main_ref_climb_", ".gc");
  ASSERT_EQ(1u, copies.size());
  for(auto& block : *copies[0])
  {
    auto* ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator());
    if(ret == nullptr)
      continue;
    bool polled = calls_poll(block);
    for(auto* pred : llvm::predecessors(&block))
      polled |= calls_poll(*pred);
    EXPECT_TRUE(polled);
  }
}

TEST_F(CodegenOptimisationTest, IfTailCallToOwnCopyHasNoPollAfterIt)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun walk(n: U64): U64 =>\n"
    "    if n == 0 then\n"
    "      7\n"
    "    else\n"
    "      Array[U64]\n"
    "      walk(n - 1)\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(no_poll_after_self_call("Main_ref_walk_"));
  // Entry, and the end of the branch that doesn't recurse.
  EXPECT_EQ(2u, polls_in("Main_ref_walk_", ".gc"));
}

TEST_F(CodegenOptimisationTest, MatchTailCallToOwnCopyHasNoPollAfterIt)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun walk(n: U64): U64 =>\n"
    "    match n\n"
    "    | 0 => 7\n"
    "    else\n"
    "      Array[U64]\n"
    "      walk(n - 1)\n"
    "    end";
  TEST_COMPILE(src);
  EXPECT_TRUE(no_poll_after_self_call("Main_ref_walk_"));
  EXPECT_EQ(2u, polls_in("Main_ref_walk_", ".gc"));
}

TEST_F(CodegenOptimisationTest, TailCallToNonRecursiveCopyKeepsExitPoll)
{
  // filler's copy only polls inside its loop, so climb still polls before it
  // returns filler's result.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => climb(n)\n"
    "  fun climb(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let below = climb(n - 1)\n"
    "    Array[U64]\n"
    "    filler(below)\n"
    "  fun filler(n: U64): U64 =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "    i";
  TEST_COMPILE(src);
  ASSERT_TRUE(has_copy("Main_ref_filler_"));
  // Entry, `return 0`, and before returning filler's result.
  EXPECT_EQ(3u, polls_in("Main_ref_climb_", ".gc"));
}

TEST_F(CodegenOptimisationTest, ReturnedIfDoesNotPollBranches)
{
  // An `if` under `return` doesn't poll in its branches, because a then
  // clause like this one can run after it and use held. The `return` polls
  // once, after the then clause.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => climb(n)\n"
    "  fun climb(n: U64): U64 =>\n"
    "    let held = Holder\n"
    "    try\n"
    "      if n == 99 then error end\n"
    "      return if n == 0 then 0 else climb(n - 1) + 1 end\n"
    "    then\n"
    "      held.touch()\n"
    "    end\n"
    "    0\n"
    "class Holder\n"
    "  fun touch(): U64 => 1";
  TEST_COMPILE(src);
  // Entry, the `return`, and the end of the body.
  EXPECT_EQ(3u, polls_in("Main_ref_climb_", ".gc"));
  // No poll comes between the computed `if` and the then clause's call.
  auto copies = functions_named("Main_ref_climb_", ".gc");
  ASSERT_EQ(1u, copies.size());
  for(auto& block : *copies[0])
  {
    bool polled = false;
    for(auto& inst : block)
    {
      if(is_poll(inst) && !block.getName().starts_with("entry"))
        polled = true;
      if(auto* call = llvm::dyn_cast<llvm::CallBase>(&inst))
        if(call->getCalledFunction() != nullptr &&
          call->getCalledFunction()->getName().contains("_touch_"))
          EXPECT_FALSE(polled);
    }
  }
}

TEST_F(CodegenOptimisationTest, BoundIfAndMatchDoNotPollBranches)
{
  // Only returned constructs poll per branch; these values are bound to
  // locals while other locals are still live.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun walk(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let a = if n > 2 then Array[U64] else Array[U64] end\n"
    "    let b = match n | 1 => Array[U64] else Array[U64] end\n"
    "    let below = walk(n - 1)\n"
    "    below + 1";
  TEST_COMPILE(src);
  // Entry, `return 0`, and the end of the body.
  EXPECT_EQ(3u, polls_in("Main_ref_walk_", ".gc"));
}

TEST_F(CodegenOptimisationTest, BranchPollsRootReturnedObject)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => build(n)\n"
    "  fun build(n: U64): Array[U64] =>\n"
    "    if n == 0 then\n"
    "      Array[U64]\n"
    "    else\n"
    "      let below = build(n - 1)\n"
    "      Array[U64]\n"
    "      below\n"
    "    end";
  TEST_COMPILE(src);
  auto copies = functions_named("Main_ref_build_", ".gc");
  ASSERT_EQ(1u, copies.size());
  size_t rooted = rooted_result_polls("Main_ref_build_");
  // One rooted poll at the end of each branch.
  EXPECT_EQ(2u, rooted);
}

TEST_F(CodegenOptimisationTest, DispatchTailCallToNonRecursiveKeepsExitPoll)
{
  // Plain's step loops but doesn't recurse, so a call that can reach it
  // doesn't skip walk's exit poll.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun ref walk(n: U64): U64 =>\n"
    "    if n == 0 then return 0 end\n"
    "    let v: Visitor ref = if n > 2 then Back else Plain end\n"
    "    v.step(this, n - 1)\n"
    "interface Visitor\n"
    "  fun ref step(main: Main ref, n: U64): U64\n"
    "class Back\n"
    "  fun ref step(main: Main ref, n: U64): U64 =>\n"
    "    Array[U64]\n"
    "    main.walk(n)\n"
    "class Plain\n"
    "  fun ref step(main: Main ref, n: U64): U64 =>\n"
    "    var i: U64 = 0\n"
    "    while i < n do\n"
    "      Array[U64]\n"
    "      i = i + 1\n"
    "    end\n"
    "    i";
  TEST_COMPILE(src);
  // Entry, `return 0`, and after the dispatched call.
  EXPECT_EQ(3u, polls_in("Main_ref_walk_", ".gc"));
}

TEST_F(CodegenOptimisationTest, NestedReturnedBranchesPollOnce)
{
  // The inner `if` is the value of the outer match's first case, so only its
  // branches poll; the outer case doesn't poll again.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => run(3)\n"
    "  be run(n: U64) => walk(n)\n"
    "  fun walk(n: U64): U64 =>\n"
    "    match n\n"
    "    | 0 => if n == 0 then 1 else 2 end\n"
    "    else\n"
    "      Array[U64]\n"
    "      walk(n - 1)\n"
    "    end";
  TEST_COMPILE(src);
  // Entry and the two branches of the inner `if`.
  EXPECT_EQ(3u, polls_in("Main_ref_walk_", ".gc"));
}
