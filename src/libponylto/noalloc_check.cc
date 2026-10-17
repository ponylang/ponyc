#include "noalloc_check.h"

#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/DenseMap.h>

using namespace llvm;

namespace {

static const char* const alloc_names[] = {
  "pony_alloc",
  "pony_alloc_small",
  "pony_alloc_large",
  "pony_alloc_final",
  "pony_alloc_small_final",
  "pony_alloc_large_final",
  "pony_realloc",
  "pony_alloc_msg",
};

static constexpr size_t alloc_names_count =
  sizeof(alloc_names) / sizeof(alloc_names[0]);

static bool is_alloc_function(StringRef name)
{
  for(size_t i = 0; i < alloc_names_count; i++)
  {
    if(name == alloc_names[i])
      return true;
  }

  return false;
}

static const char* user_facing_name(StringRef name)
{
  if(name == "pony_alloc_msg")
    return "a message send";

  return "a heap allocation";
}

struct CacheEntry
{
  enum Status { NoAlloc, Allocates, Checking };

  Status status;
  CallBase* alloc_call;
  const char* alloc_description;
};

struct AllocInfo
{
  CallBase* call;
  const char* description;
};

static bool check_function(Function &F,
  DenseMap<Function*, CacheEntry> &cache, AllocInfo &found)
{
  auto it = cache.find(&F);

  if(it != cache.end())
  {
    if(it->second.status == CacheEntry::Checking)
      return false;

    if(it->second.status == CacheEntry::Allocates)
    {
      found.call = it->second.alloc_call;
      found.description = it->second.alloc_description;
      return true;
    }

    return false;
  }

  cache[&F] = {CacheEntry::Checking, nullptr, nullptr};

  for(auto &BB : F)
  {
    for(auto &I : BB)
    {
      auto *CB = dyn_cast<CallBase>(&I);

      if(CB == nullptr)
        continue;

      Function *callee = CB->getCalledFunction();

      if(callee == nullptr)
        continue;

      if(is_alloc_function(callee->getName()))
      {
        found.call = CB;
        found.description = user_facing_name(callee->getName());
        cache[&F] = {CacheEntry::Allocates, CB, found.description};
        return true;
      }

      if(callee->isDeclaration())
        continue;

      if(callee->getMetadata("pony.noalloc") != nullptr)
        continue;

      if(check_function(*callee, cache, found))
      {
        cache[&F] = {CacheEntry::Allocates, found.call, found.description};
        return true;
      }
    }
  }

  cache[&F] = {CacheEntry::NoAlloc, nullptr, nullptr};
  return false;
}

} // anonymous namespace

namespace pony {

PreservedAnalyses NoAllocCheck::run(Module &M, ModuleAnalysisManager &)
{
  DenseMap<Function*, CacheEntry> cache;

  for(auto &F : M)
  {
    if(F.isDeclaration())
      continue;

    if(F.getMetadata("pony.noalloc") == nullptr)
      continue;

    AllocInfo found = {};

    if(check_function(F, cache, found))
    {
      std::string msg = "function '";
      msg += F.getName();
      msg += "' is marked \\noalloc\\ but performs ";
      msg += found.description;

      DiagnosticInfoGeneric diag(found.call, Twine(msg), DS_Error);
      F.getContext().diagnose(diag);
    }
  }

  return PreservedAnalyses::all();
}

} // namespace pony
