#include "gc_calls.h"
#include "../libponyrt/gc/map_format.h"

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/CodeGen/GCMetadataPrinter.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/StackMaps.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/GCStrategy.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/EarlyCSE.h>
#include <llvm/Transforms/Scalar/JumpThreading.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Scalar/SROA.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

using namespace llvm;

namespace {
static_assert((unsigned)StackMaps::Location::Register == GC_MAP_REGISTER);
static_assert((unsigned)StackMaps::Location::Direct == GC_MAP_DIRECT);
static_assert((unsigned)StackMaps::Location::Indirect == GC_MAP_INDIRECT);
static_assert((unsigned)StackMaps::Location::Constant == GC_MAP_CONSTANT);
static_assert(sizeof(gc_map_location_t) == 8 && sizeof(gc_map_site_t) == 24);

constexpr char strategy_name[] = "pony-local-roots";
constexpr char layouts_name[] = "pony.gc.layouts";

class PonyGcStrategy : public GCStrategy
{
public:
  PonyGcStrategy() { UsesMetadata = true; }
};

class PonyGcPrinter : public GCMetadataPrinter
{
public:
  bool emitStackMaps(StackMaps& maps, AsmPrinter& printer) override
  {
    const NamedMDNode* layouts =
      printer.MMI->getModule()->getNamedMetadata(layouts_name);
    if(layouts == nullptr)
      return false;
    MCStreamer& out = *printer.OutStreamer;
    auto site = maps.getCSInfos().begin();
    for(auto& entry : maps.getFnInfos())
    {
      for(uint64_t i = 0; i < entry.second.RecordCount; i++, site++)
      {
        if(site->ID == 0 || site->ID > layouts->getNumOperands())
          continue;
        MDNode* layout = layouts->getOperand((unsigned)site->ID - 1);
        auto* trace = mdconst::dyn_extract<Function>(layout->getOperand(0));
        unsigned fields = mdconst::extract<ConstantInt>(
          layout->getOperand(1))->getZExtValue();
        const auto& locs = site->Locations;
        if(trace == nullptr || fields < 3 || fields > PONY_GC_MAP_ROOTS + 2 ||
          locs.size() != fields + 2 ||
          locs[2].Type != StackMaps::Location::Constant ||
          locs[2].Offset != (int32_t)fields - 1)
          continue;

        out.switchSection(printer.OutContext.getELFSection("pony_gc_sites",
          ELF::SHT_PROGBITS, ELF::SHF_ALLOC | ELF::SHF_GNU_RETAIN));
        out.emitValueToAlignment(Align(8));
        auto relative = [&](const MCSymbol* target) {
          auto* field = printer.OutContext.createTempSymbol();
          out.emitLabel(field);
          printer.emitLabelDifference(target, field, 8);
        };
        relative(entry.first);
        relative(printer.getSymbol(trace));
        out.emitValue(site->CSOffsetExpr, 4);
        out.emitInt32(fields - 1);
        for(auto& loc : ArrayRef(locs).drop_front(3))
        {
          out.emitInt8(loc.Type);
          out.emitInt8(loc.Size);
          out.emitInt16(loc.Reg);
          out.emitInt32(loc.Offset);
        }
      }
    }
    return true;
  }
};

GCRegistry::Add<PonyGcStrategy> strategy(strategy_name, "Pony suspended roots");
GCMetadataPrinterRegistry::Add<PonyGcPrinter> printer(strategy_name,
  "Pony suspended roots");
}

// IPO may merge or move frame stores. Only replace a record whose values
// and trace callback are still uniquely determined at this suspension.
static bool frame_values(AllocaInst* frame, CallInst* identity,
  CallInst* suspend, Function* trace, unsigned count, DominatorTree& tree,
  SmallVectorImpl<Value*>& values)
{
  if(!identity->hasOneUse())
    return false;
  const DataLayout& dl = suspend->getModule()->getDataLayout();
  SmallVector<std::pair<Value*, uint64_t>> work{{frame, 0}};
  SmallVector<StoreInst*> fields(count, nullptr);
  while(!work.empty())
  {
    auto [address, offset] = work.pop_back_val();
    for(User* user : address->users())
    {
      if(user == identity || isa<LifetimeIntrinsic>(user))
        continue;
      if(auto* gep = dyn_cast<GetElementPtrInst>(user))
      {
        APInt delta(64, 0);
        if(!gep->accumulateConstantOffset(dl, delta) || delta.isNegative() ||
          delta.ugt(count * 8) || delta.getZExtValue() + offset >= count * 8)
          return false;
        work.emplace_back(gep, offset + delta.getZExtValue());
        continue;
      }
      auto* store = dyn_cast<StoreInst>(user);
      if(store == nullptr || store->getPointerOperand() != address ||
        store->getValueOperand() == address || !store->isSimple() ||
        !store->getValueOperand()->getType()->isPointerTy() || offset % 8 ||
        fields[offset / 8] != nullptr || !tree.dominates(store, suspend))
        return false;
      fields[offset / 8] = store;
    }
  }
  if(llvm::is_contained(fields, nullptr) || fields[1]->getValueOperand() != trace)
    return false;
  for(unsigned i = 0; i < count; i++)
    if(i != 1)
      values.push_back(fields[i]->getValueOperand());
  return true;
}

static bool scalar_result(Type* t)
{
  return t->isVoidTy() || t->isPointerTy() ||
    (t->isIntegerTy() && t->getIntegerBitWidth() <= 64);
}

namespace pony {

static bool lower_gc_maps(Module& module, ModuleAnalysisManager& mam,
  const SmallPtrSetImpl<Function*>& collecting)
{
  if(collecting.empty() || module.getTargetTriple().getArch() != Triple::x86_64 ||
    !module.getTargetTriple().isOSLinux() ||
    module.getDataLayout().getPointerSize() != sizeof(uint64_t))
    return false;

  SmallVector<CallInst*> sites;
  SmallVector<GlobalValue*> traces;
  SmallPtrSet<Function*, 16> changed;
  for(Function& caller : module)
  {
    if(caller.hasGC() || caller.getIntrinsicID() == Intrinsic::experimental_stackmap ||
      caller.getIntrinsicID() == Intrinsic::experimental_patchpoint ||
      caller.getIntrinsicID() == Intrinsic::experimental_gc_statepoint)
      return false;
    for(BasicBlock& bb : caller)
      for(Instruction& inst : bb)
      {
        auto* cb = dyn_cast<CallBase>(&inst);
        if(cb != nullptr && cb->getOperandBundle("deopt"))
          return false;
        if(auto* call = dyn_cast<CallInst>(&inst))
          if(call->getMetadata("pony.gc.frame") != nullptr)
            sites.push_back(call);
      }
  }
  if(sites.empty())
    return false;
  auto& fam = mam.getResult<FunctionAnalysisManagerModuleProxy>(module)
    .getManager();
  for(CallInst* call : sites)
  {
    Function* callee = call->getCalledFunction();
    if(!scalar_result(call->getType()) ||
      !scalar_result(call->getFunction()->getReturnType()))
      continue;
    if(!collecting.contains(callee) || callee->isDeclaration() ||
      call->hasStructRetAttr() ||
      call->hasOperandBundles() || call->isMustTailCall() || call->arg_empty())
      continue;
    MDNode* layout = call->getMetadata("pony.gc.frame");
    auto* trace = mdconst::dyn_extract_or_null<Function>(layout->getOperand(0));
    if(trace == nullptr)
      continue;
    unsigned last = call->arg_size() - 1;
    if(callee->getParamAlign(last) || callee->getParamDereferenceableBytes(last) ||
      callee->getParamDereferenceableOrNullBytes(last))
      continue;
    Function* caller = call->getFunction();
    if(call->arg_size() > 6 || caller->arg_size() > 6 || caller->isVarArg() ||
      callee->isVarArg() || !llvm::all_of(call->args(), [](const Use& a) {
        Type* t = a->getType();
        return t->isPointerTy() || (t->isIntegerTy() && t->getIntegerBitWidth() <= 64);
      }) ||
      !llvm::all_of(caller->args(), [](Argument& a) {
        Type* t = a.getType();
        return !a.hasByValAttr() && !a.hasStructRetAttr() &&
          (t->isPointerTy() || (t->isIntegerTy() && t->getIntegerBitWidth() <= 64));
      }))
      continue;
    auto* identity = dyn_cast<CallInst>(call->getArgOperand(last));
    if(identity == nullptr || !identity->isInlineAsm() || identity->arg_size() != 1)
      continue;
    auto* asm_fn = cast<InlineAsm>(identity->getCalledOperand());
    if(!asm_fn->getAsmString().empty() || asm_fn->getConstraintString() != "=r,0")
      continue;
    auto* frame = dyn_cast<AllocaInst>(identity->getArgOperand(0));
    auto* type = frame == nullptr ? nullptr :
      dyn_cast<StructType>(frame->getAllocatedType());
    if(type == nullptr || type->getNumElements() < 3 ||
      type->getNumElements() > PONY_GC_MAP_ROOTS + 2 ||
      mdconst::extract<ConstantInt>(layout->getOperand(1))->getZExtValue() !=
        type->getNumElements() ||
      !llvm::all_of(type->elements(), [](Type* t) { return t->isPointerTy(); }))
      continue;

    SmallVector<Value*> values;
    if(!frame_values(frame, identity, call, trace, type->getNumElements(),
      fam.getResult<DominatorTreeAnalysis>(*caller), values))
      continue;
    IRBuilder<> builder(call);
    SmallVector<Value*> roots;
    for(Value* value : values)
      roots.push_back(builder.CreatePtrToInt(value, builder.getInt64Ty()));
    Value* return_slot = builder.CreateIntrinsic(Intrinsic::addressofreturnaddress,
      {builder.getPtrTy()}, {});
    Value* token = builder.CreateGEP(builder.getInt8Ty(), return_slot,
      builder.getInt64(PONY_GC_RETURN_SLOT_TAG));
    OperandBundleDef bundle("deopt", roots);
    auto* mapped = CallInst::Create(call, {bundle}, call->getIterator());
    mapped->setArgOperand(last, token);
    // The token denotes the physical frame containing this return address.
    mapped->setTailCallKind(CallInst::TCK_NoTail);
    mapped->setMetadata("pony.gc.frame", nullptr);
    auto* layouts = module.getOrInsertNamedMetadata(layouts_name);
    layouts->addOperand(layout);
    mapped->addFnAttr(Attribute::get(module.getContext(), "statepoint-id",
      std::to_string(layouts->getNumOperands())));
    mapped->removeParamAttr(last, Attribute::Alignment);
    mapped->removeParamAttr(last, Attribute::Dereferenceable);
    mapped->removeParamAttr(last, Attribute::DereferenceableOrNull);
    call->replaceAllUsesWith(mapped);
    call->eraseFromParent();
    if(identity->use_empty())
      identity->eraseFromParent();
    caller->setGC(strategy_name);
    caller->setUWTableKind(UWTableKind::Async);
    traces.push_back(trace);
    changed.insert(caller);
  }

  if(changed.empty())
    return false;
  appendToCompilerUsed(module, traces);
  for(Function* f : changed)
  {
    fam.invalidate(*f, PreservedAnalyses::none());
    FunctionPassManager cleanup;
    cleanup.addPass(SROAPass(SROAOptions::ModifyCFG));
    cleanup.addPass(InstCombinePass());
    cleanup.addPass(EarlyCSEPass());
    cleanup.addPass(JumpThreadingPass());
    cleanup.addPass(SimplifyCFGPass());
    cleanup.run(*f, fam);
  }
  return true;
}

PreservedAnalyses LowerGcMaps::run(Module& module, ModuleAnalysisManager& mam)
{
  SmallPtrSet<Function*, 32> collecting;
  SmallPtrSet<GlobalVariable*, 8> tables;
  for(GlobalVariable& global : module.globals())
  {
    if(!global.hasInitializer() || !global.getMetadata("pony.gc.pairs"))
      continue;
    auto* pairs = dyn_cast<ConstantArray>(global.getInitializer());
    if(pairs == nullptr)
      continue;
    for(unsigned i = 1; i < pairs->getNumOperands(); i += 2)
      if(auto* f = dyn_cast<Function>(pairs->getOperand(i)))
        collecting.insert(f);
    if(global.hasLocalLinkage())
      tables.insert(&global);
  }

  bool changed = lower_gc_maps(module, mam, collecting);
  if(!tables.empty())
  {
    removeFromUsedLists(module, [&](Constant* value) {
      return tables.contains(dyn_cast<GlobalVariable>(value));
    });
    for(GlobalVariable* table : tables)
    {
      table->removeDeadConstantUsers();
      if(table->use_empty())
        table->eraseFromParent();
    }
    changed = true;
  }
  return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
}
