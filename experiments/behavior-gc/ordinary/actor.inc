#ifdef ORDINARY_OBSERVE
#include <stdio.h>
extern size_t ordinary_capacity(heap_t*);
extern _Thread_local heap_t* ordinary_watched;
extern _Thread_local size_t ordinary_reused;
#endif

void ponyint_ordinary_poll(pony_ctx_t* ctx, pony_actor_t* actor, uint32_t site)
{
  if(ctx != pony_ctx() || ctx->current != actor)
    return;
#ifdef ORDINARY_OBSERVE
  size_t before = actor->heap.used;
  size_t threshold = actor->heap.next_gc;
  size_t capacity = ordinary_capacity(&actor->heap);
  ordinary_watched = &actor->heap;
  ordinary_completed = 0;
#else
  (void)site;
#endif
  bool due = actor->heap.used > actor->heap.next_gc;
  bool eligible = due && !ctx->stack && !ctx->msg_target &&
    !ponyint_actormap_size(&ctx->acquire) &&
    !ponyint_objectmap_size(&actor->gc.local) &&
    !ponyint_actormap_size(&actor->gc.foreign) &&
    !ponyint_heap_has_finalisers(&actor->heap);
  if(eligible)
  {
#ifdef ORDINARY_OBSERVE
    if(getenv("ORDINARY_SAMPLE_ONLY") == NULL)
    {
      ordinary_collecting = true;
      try_gc(ctx, actor);
      ordinary_collecting = false;
    }
#else
    try_gc(ctx, actor);
#endif
  }
#ifdef ORDINARY_OBSERVE
  fprintf(stderr, "poll %p %u %zu %zu %d %d %zu %zu %zu %zu\n",
    (void*)actor, site, before, threshold, eligible, ordinary_completed,
    actor->heap.used, capacity, ordinary_capacity(&actor->heap), ordinary_reused);
  ordinary_reused = 0;
#endif
}
