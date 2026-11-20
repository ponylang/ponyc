#include "../mem/pagemap.h"
#include <stdint.h>
#include <stdlib.h>

enum { FRAME_LIMIT = 16 };
static _Thread_local struct
{
  pony_actor_t* actor;
  void* roots[FRAME_LIMIT];
  const char* pending;
  uint32_t depth;
} frames;

uint32_t ponyint_frame_claim(pony_actor_t* actor, const char* method)
{
  if(frames.pending == NULL || actor != frames.actor || strcmp(frames.pending, method))
    return 0;
  frames.pending = NULL;
  return frames.depth;
}

uint32_t ponyint_frame_push(pony_actor_t* actor, uint32_t permit, void* root,
  const char* target)
{
  if(actor != pony_ctx()->current || frames.pending != NULL ||
    frames.depth == FRAME_LIMIT || permit == 0)
    return 0;
  if(permit == UINT32_MAX ? frames.depth != 0 :
    (frames.depth != permit || frames.actor != actor))
    return 0;
  pony_actor_t* owner = NULL;
  if(ponyint_pagemap_get(root, &owner) == NULL || owner != actor)
    return 0;
  frames.actor = actor;
  frames.pending = target;
  frames.roots[frames.depth++] = root;
  return frames.depth;
}

uint32_t ponyint_frame_pop(uint32_t ticket)
{
  if(ticket != 0)
  {
    if(ticket != frames.depth) abort();
    frames.pending = NULL;
    frames.roots[--frames.depth] = NULL;
    if(frames.depth == 0) frames.actor = NULL;
  }
  return 0;
}

uint32_t ponyint_frame_poll(pony_actor_t* actor, uint32_t permit)
{
  pony_ctx_t* ctx = pony_ctx();
  if(permit == 0 || permit != frames.depth || actor != frames.actor ||
    ctx->current != actor || frames.pending != NULL || actor->heap.used <= actor->heap.next_gc ||
    ctx->stack || ctx->msg_target || ponyint_actormap_size(&ctx->acquire) ||
    ponyint_objectmap_size(&actor->gc.local) || ponyint_actormap_size(&actor->gc.foreign) ||
    ponyint_heap_has_finalisers(&actor->heap))
    return 0;
  return frame_collect(ctx, actor, frames.roots, frames.depth);
}
