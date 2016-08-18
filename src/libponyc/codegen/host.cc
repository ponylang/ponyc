#ifdef _MSC_VER
#  pragma warning(push)
//because LLVM IR Builder code is broken: e.g. Instructions.h:521-527
#  pragma warning(disable:4244)
#  pragma warning(disable:4800)
#  pragma warning(disable:4267)
#  pragma warning(disable:4624)
#  pragma warning(disable:4141)
#endif

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/SourceMgr.h>

#if PONY_LLVM < 307
#include <llvm/Support/Host.h>
#endif

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include <stdio.h>
#include "codegen.h"

using namespace llvm;

char* LLVMGetHostCPUName()
{
  return strdup(sys::getHostCPUName().str().c_str());
}

void LLVMSetUnsafeAlgebra(LLVMValueRef inst)
{
  unwrap<Instruction>(inst)->setHasUnsafeAlgebra(true);
}

void LLVMSetReturnNoAlias(LLVMValueRef fun)
{
  unwrap<Function>(fun)->setDoesNotAlias(0);
}

void LLVMSetDereferenceable(LLVMValueRef fun, uint32_t i, size_t size)
{
  Function* f = unwrap<Function>(fun);

  AttrBuilder attr;
  attr.addDereferenceableAttr(size);

  f->addAttributes(i, AttributeSet::get(f->getContext(), i, attr));
}

#if PONY_LLVM >= 307
void LLVMSetDereferenceableOrNull(LLVMValueRef fun, uint32_t i, size_t size)
{
  Function* f = unwrap<Function>(fun);

  AttrBuilder attr;
  attr.addDereferenceableOrNullAttr(size);

  f->addAttributes(i, AttributeSet::get(f->getContext(), i, attr));
}
#endif

#if PONY_LLVM >= 308
void LLVMSetInaccessibleMemOrArgMemOnly(LLVMValueRef fun)
{
  unwrap<Function>(fun)->setOnlyAccessesInaccessibleMemOrArgMem();
}
#endif

#if PONY_LLVM < 307
static fltSemantics const* float_semantics(Type* t)
{
  if (t->isHalfTy())
    return &APFloat::IEEEhalf;
  if (t->isFloatTy())
    return &APFloat::IEEEsingle;
  if (t->isDoubleTy())
    return &APFloat::IEEEdouble;
  if (t->isX86_FP80Ty())
    return &APFloat::x87DoubleExtended;
  if (t->isFP128Ty())
    return &APFloat::IEEEquad;

  assert(t->isPPC_FP128Ty() && "Unknown FP format");
  return &APFloat::PPCDoubleDouble;
}
#endif

LLVMValueRef LLVMConstNaN(LLVMTypeRef type)
{
  Type* t = unwrap<Type>(type);

#if PONY_LLVM >= 307
  Value* nan = ConstantFP::getNaN(t);
#else
  fltSemantics const& sem = *float_semantics(t->getScalarType());
  APFloat flt = APFloat::getNaN(sem, false, 0);
  Value* nan = ConstantFP::get(t->getContext(), flt);
#endif

  return wrap(nan);
}

#if PONY_LLVM < 308
static LLVMContext* unwrap(LLVMContextRef ctx)
{
  return reinterpret_cast<LLVMContext*>(ctx);
}
#endif

LLVMModuleRef LLVMParseIRFileInContext(LLVMContextRef ctx, const char* file)
{
  SMDiagnostic diag;

  return wrap(parseIRFile(file, diag, *unwrap(ctx)).release());
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

LLVMValueRef LLVMLifetimeStart(LLVMModuleRef module)
{
  Module* m = unwrap(module);

  return wrap(Intrinsic::getDeclaration(m, Intrinsic::lifetime_start, {}));
}

LLVMValueRef LLVMLifetimeEnd(LLVMModuleRef module)
{
  Module* m = unwrap(module);

  return wrap(Intrinsic::getDeclaration(m, Intrinsic::lifetime_end, {}));
}

LLVMValueRef LLVMInvariantStart(LLVMModuleRef module)
{
  Module* m = unwrap(module);

  return wrap(Intrinsic::getDeclaration(m, Intrinsic::invariant_start, {}));
}
