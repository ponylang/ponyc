#include "codegen.h"

#if PONY_LLVM >= 1200

#include "genjit.h"
#include "genexe.h"
#include "genopt.h"

#include "llvm_config_begin.h"
#  include "llvm/ADT/StringRef.h"
#  include "llvm/ExecutionEngine/JITSymbol.h"
#  include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#  include "llvm/ExecutionEngine/Orc/Core.h"
#  include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#  include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#  include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#  include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#  include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#  include "llvm/ExecutionEngine/Orc/TargetProcessControl.h"
#  include "llvm/ExecutionEngine/SectionMemoryManager.h"
#  include "llvm/IR/DataLayout.h"
#  include "llvm/IR/LLVMContext.h"
#  include "llvm/IR/LegacyPassManager.h"
#  include "llvm/Transforms/InstCombine/InstCombine.h"
#  include "llvm/Transforms/Scalar.h"
#  include "llvm/Transforms/Scalar/GVN.h"
#  include <memory>
#include "llvm_config_end.h"

using namespace llvm;
using namespace llvm::orc;

#define MAIN_DYLIB "<main>"

class PonyJIT
{
private:
  std::unique_ptr<TargetProcessControl> _tpc;
  std::unique_ptr<ExecutionSession> _es;

  DataLayout _dl;
  MangleAndInterner _mangle;

  RTDyldObjectLinkingLayer _object_layer;
  IRCompileLayer _compile_layer;
  IRTransformLayer _optimize_layer;

  JITDylib& _main_dylib;
  ThreadSafeContext _ctx;

public:
  PonyJIT(std::unique_ptr<TargetProcessControl> tpc,
    std::unique_ptr<ExecutionSession> es, JITTargetMachineBuilder jtmb,
    DataLayout dl)
  : _tpc(std::move(tpc)), _es(std::move(es)), _dl(std::move(dl)),
    _mangle(*this->_es, _dl),
    _object_layer(*this->_es,
      []() { return std::make_unique<SectionMemoryManager>(); }),
    _compile_layer(*this->_es, _object_layer,
      std::make_unique<ConcurrentIRCompiler>(std::move(jtmb))),
    _optimize_layer(*this->_es, _compile_layer, optimizeModule),
    _main_dylib(_es->createBareJITDylib(MAIN_DYLIB)),
    _ctx(std::make_unique<LLVMContext>())
  {
    _main_dylib.addGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(
        _dl.getGlobalPrefix())));
  }

  ~PonyJIT()
  {
    if (auto err = _es->endSession())
      _es->reportError(std::move(err));
  }

  static Expected<std::unique_ptr<PonyJIT>> create()
  {
    auto ssp = std::make_shared<SymbolStringPool>();
    auto tpc = SelfTargetProcessControl::Create(ssp);
    if (!tpc)
      return tpc.takeError();

    auto es = std::make_unique<ExecutionSession>(std::move(ssp));

    JITTargetMachineBuilder jtmb((*tpc)->getTargetTriple());

    auto dl = jtmb.getDefaultDataLayoutForTarget();
    if (!dl)
      return dl.takeError();

    return std::make_unique<PonyJIT>(std::move(*tpc), std::move(es),
      std::move(jtmb), std::move(*dl));
  }

  Error addModule(std::unique_ptr<Module> m, ResourceTrackerSP rt = nullptr)
  {
    if (!rt)
      rt = _main_dylib.getDefaultResourceTracker();
    return _optimize_layer.add(rt, ThreadSafeModule(std::move(m), _ctx));
  }

  Expected<JITEvaluatedSymbol> getSymbol(StringRef name)
  {
    return _es->lookup({ &_main_dylib }, _mangle(name.str()));
  }

  JITTargetAddress getSymbolAddress(const char* name)
  {
    auto symbol = getSymbol(name);
    if (!symbol)
      return 0;

    return symbol->getAddress();
  }

private:
  static Expected<ThreadSafeModule> optimizeModule(ThreadSafeModule tsm,
    const MaterializationResponsibility& r)
  {
    tsm.withModuleDo([](Module& m)
      {
        auto fpm = std::make_unique<legacy::FunctionPassManager>(&m);

        for (auto& f : m)
          fpm->run(f);
      });

    return std::move(tsm);
  }
};

bool gen_jit_and_run(compile_t* c, int* exit_code, jit_symbol_t* symbols,
  size_t symbol_count)
{
  reach_type_t* t_main = reach_type_name(c->reach, "Main");
  reach_type_t* t_env = reach_type_name(c->reach, "Env");

  if ((t_main == NULL) || (t_env == NULL))
    return false;

  gen_main(c, t_main, t_env);

  if (!genopt(c, true))
    return false;

  if (LLVMVerifyModule(c->module, LLVMReturnStatusAction, NULL) != 0)
    return false;

  auto jit = cantFail(PonyJIT::create());

  std::unique_ptr<Module> m(unwrap(c->module));
  auto err = jit->addModule(std::move(m));
  if (err)
  {
    errorf(c->opt->check.errors, nullptr, "LLVM ORC JIT Error");
    return false;
  }
  c->module = nullptr; // the module will be deleted by the PonyJIT object

  for (size_t i = 0; i < symbol_count; i++)
  {
    void* address = (void*)jit->getSymbolAddress(symbols[i].name);
    if (address == nullptr)
      return false;

    memcpy(symbols[i].address, address, symbols[i].size);
  }

  auto main = reinterpret_cast<int(*)(int, const char**, const char**)>(
    jit->getSymbolAddress("main")
  );

  if (main == nullptr)
    return false;

  const char* argv[] = { "ponyjit", nullptr };
  const char* envp = nullptr;

  int ec = main(1, argv, &envp);

  if (exit_code != nullptr)
    *exit_code = ec;
  return true;
}

#endif
