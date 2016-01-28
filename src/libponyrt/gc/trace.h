#ifndef gc_trace_h
#define gc_trace_h

#include <pony.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

void pony_gc_mark(pony_ctx_t* ctx);

void pony_mark_done(pony_ctx_t* ctx);

PONY_EXTERN_C_END

#endif
