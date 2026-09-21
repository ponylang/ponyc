#include "genopt.h"

#include <string.h>

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Passes/PassBuilder.h>

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/Lint.h>
#include <llvm/Transforms/IPO/Inliner.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/SROA.h>

#include <llvm-c/DebugInfo.h>

#include "llvm_config_end.h"

#include "../ast/stringtab.h"
#include "ponyassert.h"

using namespace llvm;

// Pass to replace pony_ctx calls in a dispatch function by the context passed
// to the function.
class DispatchPonyCtx : public PassInfoMixin<DispatchPonyCtx>
{
public:

  DispatchPonyCtx() : PassInfoMixin<DispatchPonyCtx>()
  {}

  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am)
  {
    // Check if we're in a Dispatch function.
    StringRef name = f.getName();
    if(name.size() < 10 || name.rfind("_Dispatch") != name.size() - 9)
      return PreservedAnalyses::all();

    pony_assert(f.arg_size() > 0);

    Value* ctx = &(*f.arg_begin());

    bool changed = false;

    for(auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      for(auto iter = block->begin(), end = block->end(); iter != end; ++iter)
      {
        Instruction* inst = &(*iter);

        if(runOnInstruction(inst, ctx))
          changed = true;
      }
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool runOnInstruction(Instruction* inst, Value* ctx)
  {
    auto call_base = dyn_cast<CallBase>(inst);
    if (!call_base)
    {
      return false;
    }

    CallBase& call = *call_base;
    Function* fun = call.getCalledFunction();

    if(fun == NULL)
      return false;

    if(fun->getName().compare("pony_ctx") != 0)
      return false;

    inst->replaceAllUsesWith(ctx);

    return true;
  }
};

// A pass to group message sends in the same BasicBlock.
class MergeMessageSend : public PassInfoMixin<MergeMessageSend>
{
public:
  struct MsgFnGroup
  {
    BasicBlock::iterator alloc, trace, done, send;
  };

  enum MsgKind
  {
    MsgNone,
    MsgNoTrace,
    MsgTrace
  };

  compile_t* c;
  Function* send_next_fn;
  Function* msg_chain_fn;
  Function* sendv_single_fn;

  MergeMessageSend(compile_t* compiler) : PassInfoMixin<MergeMessageSend>()
  {
    c = compiler;
    send_next_fn = nullptr;
    msg_chain_fn = nullptr;
    sendv_single_fn = nullptr;
  }

  void doInitialization(Module& m)
  {
    if (send_next_fn != NULL) return;

    send_next_fn = m.getFunction("pony_send_next");
    msg_chain_fn = m.getFunction("pony_chain");
    sendv_single_fn = m.getFunction("pony_sendv_single");

    if(send_next_fn == NULL)
      send_next_fn = declareTraceNextFn(m);

    if(msg_chain_fn == NULL)
      msg_chain_fn = declareMsgChainFn(m);

    return;
  }

  PreservedAnalyses run(Function &f, FunctionAnalysisManager &am)
  {
    doInitialization(*f.getParent());

    bool changed = false;
    for (auto block = f.begin(), end = f.end(); block != end; ++block)
    {
      // |=, not ||: || short-circuits once changed is true, which would skip
      // runOnBasicBlock (and so the merging) for every later block in this
      // invocation.
      changed |= runOnBasicBlock(*block);
    }

    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool runOnBasicBlock(BasicBlock& b)
  {
    auto start = b.begin();
    auto end = b.end();
    bool changed = false;
    SmallVector<CallInst*, 16> sends;

    // This pass is written with the assumption that it is operating on send
    // code instruction sequences that look like they do when coming fresh out
    // of LLVM IR generation in the CodeGen pass.
    // Thus, it is not safe to run this pass on blocks where it has already run,
    // or more specifically, blocks stitched together from combined blocks where
    // the pass has been run on some of the constituent blocks (e.g. a block
    // inlined from an already-optimised function). Re-running over its own
    // output corrupts already-chained sends and trips the makeMsgChains
    // assertions (#3784, #5589).
    // So here we check for the presence of any call to pony_send_next or
    // pony_chain, which are produced nowhere else and so mark a block whose
    // sends this pass has already chained or trace-merged. (A block whose sends
    // were only reordered -- e.g. consecutive non-tracing sends to different
    // destinations -- leaves no such marker, but it is re-run-safe: those sends
    // are never chained, so the makeMsgChains invariant still holds.) If we find
    // a marker, we bail out without modifying the block.
    // This match is by function name and deliberately does NOT go through
    // findCallTo: findCallTo only considers calls tagged with "pony.msgsend"
    // metadata, and the pony_chain calls makeMsgChains creates carry no such
    // metadata. A metadata-filtered check therefore misses chain-only output
    // and lets the pass re-run on an already-merged block.
    bool already_ran = false;
    for(auto iter = start; iter != end; ++iter)
    {
      auto call = dyn_cast<CallBase>(&*iter);
      if(call == NULL)
        continue;

      Function* fun = call->getCalledFunction();
      if((fun != NULL) && (fun->getName().compare("pony_send_next") == 0 ||
        fun->getName().compare("pony_chain") == 0))
      {
        already_ran = true;
        break;
      }
    }
    if(already_ran)
      return false;

    while(start != end)
    {
      MsgFnGroup first;
      MsgKind first_kind = findMsgSend(start, end, true, first);

      switch(first_kind)
      {
        case MsgNone:
          start = end;
          break;

        default:
        {
          sends.push_back(cast<CallInst>(&(*first.send)));
          bool stop = false;

          while(!stop)
          {
            MsgFnGroup next;
            MsgKind next_kind = findMsgSend(std::next(first.send), end, false,
              next);

            switch(next_kind)
            {
              case MsgNone:
                stop = true;
                break;

              default:
                sends.push_back(cast<CallInst>(&(*next.send)));
                mergeMsgSend(first, next, first_kind, next_kind);
                first = next;
                first_kind = next_kind;
                changed = true;
                break;
            }
          }

          sends.push_back(nullptr);
          start = std::next(first.send);
          break;
        }
      }
    }

    if(!sends.empty())
      makeMsgChains(sends);

    return changed;
  }

  MsgKind findMsgSend(BasicBlock::iterator start, BasicBlock::iterator end,
    bool can_pass_writes, MsgFnGroup& out_calls)
  {
    auto alloc = findCallTo("pony_alloc_msg", start, end, can_pass_writes);

    if(alloc == end)
      return MsgNone;

    decltype(alloc) trace;
    size_t fn_index;

    std::tie(trace, fn_index) =
      findCallTo(std::vector<StringRef>{"pony_gc_send", "pony_sendv",
        "pony_sendv_single"}, std::next(alloc), end, true);

    switch(fn_index)
    {
      case (size_t)-1:
        return MsgNone;

      case 0:
        break;

      default:
        out_calls.alloc = alloc;
        out_calls.send = trace;
        return MsgNoTrace;
    }

    auto done = findCallTo("pony_send_done", std::next(trace), end, true);

    if(done == end)
      return MsgNone;

    auto send = findCallTo(std::vector<StringRef>{"pony_sendv",
      "pony_sendv_single"}, std::next(done), end, true).first;

    if(send == end)
      return MsgNone;

    out_calls.alloc = alloc;
    out_calls.trace = trace;
    out_calls.done = done;
    out_calls.send = send;
    return MsgTrace;
  }

  BasicBlock::iterator findCallTo(StringRef name,
    BasicBlock::iterator start, BasicBlock::iterator end, bool can_pass_writes)
  {
    return findCallTo(std::vector<StringRef>{name}, start, end, can_pass_writes).first;
  }

  std::pair<BasicBlock::iterator, size_t> findCallTo(
      std::vector<StringRef> const& names, BasicBlock::iterator start,
    BasicBlock::iterator end, bool can_pass_writes)
  {
    for(auto iter = start; iter != end; ++iter)
    {
      if(iter->getMetadata("pony.msgsend") != NULL)
      {
        auto call_base = dyn_cast<CallBase>(&*iter);
        if (!call_base)
          continue;

        CallBase& call = *call_base;

        Function* fun = call.getCalledFunction();
        pony_assert(fun != NULL);

        for(size_t i = 0; i < names.size(); i++)
        {
          if(fun->getName().compare(names[i]) == 0)
            return {iter, i};
        }
      }

      if(!can_pass_writes && iter->mayWriteToMemory())
        break;
    }

    return {end, -1};
  }

  void mergeMsgSend(MsgFnGroup& first, MsgFnGroup& next, MsgKind first_kind,
    MsgKind& next_kind)
  {
    pony_assert((first_kind != MsgNone) && (next_kind != MsgNone));

    if(first_kind == MsgNoTrace)
    {
      auto src = first.send;
      auto dst = next.send;

      while(1)
      {
        auto prev = std::prev(src);
        src->moveBefore(dst);

        if(prev->getMetadata("pony.msgsend") == NULL)
          break;

        auto call_base = dyn_cast<CallBase>(&*prev);
        if (!call_base)
          break;
        CallBase& call = *call_base;

        auto fun = call.getCalledFunction();
        pony_assert(fun != NULL);

        if(fun->getName().compare("pony_alloc_msg") == 0)
          break;

        dst = src;
        src = prev;
      }
    } else {
      auto iter = first.alloc;

      while(iter != first.trace)
      {
        auto& inst = (*iter++);
        inst.moveBefore(next.alloc);
      }

      if(next_kind == MsgNoTrace)
      {
        auto first_send_post = std::next(first.send);

        while(iter != first_send_post)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(next.send);
        }

        next.trace = first.trace;
        next.done = first.done;
        next_kind = MsgTrace;
      } else {
        while(iter != first.done)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(next.trace);
        }

        iter++;
        first.done->eraseFromParent();

        auto call_base = dyn_cast<CallBase>(&*next.trace);
        CallBase& call = *call_base;

        call.setCalledFunction(send_next_fn);

        auto first_send_post = std::next(first.send);
        auto next_done_post = std::next(next.done);

        while(iter != first_send_post)
        {
          auto& inst = *(iter++);
          if(inst.getOpcode() == Instruction::Call)
            inst.moveBefore(next_done_post);
        }

        next.trace = first.trace;
      }
    }
  }

  bool makeMsgChains(SmallVector<CallInst*, 16> const& sends)
  {
    if(sends.size() < 2)
      return false;

    auto iter = std::begin(sends);
    auto next = std::next(iter);
    Value* chain_start = nullptr;
    bool is_single = false;
    bool changed = false;

    while(true)
    {
      pony_assert(*iter != nullptr);

      if((next == std::end(sends)) || (*next == nullptr))
      {
        if(chain_start != nullptr)
        {
          (*iter)->setArgOperand(2, chain_start);
          chain_start = nullptr;

          if(is_single)
          {
            pony_assert(sendv_single_fn != nullptr);
            (*iter)->setCalledFunction(sendv_single_fn);
            is_single = false;
          }
        }

        if(next == std::end(sends))
          break;

        iter = std::next(next);

        if(iter == std::end(sends))
          break;

        next = std::next(iter);
        continue;
      }

      auto prev_msg = (*iter)->getArgOperand(2);
      auto next_msg = (*next)->getArgOperand(2);

      pony_assert(prev_msg == (*iter)->getArgOperand(3));
      pony_assert(next_msg == (*next)->getArgOperand(3));

      if((*iter)->getArgOperand(1) == (*next)->getArgOperand(1))
      {
        if((*iter)->getCalledFunction() == sendv_single_fn)
          is_single = true;

        auto chain_call = CallInst::Create(msg_chain_fn, {prev_msg, next_msg},
          "", (*iter)->getIterator());
        chain_call->setTailCall();
        (*iter)->eraseFromParent();

        if(chain_start == nullptr)
        {
          chain_start = prev_msg;
          changed = true;
        }
      } else if(chain_start != nullptr) {
        (*iter)->setArgOperand(2, chain_start);
        chain_start = nullptr;

        if(is_single)
        {
          pony_assert(sendv_single_fn != nullptr);
          (*iter)->setCalledFunction(sendv_single_fn);
          is_single = false;
        }
      }

      iter = next;
      next = std::next(iter);
    }

    return changed;
  }

  Function* declareTraceNextFn(Module& m)
  {
    // Two params (ctx, destination actor) to match pony_gc_send: when this pass
    // rewrites a later message's pony_gc_send into pony_send_next via
    // setCalledFunction, the destination operand is preserved so each merged
    // message keeps its own self-send classification.
    FunctionType* fn_type = FunctionType::get(unwrap(c->void_type),
      {unwrap(c->ptr), unwrap(c->ptr)}, false);
    Function* fn = Function::Create(fn_type, Function::ExternalLinkage,
      "pony_send_next", &m);

    fn->addFnAttr(Attribute::NoUnwind);
    // The destination actor (param index 1) is only stored/compared, never
    // dereferenced -- matches pony_gc_send's second-argument attribute.
    fn->addParamAttr(1, Attribute::ReadNone);
    return fn;
  }

  Function* declareMsgChainFn(Module& m)
  {
    FunctionType* fn_type = FunctionType::get(unwrap(c->void_type),
      {unwrap(c->ptr), unwrap(c->ptr)}, false);
    Function* fn = Function::Create(fn_type, Function::ExternalLinkage,
      "pony_chain", &m);

    fn->addFnAttr(Attribute::NoUnwind);
    fn->setOnlyAccessesArgMemory();
    // pony_chain(prev, next) reads and writes *prev (prev->next) but never
    // stores the prev pointer itself, so prev (param 0) does not escape. It
    // stores the next pointer into prev->next -- so next is captured and must
    // not be marked captures(none) -- but never dereferences *next, so next
    // (param 1) is ReadNone. addParamAttr is 0-based.
    fn->addParamAttr(0,
      Attribute::getWithCaptureInfo(m.getContext(), CaptureInfo::none()));
    fn->addParamAttr(1, Attribute::ReadNone);
    return fn;
  }
};

static bool pony_opt_module(compile_t* c, LLVMModuleRef module,
  LLVMDIBuilderRef di)
{
  errors_t* errors = c->opt->check.errors;

  LLVMDIBuilderFinalize(di);

  {
    PassBuilder PB;
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    ModulePassManager MPM;

    if(c->opt->release)
    {
      FunctionPassManager canon;
      canon.addPass(SROAPass(SROAOptions::ModifyCFG));
      canon.addPass(InstCombinePass());
      MPM.addPass(createModuleToFunctionPassAdaptor(std::move(canon)));

      CGSCCPassManager CGPM;
      CGPM.addPass(InlinerPass());
      MPM.addPass(createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));

      FunctionPassManager pony_fn;
      pony_fn.addPass(DispatchPonyCtx());
      pony_fn.addPass(MergeMessageSend(c));
      MPM.addPass(createModuleToFunctionPassAdaptor(std::move(pony_fn)));
    }

    if(c->opt->lint_llvm)
      MPM.addPass(createModuleToFunctionPassAdaptor(LintPass(false)));

    MPM.run(*unwrap(module), MAM);
  }

  if(c->opt->verify)
  {
    size_t mod_name_len = 0;
    const char* mod_name = LLVMGetModuleIdentifier(module, &mod_name_len);
    if(c->opt->verbosity >= VERBOSITY_MINIMAL)
      fprintf(stderr, "Verifying %s\n", mod_name);

    char* msg = NULL;

    if(LLVMVerifyModule(module, LLVMPrintMessageAction, &msg) != 0)
    {
      errorf(errors, NULL, "Module verification failed: %s", msg);
      errorf_continue(errors, NULL,
        "Please file an issue ticket. Use --noverify to bypass this error.");
      LLVMDisposeMessage(msg);
      return false;
    }

    if(msg != NULL)
      LLVMDisposeMessage(msg);
  }

  return true;
}

bool pony_specific_opt(compile_t* c)
{
  if(c->per_module_count > 0)
  {
    for(size_t i = 0; i < c->per_module_count; i++)
    {
      if(!pony_opt_module(c, c->per_module_states[i].module,
        c->per_module_states[i].di))
        return false;
    }
    return true;
  }

  return pony_opt_module(c, c->module, c->di);
}

bool target_is_linux(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSLinux();
}

bool target_is_bsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSDragonFly() || triple.isOSFreeBSD() || triple.isOSOpenBSD();
}

bool target_is_freebsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSFreeBSD();
}

bool target_is_dragonfly(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSDragonFly();
}

bool target_is_openbsd(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSOpenBSD();
}

bool target_is_macosx(char* t)
{
  Triple triple = Triple(t);

  return triple.isMacOSX();
}

bool target_is_windows(char* t)
{
  Triple triple = Triple(t);

  return triple.isOSWindows();
}

bool target_is_posix(char* t)
{
  Triple triple = Triple(t);

  return triple.isMacOSX() || triple.isOSFreeBSD() || triple.isOSLinux()
    || triple.isOSDragonFly() || triple.isOSOpenBSD();
}

bool target_is_x86(char* t)
{
  Triple triple = Triple(t);

  std::string s = Triple::getArchTypePrefix(triple.getArch()).str();
  const char* arch = s.c_str();

  return !strcmp("x86", arch);
}

bool target_is_arm(char* t)
{
  Triple triple = Triple(t);

  std::string s = Triple::getArchTypePrefix(triple.getArch()).str();
  const char* arch = s.c_str();

  return (!strcmp("arm", arch) || !strcmp("aarch64", arch));
}

bool target_is_arm32(char* t)
{
  Triple triple = Triple(t);
  std::string s = Triple::getArchTypePrefix(triple.getArch()).str();
  const char* arch = s.c_str();
  return !strcmp("arm", arch) && target_is_ilp32(t);
}

bool target_is_riscv(char* t)
{
  Triple triple = Triple(t);
  std::string s = Triple::getArchTypePrefix(triple.getArch()).str();
  const char* arch = s.c_str();
  return (!strcmp("riscv32", arch) || !strcmp("riscv64", arch));
}

// This function is used to safeguard against potential oversights on the size
// of Bool on any future port to PPC32.
// We do not currently support compilation to PPC. It could work, but no
// guarantees.
bool target_is_ppc(char* t)
{
  Triple triple = Triple(t);

  std::string s = Triple::getArchTypePrefix(triple.getArch()).str();
  const char* arch = s.c_str();

  return !strcmp("ppc", arch);
}

bool target_is_lp64(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch64Bit() && !triple.isOSWindows();
}

bool target_is_llp64(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch64Bit() && triple.isOSWindows();
}

bool target_is_ilp32(char* t)
{
  Triple triple = Triple(t);

  return triple.isArch32Bit();
}

bool target_is_native128(char* t)
{
  Triple triple = Triple(t);

  return !triple.isArch32Bit() && !triple.isKnownWindowsMSVCEnvironment();
}

bool target_is_bigendian(char* t)
{
  Triple triple = Triple(t);

  return !triple.isLittleEndian();
}

bool target_is_littleendian(char* t)
{
  Triple triple = Triple(t);

  return triple.isLittleEndian();
}

bool is_cross_compiling(pass_opt_t* opt)
{
  char* default_triple_str = LLVMGetDefaultTargetTriple();
  Triple target(opt->triple);
  Triple host(default_triple_str);
  LLVMDisposeMessage(default_triple_str);

  if(target.getArch() != host.getArch())
    return true;

  // Darwin and MacOSX are different Triple::OSType enum values but the same
  // platform. The target triple uses "macosx" (after ponyc normalization)
  // while LLVMGetDefaultTargetTriple returns "darwin"; comparing OS enums
  // directly misidentifies every native macOS build as cross-compilation.
  if(target.isMacOSX() && host.isMacOSX())
    return false;

  return target.getOS() != host.getOS()
    || target.getEnvironment() != host.getEnvironment();
}

// Twin of genexe.cc's gnu_multiarch_arch (keep the two in sync): LLVM reports
// 32-bit little-endian ARM as the raw triple arch ("armv7l", "armv6", etc.),
// but Debian/Ubuntu/Raspbian multiarch directories normalize all of them to
// "arm" (the EABI float variant rides in the environment, not the arch). The
// other arches this linker supports already match LLVM's spelling, so fall
// back to getArchName(). See the fuller explanation on the genexe.cc twin.
static std::string gnu_multiarch_arch(const Triple& triple)
{
  switch(triple.getArch())
  {
    case Triple::arm:
    case Triple::thumb:
      return "arm";
    default:
      return std::string(triple.getArchName());
  }
}

const char* system_triple(pass_opt_t* opt)
{
  Triple triple(opt->triple);
  std::string result = gnu_multiarch_arch(triple) + "-"
    + std::string(triple.getOSName()) + "-"
    + std::string(triple.getEnvironmentName());
  return stringtab(opt->strtab, result.c_str());
}
