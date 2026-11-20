#ifndef gc_root_audit_h
#define gc_root_audit_h

#include "../pony.h"
#include <platform.h>

PONY_EXTERN_C_BEGIN

PONY_API void ponyint_gc_root_audit(pony_ctx_t* ctx, pony_actor_t* receiver,
  const char* method, uint32_t line, const char* reason, size_t count,
  void* const* roots, const char* const* modes);

PONY_EXTERN_C_END

#endif
