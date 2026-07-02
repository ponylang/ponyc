#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/genopt.h>

#include "util.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstrTypes.h>

#include "llvm_config_end.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))


class CodegenOptimisationTest : public PassTest
{
protected:
  // Run the Pony-specific optimisation passes (including MergeMessageSend) over
  // the compiled module, the same way genexe does for a real build. The unit
  // test compile path stops at gentypes and never optimises, so without this a
  // test of an optimisation pass would never actually run it.
  //
  // The functions are forced to external linkage first: a release build gives
  // generated functions private linkage, and because the test module has no
  // gen_main entry point, the O3 pipeline's global dead-code elimination would
  // otherwise delete every (unreferenced) function before the scan below could
  // see the merged sends.
  void optimise()
  {
    auto module = llvm::unwrap(compile->module);

    for(auto& function : *module)
    {
      if(!function.isDeclaration())
        function.setLinkage(llvm::GlobalValue::ExternalLinkage);
    }

    // genopt runs the module verifier (opt.verify is set by PassTest), so an
    // invalid-IR merge aborts the test here.
    ASSERT_TRUE(genopt(compile, true));
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
