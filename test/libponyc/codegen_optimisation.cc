#include <gtest/gtest.h>
#include <platform.h>

#include <vector>

#include <codegen/genopt.h>
#include <heap_to_stack.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstrTypes.h>
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
    for(auto location : {llvm::IRMemLocation::ArgMem, llvm::IRMemLocation::Other})
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
