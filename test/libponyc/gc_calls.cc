#include <gtest/gtest.h>

#include "llvm_config_begin.h"
#include <gc_calls.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#include "llvm_config_end.h"

namespace
{
class GcCalls : public testing::Test
{
protected:
  llvm::LLVMContext ctx;
  std::unique_ptr<llvm::Module> module;

  void optimize(const std::string& ordinary, const std::string& collecting)
  {
    llvm::SMDiagnostic diagnostic;
    module = llvm::parseAssemblyString(
      "@entries = constant [2 x ptr] [ptr @work, ptr @collect], "
      "!pony.gc.pairs !0\n!0 = !{}\n"
      "declare ptr @pony_alloc_small(ptr, i32)\n"
      "define i1 @ponyint_gc_has_small_room(ptr %actor) {\n"
      "  %room = load i1, ptr %actor\n  ret i1 %room\n}\n"
      "define ptr @work(ptr %a, ptr %b, i1 %base) {\n" + ordinary + "}\n"
      "define ptr @collect(ptr %a, ptr %b, i1 %base, ptr %actor, "
      "ptr %frame) {\n" + collecting + "}\n", diagnostic, ctx);
    ASSERT_NE(nullptr, module) << diagnostic.getMessage().str();
    llvm::ModuleAnalysisManager analyses;
    pony::OptimizeGcCalls().run(*module, analyses);
    ASSERT_FALSE(llvm::verifyModule(*module, &llvm::errs()));
  }
};

TEST_F(GcCalls, RecursiveCallKeepsItsComputedArguments)
{
  ASSERT_NO_FATAL_FAILURE(optimize("ret ptr %a\n",
    "%r = call ptr @collect(ptr %b, ptr %a, i1 true, ptr %actor, ptr %frame)\n"
    "ret ptr %r\n"));
  auto* fn = module->getFunction("collect");
  auto* call = llvm::dyn_cast<llvm::CallInst>(&fn->getEntryBlock().front());
  ASSERT_NE(nullptr, call);
  EXPECT_EQ(module->getFunction("work"), call->getCalledFunction());
  ASSERT_EQ(3u, call->arg_size());
  EXPECT_EQ(fn->getArg(1), call->getArgOperand(0));
  EXPECT_EQ(fn->getArg(0), call->getArgOperand(1));
  EXPECT_TRUE(llvm::cast<llvm::ConstantInt>(call->getArgOperand(2))->isOne());
}

TEST_F(GcCalls, AllocationOnUnwindCanRepeatAtEveryDepth)
{
  for(bool allocate_on_unwind : {false, true})
  {
    std::string body = "br i1 %base, label %leaf, label %descend\n"
      "leaf:\n%p = call ptr @pony_alloc_small(ptr %a, i32 0)\nret ptr %p\n"
      "descend:\n%r = call ptr @work(ptr %b, ptr %a, i1 true)\n";
    if(allocate_on_unwind)
      body += "%q = call ptr @pony_alloc_small(ptr %a, i32 0)\n";
    ASSERT_NO_FATAL_FAILURE(optimize(body + "ret ptr %r\n", "ret ptr %b\n"));
    auto* fn = module->getFunction("collect");
    if(allocate_on_unwind)
    {
      EXPECT_EQ(1u, fn->size());
      continue;
    }
    auto* branch = llvm::dyn_cast<llvm::CondBrInst>(
      fn->getEntryBlock().getTerminator());
    ASSERT_NE(nullptr, branch);
    auto* room = llvm::dyn_cast<llvm::LoadInst>(branch->getCondition());
    ASSERT_NE(nullptr, room);
    EXPECT_EQ(fn->getArg(3), room->getPointerOperand());
    auto* ret = llvm::cast<llvm::ReturnInst>(
      branch->getSuccessor(0)->getTerminator());
    auto* call = llvm::dyn_cast<llvm::CallInst>(ret->getReturnValue());
    ASSERT_NE(nullptr, call);
    EXPECT_EQ(module->getFunction("work"), call->getCalledFunction());
    for(unsigned i = 0; i < 3; i++)
      EXPECT_EQ(fn->getArg(i), call->getArgOperand(i));
  }
}
TEST_F(GcCalls, PurePrefixCannotClearInheritedCollectionDebt)
{
  llvm::SMDiagnostic diagnostic;
  module = llvm::parseAssemblyString(R"(
    @entries = constant [2 x ptr] [ptr @id, ptr @id_gc], !pony.gc.pairs !0
    !0 = !{}
    define ptr @id(ptr %x) { ret ptr %x }
    define ptr @id_gc(ptr %x, ptr %actor, ptr %parent) { ret ptr %x }
    declare ptr @ponyint_gc_poll(ptr, ptr)
    define ptr @work(ptr %actor, ptr %parent) {
      %frame = call ptr @id_gc(ptr %parent, ptr %actor, ptr %parent)
      %p = call ptr @ponyint_gc_poll(ptr %actor, ptr %frame)
      ret ptr %p
    }
  )", diagnostic, ctx);
  ASSERT_NE(nullptr, module) << diagnostic.getMessage().str();
  llvm::ModuleAnalysisManager analyses;
  pony::OptimizeGcCalls().run(*module, analyses);
  auto* ret = llvm::cast<llvm::ReturnInst>(
    module->getFunction("work")->getEntryBlock().getTerminator());
  auto* poll = llvm::dyn_cast<llvm::CallInst>(ret->getReturnValue());
  ASSERT_NE(nullptr, poll);
  EXPECT_EQ(module->getFunction("ponyint_gc_poll"), poll->getCalledFunction());
  auto* prefix = llvm::dyn_cast<llvm::CallInst>(poll->getArgOperand(1));
  ASSERT_NE(nullptr, prefix);
  EXPECT_EQ(module->getFunction("id"), prefix->getCalledFunction());
}

TEST_F(GcCalls, FinalLinkReleasesThePrivateEntryTable)
{
  llvm::SMDiagnostic diagnostic;
  module = llvm::parseAssemblyString(R"(
    @entries = private constant [2 x ptr] [ptr @work, ptr @collect], !pony.gc.pairs !0
    @keep = internal global i8 0
    @llvm.compiler.used = appending global [2 x ptr] [ptr @entries, ptr @keep], section "llvm.metadata"
    declare void @work()
    declare void @collect(ptr, ptr)
    !0 = !{}
  )", diagnostic, ctx);
  ASSERT_NE(nullptr, module) << diagnostic.getMessage().str();
  llvm::ModuleAnalysisManager analyses;
  pony::LowerGcMaps().run(*module, analyses);
  EXPECT_EQ(nullptr, module->getNamedGlobal("entries"));
  auto* used = module->getNamedGlobal("llvm.compiler.used");
  ASSERT_NE(nullptr, used);
  ASSERT_EQ(1u, used->getInitializer()->getNumOperands());
  EXPECT_EQ(module->getNamedGlobal("keep"), used->getInitializer()->getOperand(0));
}

TEST_F(GcCalls, BorrowCleanupKeepsUntaggedInlineAssembly)
{
  llvm::SMDiagnostic diagnostic;
  module = llvm::parseAssemblyString(R"(
    define ptr @work(ptr %p) {
      %root = call ptr asm sideeffect "", "=r,0"(ptr %p), !pony.gc.borrow !0
      %frame = call ptr asm sideeffect "", "=r,0"(ptr %root)
      ret ptr %frame
    }
    !0 = !{}
    !llvm.module.flags = !{!1}
    !1 = !{i32 7, !"pony.midbehavior.gc", i32 1}
  )", diagnostic, ctx);
  ASSERT_NE(nullptr, module) << diagnostic.getMessage().str();
  llvm::ModuleAnalysisManager analyses;
  pony::OptimizeGcCalls().run(*module, analyses);
  auto* work = module->getFunction("work");
  auto* call = llvm::dyn_cast<llvm::CallInst>(&work->getEntryBlock().front());
  ASSERT_NE(nullptr, call);
  EXPECT_FALSE(call->hasMetadata("pony.gc.borrow"));
  EXPECT_EQ(work->getArg(0), call->getArgOperand(0));
  EXPECT_EQ(work->getEntryBlock().getTerminator(), call->getNextNode());
}

TEST_F(GcCalls, NativeMapRequiresTheActualTracerAndAnUnaliasedRecord)
{
  for(const std::string mode : {"mapped", "different tracer", "escaped",
    "stack arguments", "aggregate return", "aggregate caller"})
  {
    SCOPED_TRACE(mode);
    std::string type = mode == "aggregate return" ? "{i64, i64}" : "i64";
    std::string result = mode == "aggregate caller" ? "{i64, i64}" : "void";
    std::string ret = result == "void" ? "ret void" :
      "ret {i64, i64} zeroinitializer";
    std::string params = mode == "stack arguments" ?
      ", ptr %a, ptr %b, ptr %c, ptr %d, ptr %e" : "";
    std::string tracer = mode == "different tracer" ? "@other" : "@trace";
    std::string escape = mode == "escaped" ?
      "call void @escape(ptr %frame)\n" : "";
    llvm::SMDiagnostic diagnostic;
    module = llvm::parseAssemblyString(
      "target triple = \"x86_64-unknown-linux-gnu\"\n"
      "%F = type { ptr, ptr, ptr }\n"
      "@pairs = constant [2 x ptr] [ptr @plain, ptr @collect], "
      "!pony.gc.pairs !0\n!0 = !{}\n!1 = !{ptr @trace, i32 3}\n"
      "declare void @trace(ptr, ptr)\ndeclare void @other(ptr, ptr)\n"
      "declare void @escape(ptr)\ndeclare " + type + " @plain(ptr)\n"
      "define " + type + " @collect(ptr %x, ptr %actor, ptr %parent) { ret "
      + type + " zeroinitializer }\n"
      "define " + result + " @caller(ptr %parent, ptr %root" + params + ") {\n"
      "%frame = alloca %F\n%t = getelementptr %F, ptr %frame, i32 0, i32 1\n"
      "%r = getelementptr %F, ptr %frame, i32 0, i32 2\n"
      "store ptr %parent, ptr %frame\nstore ptr " + tracer + ", ptr %t\n"
      "store ptr %root, ptr %r\n" + escape +
      "%id = call ptr asm \"\", \"=r,0\"(ptr %frame)\n"
      "%x = call " + type + " @collect(ptr %root, ptr null, ptr %id), "
      "!pony.gc.frame !1\n" + ret + "\n}", diagnostic, ctx);
    ASSERT_NE(nullptr, module) << diagnostic.getMessage().str();
    llvm::PassBuilder passes;
    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager sccs;
    llvm::ModuleAnalysisManager modules;
    passes.registerLoopAnalyses(loops);
    passes.registerFunctionAnalyses(functions);
    passes.registerCGSCCAnalyses(sccs);
    passes.registerModuleAnalyses(modules);
    passes.crossRegisterProxies(loops, functions, sccs, modules);
    pony::LowerGcMaps().run(*module, modules);
    ASSERT_FALSE(llvm::verifyModule(*module, &llvm::errs()));
    auto* caller = module->getFunction("caller");
    EXPECT_EQ(mode == "mapped", caller->hasGC());
    auto* target = module->getFunction("collect");
    llvm::CallInst* call = nullptr;
    for(auto* user : target->users())
      if(auto* inst = llvm::dyn_cast<llvm::CallInst>(user))
        call = inst;
    ASSERT_NE(nullptr, call);
    EXPECT_EQ(mode == "mapped", call->getOperandBundle("deopt").has_value());
  }
}
}
