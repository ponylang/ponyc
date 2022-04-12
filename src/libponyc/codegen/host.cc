#include "llvm_config_begin.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Target/TargetOptions.h>

#include "llvm_config_end.h"

#include <stdio.h>
#include "codegen.h"
#include "genopt.h"
#include "ponyassert.h"

using namespace llvm;

LLVMTargetMachineRef codegen_machine(LLVMTargetRef target, pass_opt_t* opt)
{
  Optional<Reloc::Model> reloc;

  if(opt->pic)
    reloc = Reloc::PIC_;

  // The Arm debug fix is a "temporary" fix for issue #3874
  // https://github.com/ponylang/ponyc/issues/3874
  // Hopefully we get #3874 figured out in a reasonable amount of time.
  CodeGenOpt::Level opt_level =
    opt->release ? CodeGenOpt::Aggressive :
      target_is_arm(opt->triple) ? CodeGenOpt::Default : CodeGenOpt::None;

  TargetOptions options;

  Target* t = reinterpret_cast<Target*>(target);

  TargetMachine* m = t->createTargetMachine(opt->triple, opt->cpu,
    opt->features, options, reloc, llvm::None, opt_level, false);

  return reinterpret_cast<LLVMTargetMachineRef>(m);
}

void LLVMSetUnsafeAlgebra(LLVMValueRef inst)
{
  unwrap<Instruction>(inst)->setHasAllowReassoc(true);
}

void LLVMSetNoUnsignedWrap(LLVMValueRef inst)
{
  unwrap<BinaryOperator>(inst)->setHasNoUnsignedWrap(true);
}

void LLVMSetNoSignedWrap(LLVMValueRef inst)
{
  unwrap<BinaryOperator>(inst)->setHasNoSignedWrap(true);
}

void LLVMSetIsExact(LLVMValueRef inst)
{
  unwrap<BinaryOperator>(inst)->setIsExact(true);
}

LLVMValueRef LLVMConstNaN(LLVMTypeRef type)
{
  Type* t = unwrap<Type>(type);

  Value* nan = ConstantFP::getNaN(t);
  return wrap(nan);
}

LLVMValueRef LLVMConstInf(LLVMTypeRef type, bool negative)
{
  Type* t = unwrap<Type>(type);

  Value* inf = ConstantFP::getInfinity(t, negative);
  return wrap(inf);
}

LLVMModuleRef LLVMParseIRFileInContext(LLVMContextRef ctx, const char* file)
{
  SMDiagnostic diag;

  return wrap(parseIRFile(file, diag, *unwrap(ctx)).release());
}

// From LLVM internals.
static MDNode* extractMDNode(MetadataAsValue* mdv)
{
  Metadata* md = mdv->getMetadata();
  pony_assert(isa<MDNode>(md) || isa<ConstantAsMetadata>(md));

  MDNode* n = dyn_cast<MDNode>(md);
  if(n != NULL)
    return n;

  return MDNode::get(mdv->getContext(), md);
}

bool LLVMHasMetadataStr(LLVMValueRef val, const char* str)
{
  Value* v = unwrap<Value>(val);
  Instruction* i = dyn_cast<Instruction>(v);

  if(i != NULL)
    return i->getMetadata(str) != NULL;

  return cast<Function>(v)->getMetadata(str) != NULL;
}

void LLVMSetMetadataStr(LLVMValueRef val, const char* str, LLVMValueRef node)
{
  pony_assert(node != NULL);

  MDNode* n = extractMDNode(unwrap<MetadataAsValue>(node));
  Value* v = unwrap<Value>(val);
  Instruction* i = dyn_cast<Instruction>(v);

  if(i != NULL)
    i->setMetadata(str, n);
  else
    cast<Function>(v)->setMetadata(str, n);
}

void LLVMMDNodeReplaceOperand(LLVMValueRef parent, unsigned i,
  LLVMValueRef node)
{
  pony_assert(parent != NULL);
  pony_assert(node != NULL);

  MDNode *pn = extractMDNode(unwrap<MetadataAsValue>(parent));
  MDNode *cn = extractMDNode(unwrap<MetadataAsValue>(node));
  pn->replaceOperandWith(i, cn);
}

LLVMValueRef LLVMBuildStructGEP_P(LLVMBuilderRef B, LLVMValueRef Pointer,
  unsigned Idx, const char *Name)
{
  Value *Val = unwrap(Pointer);
  Type *Ty = Val->getType()->getScalarType()->getNonOpaquePointerElementType();
  return wrap(unwrap(B)->CreateStructGEP(Ty, Val, Idx, Name));
}

LLVMValueRef LLVMConstInBoundsGEP_P(LLVMValueRef ConstantVal,
                                  LLVMValueRef *ConstantIndices,
                                  unsigned NumIndices) {
  ArrayRef<Constant *> IdxList(unwrap<Constant>(ConstantIndices, NumIndices),
                               NumIndices);
  Constant *Val = unwrap<Constant>(ConstantVal);
  Type *Ty = Val->getType()->getScalarType()->getNonOpaquePointerElementType();
  return wrap(ConstantExpr::getInBoundsGetElementPtr(Ty, Val, IdxList));
}

LLVMValueRef LLVMBuildInBoundsGEP_P(LLVMBuilderRef B, LLVMValueRef Pointer,
                                  LLVMValueRef *Indices, unsigned NumIndices,
                                  const char *Name) {
  ArrayRef<Value *> IdxList(unwrap(Indices), NumIndices);
  Value *Val = unwrap(Pointer);
  Type *Ty = Val->getType()->getScalarType()->getNonOpaquePointerElementType();
  return wrap(unwrap(B)->CreateInBoundsGEP(Ty, Val, IdxList, Name));
}

LLVMValueRef LLVMBuildLoad_P(LLVMBuilderRef B, LLVMValueRef PointerVal,
                           const char *Name) {
  Value *V = unwrap(PointerVal);
  PointerType *Ty = cast<PointerType>(V->getType());

  return wrap(
      unwrap(B)->CreateLoad(Ty->getNonOpaquePointerElementType(), V, Name));
}

LLVMValueRef LLVMBuildCall_P(LLVMBuilderRef B, LLVMValueRef Fn,
                           LLVMValueRef *Args, unsigned NumArgs,
                           const char *Name) {
  Value *V = unwrap(Fn);
  FunctionType *FnT =
      cast<FunctionType>(V->getType()->getNonOpaquePointerElementType());

  return wrap(unwrap(B)->CreateCall(FnT, unwrap(Fn),
                                    makeArrayRef(unwrap(Args), NumArgs), Name));
}

LLVMValueRef LLVMBuildInvoke_P(LLVMBuilderRef B, LLVMValueRef Fn,
                             LLVMValueRef *Args, unsigned NumArgs,
                             LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch,
                             const char *Name) {
  Value *V = unwrap(Fn);
  FunctionType *FnT =
      cast<FunctionType>(V->getType()->getNonOpaquePointerElementType());

  return wrap(
      unwrap(B)->CreateInvoke(FnT, unwrap(Fn), unwrap(Then), unwrap(Catch),
                              makeArrayRef(unwrap(Args), NumArgs), Name));
}


LLVMValueRef LLVMMemcpy(LLVMModuleRef module, bool ilp32)
{
  Module* m = unwrap(module);

  Type* params[3];
  params[0] = Type::getInt8PtrTy(m->getContext());
  params[1] = params[0];
  params[2] = Type::getIntNTy(m->getContext(), ilp32 ? 32 : 64);

  return wrap(Intrinsic::getDeclaration(m, Intrinsic::memcpy, {params, 3}));
}

LLVMValueRef LLVMMemmove(LLVMModuleRef module, bool ilp32)
{
  Module* m = unwrap(module);

  Type* params[3];
  params[0] = Type::getInt8PtrTy(m->getContext());
  params[1] = params[0];
  params[2] = Type::getIntNTy(m->getContext(), ilp32 ? 32 : 64);

  return wrap(Intrinsic::getDeclaration(m, Intrinsic::memmove, {params, 3}));
}

LLVMValueRef LLVMLifetimeStart(LLVMModuleRef module, LLVMTypeRef type)
{
  Module* m = unwrap(module);

  Type* t[1] = { unwrap(type) };
  return wrap(Intrinsic::getDeclaration(m, Intrinsic::lifetime_start, t));
}

LLVMValueRef LLVMLifetimeEnd(LLVMModuleRef module, LLVMTypeRef type)
{
  Module* m = unwrap(module);

  Type* t[1] = { unwrap(type) };
  return wrap(Intrinsic::getDeclaration(m, Intrinsic::lifetime_end, t));
}
