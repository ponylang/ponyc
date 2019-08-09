#include "genjit.h"
#include "genexe.h"
#include "genopt.h"

#if PONY_LLVM >= 700

#include "llvm_config_begin.h"
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/JITSymbol.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#  include <llvm/ExecutionEngine/Orc/Core.h>
#  include <llvm/ExecutionEngine/Orc/Legacy.h>
#  include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#  include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#  include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#  include <llvm/IR/DataLayout.h>
#  include <llvm/IR/Mangler.h>
#  include <llvm/Support/Error.h>
#  include <llvm/Target/TargetMachine.h>
#include "llvm_config_end.h"

using namespace llvm;
using namespace llvm::orc;

#if PONY_LLVM >= 800

class PonyJIT
{
  ExecutionSession _es;
  RTDyldObjectLinkingLayer _obj_layer;
  IRCompileLayer _compile_layer;

  DataLayout _dl;
  MangleAndInterner _mangle;
  ThreadSafeContext _ctx;

public:
  PonyJIT(JITTargetMachineBuilder jtmb, DataLayout dl) :
    _es(),
    _obj_layer(_es, []() { return llvm::make_unique<SectionMemoryManager>();}),
    _compile_layer(_es, _obj_layer, ConcurrentIRCompiler(std::move(jtmb))),
    _dl(std::move(dl)),
    _mangle(_es, _dl),
    _ctx(llvm::make_unique<LLVMContext>())
  {
#if PONY_LLVM < 900
    _es.getMainJITDylib().setGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(_dl)));
#else
    _es.getMainJITDylib().setGenerator(
      cantFail(DynamicLibrarySearchGenerator::GetForCurrentProcess(_dl.getGlobalPrefix())));
#endif

#if (PONY_LLVM >= 800) && defined(PLATFORM_IS_WINDOWS)
    _obj_layer.setOverrideObjectFlagsWithResponsibilityFlags(true);
#endif
  }

  static Expected<std::unique_ptr<PonyJIT>> create()
  {
    auto jtmb = JITTargetMachineBuilder::detectHost();
    if (!jtmb)
      return jtmb.takeError();

    auto dl = jtmb->getDefaultDataLayoutForTarget();
    if (!dl)
      return dl.takeError();

    return llvm::make_unique<PonyJIT>(std::move(*jtmb), std::move(*dl));
  }

  Error addModule(std::unique_ptr<Module> module)
  {
    return _compile_layer.add(_es.getMainJITDylib(),
      ThreadSafeModule(std::move(module), _ctx));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef name)
  {
    return _es.lookup({&_es.getMainJITDylib()}, _mangle(name.str()));
  }

  JITTargetAddress getSymbolAddress(const char* name)
  {
    auto symbol = lookup(name);
    if (!symbol)
      return 0;

    return symbol->getAddress();
  }
};

#else

class PonyJIT
{
  ExecutionSession _es;
  std::shared_ptr<SymbolResolver> _sr;
  std::unique_ptr<TargetMachine> _tm;
  const DataLayout _dl;
  RTDyldObjectLinkingLayer _obj_layer;
  IRCompileLayer<decltype(_obj_layer), SimpleCompiler> _compile_layer;

public:
  Error error;

  PonyJIT() :
    _es(),
    _sr(createLegacyLookupResolver(_es,
      [this](const std::string& name) -> JITSymbol
      {
        auto symbol = _compile_layer.findSymbol(name, false);
        if (symbol) return symbol;

        auto err = symbol.takeError();
        if (err) return std::move(err);

        auto symaddr = RTDyldMemoryManager::getSymbolAddressInProcess(name);
        if (symaddr)
          return JITSymbol(symaddr, JITSymbolFlags::Exported);

        return nullptr;
      },
      [this](Error err)
      {
        error = std::move(err);
      })),
    _tm(EngineBuilder().selectTarget()),
    _dl(_tm->createDataLayout()),
    _obj_layer(_es,
      [this](VModuleKey)
      {
        return RTDyldObjectLinkingLayer::Resources
        {
          std::make_shared<SectionMemoryManager>(), _sr
        };
      }),
    _compile_layer(_obj_layer, SimpleCompiler(*_tm)),
    error(Error::success())
  {
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
  }

  VModuleKey addModule(std::unique_ptr<Module> module)
  {
    VModuleKey key = _es.allocateVModule();
    cantFail(_compile_layer.addModule(key, std::move(module)));
    return key;
  }

  JITTargetAddress getSymbolAddress(const std::string& name)
  {
    std::string mangled;
    raw_string_ostream mangled_stream(mangled);
    Mangler::getNameWithPrefix(mangled_stream, name, _dl);
    JITSymbol symbol = _compile_layer.findSymbol(mangled_stream.str(), false);
    return cantFail(symbol.getAddress());
  }
};
#endif

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

#if PONY_LLVM >= 800
  auto jit = cantFail(PonyJIT::create());

  auto err = jit->addModule(std::unique_ptr<Module>(unwrap(c->module)));
  if (err)
  {
    errorf(c->opt->check.errors, nullptr, "LLVM ORC JIT Error");
    return false;
  }
  c->module = nullptr;

#else
  auto jit = llvm::make_unique<PonyJIT>();
  jit->addModule(std::unique_ptr<Module>(llvm::unwrap(c->module)));
  c->module = nullptr;

  if (jit->error)
  {
    errorf(c->opt->check.errors, nullptr, "LLVM ORC JIT Error");
    return false;
  }
#endif

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

#else

#include "llvm_config_begin.h"
#  include <llvm/IR/Mangler.h>
#  include <llvm/IR/Module.h>
#  include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#  include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#  include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#  if PONY_LLVM >= 500
#    include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#  else
#    include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#  endif
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include "llvm_config_end.h"

namespace orc = llvm::orc;

#if PONY_LLVM >= 500
using LinkingLayerType = orc::RTDyldObjectLinkingLayer;
using CompileLayerType = orc::IRCompileLayer<LinkingLayerType,
  orc::SimpleCompiler>;
using ModuleHandleType = CompileLayerType::ModuleHandleT;
#else
using LinkingLayerType = orc::ObjectLinkingLayer<orc::DoNothingOnNotifyLoaded>;
using CompileLayerType = orc::IRCompileLayer<LinkingLayerType>;
using ModuleHandleType = CompileLayerType::ModuleSetHandleT;
#endif

static std::string mangle_symbol(llvm::Mangler& mangler,
  const llvm::GlobalValue* val)
{
  std::string mangled{};
  llvm::raw_string_ostream mangle_stream{mangled};
  mangler.getNameWithPrefix(mangle_stream, val, false);
  mangle_stream.flush();
  return mangled;
}

static void* find_symbol(llvm::Module& module, CompileLayerType& compile_layer,
  ModuleHandleType handle, llvm::Mangler& mangler, std::string const& name)
{
  const llvm::GlobalValue* val = module.getNamedGlobal(name);

  if(val == nullptr)
    val = module.getFunction(name);

  if(val == nullptr)
    return nullptr;

  auto local_symbol = compile_layer.findSymbolIn(handle,
   mangle_symbol(mangler, val), false);

  if(!local_symbol)
    return nullptr;

  auto address = local_symbol.getAddress();

#if PONY_LLVM >= 500
  if(!address)
    return nullptr;

  return reinterpret_cast<void*>(address.get());
#else
  return reinterpret_cast<void*>(address);
#endif
}

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

  // The Orc JIT wants the module in a shared_ptr, but we don't want to transfer
  // ownership to that shared_ptr. Use an empty deleter so that the module
  // doesn't get freed when the shared_ptr is destroyed.
  auto noop_deleter = [](void* p){ (void)p; };
  std::shared_ptr<llvm::Module> module{llvm::unwrap(c->module), noop_deleter};

  auto machine = reinterpret_cast<llvm::TargetMachine*>(c->machine);

  auto mem_mgr = std::make_shared<llvm::SectionMemoryManager>();

  LinkingLayerType linking_layer{
#if PONY_LLVM >= 500
    [&mem_mgr]{ return mem_mgr; }
#endif
  };

  CompileLayerType compile_layer{linking_layer, orc::SimpleCompiler{*machine}};

  auto local_lookup = [&compile_layer](llvm::StringRef name)
  {
#if PONY_LLVM >= 400
    return compile_layer.findSymbol(name, false);
#else
    if(auto sym = compile_layer.findSymbol(name, false))
      return sym.toRuntimeDyldSymbol();

    return llvm::RuntimeDyld::SymbolInfo{nullptr};
#endif
  };

  auto external_lookup = [
#if PONY_LLVM >= 500
    &mem_mgr
#endif
  ](llvm::StringRef name)
  {
#if PONY_LLVM >= 500
    return mem_mgr->findSymbol(name);
#else
#  if PONY_LLVM >= 400
    using SymType = llvm::JITSymbol;
#  else
    using SymType = llvm::RuntimeDyld::SymbolInfo;
#  endif

    if(auto sym = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(name))
      return SymType{sym, llvm::JITSymbolFlags::Exported};

    return SymType{nullptr};
#endif
  };

  auto resolver = orc::createLambdaResolver(local_lookup, external_lookup);
#if PONY_LLVM >= 500
  auto maybe_handle = compile_layer.addModule(module, resolver);

  if(!maybe_handle)
    return false;

  auto handle = maybe_handle.get();
#else
  std::vector<decltype(module)> module_set{module};
  auto handle = compile_layer.addModuleSet(std::move(module_set), mem_mgr,
    std::move(resolver));
#endif

  llvm::Mangler mangler{};

  for(size_t i = 0; i < symbol_count; i++)
  {
    auto address = find_symbol(*module, compile_layer, handle, mangler,
      symbols[i].name);

    if(address == nullptr)
      return false;

    memcpy(symbols[i].address, address, symbols[i].size);
  }

  auto local_main = reinterpret_cast<int(*)(int, const char**, const char**)>(
    find_symbol(*module, compile_layer, handle, mangler, "main"));

  if(local_main == nullptr)
    return false;

  const char* argv[] = {"ponyjit", nullptr};
  const char* envp = NULL;

  int ec = local_main(1, argv, &envp);

  if(exit_code != nullptr)
    *exit_code = ec;

  return true;
}

#endif
