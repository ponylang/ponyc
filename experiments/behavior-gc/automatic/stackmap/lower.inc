static void retainNativeFrame(Function& f)
{
  f.removeFnAttr(Attribute::AlwaysInline);
  f.addFnAttr(Attribute::NoInline);
  f.addFnAttr("frame-pointer", "all");
  f.addFnAttr("disable-tail-calls", "true");
  f.setUWTableKind(UWTableKind::Async);
}

static void emitMap(IRBuilder<>& b, unsigned id, Value* slot = nullptr)
{
  auto* m = b.GetInsertBlock()->getModule();
  auto* map = Intrinsic::getOrInsertDeclaration(m, Intrinsic::experimental_stackmap);
  SmallVector<Value*> args{b.getInt64(id), b.getInt32(0)};
  if(slot != nullptr)
    args.push_back(slot);
  b.CreateCall(map, args);
}

static void lowerThresholdPolls(Module& m)
{
  const char* control = getenv("GC_STACKMAP_CONTROL");
  bool maps = control == nullptr || strcmp(control, "check") != 0;
  bool check = control == nullptr || strcmp(control, "maps") != 0;
  SmallVector<CallInst*> sites;
  for(auto& f : m)
    for(auto& block : f)
      for(auto& i : block)
        if(marker(&i, "ponyint_frame_push") || marker(&i, "ponyint_frame_poll"))
          sites.push_back(cast<CallInst>(&i));

  for(auto* site : sites)
  {
    IRBuilder<> b(site);
    if(marker(site, "ponyint_frame_push"))
    {
      auto* call = wrappedCall(site);
      if(!call)
        continue;
      retainNativeFrame(*site->getFunction());
      retainNativeFrame(*call->getCalledFunction());
      call->setTailCallKind(CallInst::TCK_NoTail);
      if(maps)
      {
        Value* slot = site->getArgOperand(2);
        emitMap(b, 1, slot);
        b.SetInsertPoint(call->getNextNode());
        auto* permit = dyn_cast<ConstantInt>(site->getArgOperand(1));
        emitMap(b, permit && permit->isMinusOne() ? 3 : 2, slot);
      }
    }
    else if(check)
    {
      retainNativeFrame(*site->getFunction());
      auto* actor = site->getArgOperand(0);
      auto due = m.getOrInsertFunction("ponyint_threshold_due",
        b.getInt32Ty(), actor->getType());
      auto* result = b.CreateCall(due, {actor});
      if(maps)
      {
        auto* taken = SplitBlockAndInsertIfThen(
          b.CreateICmpNE(result, b.getInt32(0)), site, false);
        b.SetInsertPoint(taken);
        auto slow = m.getOrInsertFunction("ponyint_mapped_collect",
          b.getVoidTy(), actor->getType());
        b.CreateCall(slow, {actor})->setTailCallKind(CallInst::TCK_NoTail);
        emitMap(b, 4);
      }
    }
  }
  discardMarkers(m);
}
