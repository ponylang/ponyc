#ifndef GC_FRAMES_H
#define GC_FRAMES_H

#include "marklocal.h"

PONY_EXTERN_C_BEGIN

void ponyint_gc_maps_init(void);
void ponyint_gc_maps_destroy(void);
bool ponyint_gc_trace_frames(pony_ctx_t* ctx, gc_frame_t* frame);

PONY_EXTERN_C_END

#endif
