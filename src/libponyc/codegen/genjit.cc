#include "genjit.h"
#include "genexe.h"
#include "genopt.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Mangler.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#if PONY_LLVM >= 500
#  include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#else
#  include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#endif
#include <llvm/ExecutionEngine/SectionMemoryManager.h>

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

  if((t_main == NULL) || (t_env == NULL))
    return false;

  gen_main(c, t_main, t_env);

  if(!genopt(c, true))
    return false;

  if(LLVMVerifyModule(c->module, LLVMReturnStatusAction, NULL) != 0)
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
