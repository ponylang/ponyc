uint32_t ponyint_threshold_due(pony_actor_t* actor)
{
  return actor->heap.used > actor->heap.next_gc;
}

void ponyint_threshold_collect(pony_actor_t* actor, void* outer, void* inner)
{
  pony_ctx_t* ctx = pony_ctx();
  if(ctx->current != actor || ctx->stack || ctx->msg_target ||
    ponyint_actormap_size(&ctx->acquire) ||
    ponyint_objectmap_size(&actor->gc.local) ||
    ponyint_actormap_size(&actor->gc.foreign) ||
    ponyint_heap_has_finalisers(&actor->heap))
    return;

  void* roots[] = {outer, inner};
  for(size_t i = 0; i < 2; i++)
  {
    pony_actor_t* owner = NULL;
    if(ponyint_pagemap_get(roots[i], &owner) == NULL || owner != actor)
      return;
  }
  frame_collect(ctx, actor, roots, 2);
}
