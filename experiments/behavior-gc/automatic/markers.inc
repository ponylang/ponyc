static bool marker(const Instruction* inst, StringRef name)
{
  auto* call = dyn_cast_or_null<CallInst>(inst);
  return call && call->getMetadata("pony.gc.frame") &&
    call->getCalledFunction() && call->getCalledFunction()->getName() == name;
}

static CallInst* wrappedCall(CallInst* push)
{
  auto* call = dyn_cast_or_null<CallInst>(push->getNextNode());
  if(!call || call->arg_size() != 1 ||
    call->getArgOperand(0) != push->getArgOperand(0))
    return nullptr;
  auto* pop = dyn_cast_or_null<CallInst>(call->getNextNode());
  if(!marker(pop, "ponyint_frame_pop") || pop->getArgOperand(0) != push)
    return nullptr;

  auto* target = call->getCalledFunction();
  if(!target || target->isDeclaration() || target == push->getFunction())
    return nullptr;
  StringRef expected;
  if(!getConstantStringInfo(push->getArgOperand(3), expected))
    return nullptr;
  for(auto& inst : target->getEntryBlock())
  {
    if(!marker(&inst, "ponyint_frame_claim"))
      continue;
    auto& claim = cast<CallInst>(inst);
    StringRef name;
    if(claim.getArgOperand(0) == target->getArg(0) &&
      getConstantStringInfo(claim.getArgOperand(1), name) && name == expected)
      return call;
  }
  return nullptr;
}

static void discardMarkers(Module& m)
{
  SmallVector<CallInst*> discard;
  for(auto& fn : m)
    for(auto& block : fn)
      for(auto& inst : block)
        if(inst.getMetadata("pony.gc.frame"))
          discard.push_back(cast<CallInst>(&inst));
  for(auto* call : discard)
  {
    call->replaceAllUsesWith(ConstantInt::get(call->getType(), 0));
    call->eraseFromParent();
  }
}
