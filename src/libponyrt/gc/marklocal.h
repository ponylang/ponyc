#ifndef GC_MARKLOCAL_H
#define GC_MARKLOCAL_H

#include "../pony.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

typedef struct gc_frame_t
{
  struct gc_frame_t* parent;
  pony_trace_fn trace;
} gc_frame_t;

PONY_API void ponyint_gc_native_store(void* address);
PONY_API void ponyint_gc_native_alias(void);
PONY_API bool ponyint_gc_has_small_room(pony_actor_t* actor);
PONY_API pony_ctx_t* ponyint_gc_begin(pony_ctx_t* ctx, pony_actor_t* actor,
  gc_frame_t* frame);
PONY_API pony_ctx_t* ponyint_gc_poll(pony_actor_t* actor, gc_frame_t* frame);
PONY_API bool ponyint_gc_finish(pony_ctx_t* trace_ctx);

PONY_EXTERN_C_END

#endif
