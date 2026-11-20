static void addThresholdCheck(CallInst* poll, Value* outer, Value* inner)
{
  IRBuilder<> builder(poll);
  auto* m = poll->getModule();
  auto* actor = poll->getArgOperand(0);
  auto* ptr = actor->getType();
  auto due = m->getOrInsertFunction("ponyint_threshold_due",
    builder.getInt32Ty(), ptr);
  auto* check = builder.CreateCall(due, {actor});
  auto* taken = SplitBlockAndInsertIfThen(
    builder.CreateICmpNE(check, builder.getInt32(0)), poll, false);
  builder.SetInsertPoint(taken);
  auto collect = m->getOrInsertFunction("ponyint_threshold_collect",
    builder.getVoidTy(), ptr, ptr, ptr);
  auto* first = builder.CreateLoad(ptr, outer);
  auto* second = builder.CreateLoad(ptr, inner);
  builder.CreateCall(collect, {actor, first, second});
}

static void specializeChain(CallInst* outer, bool checks)
{
  auto* middle = wrappedCall(outer);
  InlineFunctionInfo first;
  if(!middle || !InlineFunction(*middle, first).isSuccess())
    return;
  for(auto* entry : first.InlinedCallSites)
  {
    if(!marker(entry, "ponyint_frame_push"))
      continue;
    auto* inner = cast<CallInst>(entry);
    auto* phase = wrappedCall(inner);
    InlineFunctionInfo second;
    if(!phase || !InlineFunction(*phase, second).isSuccess())
      continue;
    for(auto* call : second.InlinedCallSites)
    {
      if(checks && marker(call, "ponyint_frame_poll"))
        addThresholdCheck(cast<CallInst>(call), outer->getArgOperand(2),
          inner->getArgOperand(2));
    }
    return;
  }
}

static void selectEntryPaths(Module& m, ArrayRef<CallInst*> starts)
{
  DenseMap<Function*, Function*> armed;
  SmallVector<Function*> originals;
  for(auto& fn : m)
    if(!fn.isDeclaration())
      for(auto& inst : fn.getEntryBlock())
        if(marker(&inst, "ponyint_frame_claim"))
        {
          originals.push_back(&fn);
          break;
        }

  for(auto* fn : originals)
  {
    ValueToValueMapTy values;
    auto* copy = CloneFunction(fn, values);
    copy->setName(fn->getName() + ".armed");
    copy->setLinkage(GlobalValue::InternalLinkage);
    copy->setComdat(nullptr);
    armed[fn] = copy;
  }
  for(auto& pair : armed)
    for(auto& block : *pair.second)
      for(auto& inst : block)
      {
        auto* call = dyn_cast<CallInst>(&inst);
        if(!call || !call->getCalledFunction())
          continue;
        if(marker(call, "ponyint_frame_push"))
        {
          IRBuilder<> b(call);
          call->setArgOperand(2, b.CreateLoad(b.getPtrTy(), call->getArgOperand(2)));
        }
        call->setMetadata("pony.gc.frame", nullptr);
        auto found = armed.find(call->getCalledFunction());
        if(found != armed.end())
          call->setCalledFunction(found->second);
      }

  for(auto* push : starts)
  {
    auto* plain = wrappedCall(push);
    if(!plain || !armed.count(plain->getCalledFunction()))
      continue;
    auto* pop = cast<CallInst>(plain->getNextNode());
    IRBuilder<> b(plain);
    auto* actor = push->getArgOperand(0);
    auto due = m.getOrInsertFunction("ponyint_threshold_due", b.getInt32Ty(),
      actor->getType());
    auto* check = b.CreateCall(due, {actor});
    Instruction* slow;
    Instruction* fast;
    SplitBlockAndInsertIfThenElse(b.CreateICmpNE(check, b.getInt32(0)),
      plain, &slow, &fast);
    b.SetInsertPoint(slow);
    auto* root = b.CreateLoad(b.getPtrTy(), push->getArgOperand(2));
    auto* ticket = b.CreateCall(push->getCalledFunction(),
      {actor, push->getArgOperand(1), root, push->getArgOperand(3)});
    auto* copy = cast<CallInst>(plain->clone());
    copy->setCalledFunction(armed[plain->getCalledFunction()]);
    b.Insert(copy);
    b.CreateCall(pop->getCalledFunction(), {ticket});
    auto* merge = plain->getParent();
    plain->moveBefore(fast->getIterator());
    b.SetInsertPoint(merge, merge->begin());
    auto* value = b.CreatePHI(plain->getType(), 2);
    plain->replaceAllUsesWith(value);
    value->addIncoming(plain, fast->getParent());
    value->addIncoming(copy, slow->getParent());
  }
}

static void lowerThresholdPolls(Module& m)
{
  SmallVector<CallInst*> starts;
  for(auto& fn : m)
    for(auto& block : fn)
      for(auto& inst : block)
      {
        if(!marker(&inst, "ponyint_frame_push"))
          continue;
        auto& push = cast<CallInst>(inst);
        auto* permit = dyn_cast<ConstantInt>(push.getArgOperand(1));
        if(permit && permit->isMinusOne())
          starts.push_back(&push);
      }

  bool checks = getenv("GC_FRAME_INLINE_CONTROL") == nullptr;
  if(getenv("GC_FRAME_ENTRY_THRESHOLD") != nullptr)
    selectEntryPaths(m, starts);
  else
    for(auto* start : starts)
      specializeChain(start, checks);

  discardMarkers(m);
}
