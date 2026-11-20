#include "root_audit.h"
#include "../actor/actor.h"
#include "../ds/hash.h"
#include "../lang/stdfd.h"
#include "../sched/scheduler.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef PLATFORM_IS_WINDOWS
#include <stdlib.h>
#include <winsock2.h>
#endif

PONY_EXTERN_C_BEGIN

static bool same_root(void* a, void* b)
{
  return *(void* const*)a == *(void* const*)b;
}

PONY_API void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* receiver,
  const char* method, uint32_t line, uint32_t column, const char* reason,
  size_t count, void* const* roots, const char* const* modes)
{
#ifdef PLATFORM_IS_WINDOWS
  DWORD last_error = GetLastError();
  int socket_error = WSAGetLastError();
  unsigned long dos_error;
  _get_doserrno(&dos_error);
#endif
  int saved_errno = errno;
  bool current = (receiver != NULL) && (ctx->current == receiver);
  bool finalisers = current && ponyint_heap_has_finalisers(&receiver->heap);
  bool ready = (reason == NULL) && current && !finalisers;

  ponyint_stderr_lock();
  fprintf(stderr, "gc-root-audit %s %" PRIu32 ":%" PRIu32 " %s",
    method, line, column, ready ? "frame-ready" : "refused");
  if(reason != NULL)
    fprintf(stderr, " %s", reason);
  if(!current)
    fputs(" receiver-mismatch", stderr);
  if(finalisers)
    fputs(" finalisers", stderr);

  hashmap_t slots = {0};
  for(size_t i = 0; i < count; i++)
  {
    size_t pos = HASHMAP_UNKNOWN;
    size_t hash = ponyint_hash_ptr(roots[i]);
    void* key = (void*)&roots[i];
    void* const* first =
      (void* const*)ponyint_hashmap_get(&slots, key, hash, same_root, &pos);
    if(first == NULL)
    {
      ponyint_hashmap_putindex(&slots, key, hash, same_root, pos);
      first = &roots[i];
    }
    fprintf(stderr, " %s=%zu", modes[i], (size_t)(first - roots));
  }
  ponyint_hashmap_destroy(&slots, NULL);
  fputc('\n', stderr);
  ponyint_stderr_unlock();
#ifdef PLATFORM_IS_WINDOWS
  _set_doserrno(dos_error);
#endif
  errno = saved_errno;
#ifdef PLATFORM_IS_WINDOWS
  WSASetLastError(socket_error);
  SetLastError(last_error);
#endif
}

PONY_EXTERN_C_END
