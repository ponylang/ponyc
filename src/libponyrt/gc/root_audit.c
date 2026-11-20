#include "root_audit.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include <inttypes.h>
#include <stdio.h>

PONY_EXTERN_C_BEGIN

PONY_API void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* receiver,
  const char* method, uint32_t line, const char* reason, size_t count,
  void* const* roots, const char* const* modes)
{
  bool current = (receiver != NULL) && (ctx->current == receiver);
  bool finalisers = current && ponyint_heap_has_finalisers(&receiver->heap);
  bool ready = (reason == NULL) && current && !finalisers;

#ifdef PLATFORM_IS_WINDOWS
  _lock_file(stderr);
#else
  flockfile(stderr);
#endif
  fprintf(stderr, "gc-root-audit %s %" PRIu32 " %s", method, line,
    ready ? "frame-ready" : "refused");
  if(reason != NULL)
    fprintf(stderr, " %s", reason);
  if(!current)
    fputs(" receiver-mismatch", stderr);
  if(finalisers)
    fputs(" finalisers", stderr);

  for(size_t i = 0; i < count; i++)
  {
    size_t first = 0;
    while(roots[first] != roots[i])
      first++;
    fprintf(stderr, " %s=%zu", modes[i], first);
  }
  fputc('\n', stderr);
#ifdef PLATFORM_IS_WINDOWS
  _unlock_file(stderr);
#else
  funlockfile(stderr);
#endif
}

PONY_EXTERN_C_END
